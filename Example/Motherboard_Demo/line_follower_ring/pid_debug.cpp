#include "pid.hpp"
#include "pwm.hpp"
#include "schedule.hpp"
#include "zf_driver_encoder.h"
#include <arpa/inet.h>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

// ==========================================
// 设备路径定义
// ==========================================
#define MOTOR1_DIR "/dev/zf_driver_gpio_motor_1"
#define MOTOR2_DIR "/dev/zf_driver_gpio_motor_2"

#define ENCODER_1 "/dev/zf_encoder_1"
#define ENCODER_2 "/dev/zf_encoder_2"

// ==========================================
// PID 参数（全局，可通过 TCP 调参）
// ==========================================
float g_left_kp = .0f, g_left_ki = .0f, g_left_kd = .0f;
float g_right_kp = .0f, g_right_ki = .0f, g_right_kd = .0f;

// 目标速度（由视觉或其他模块设置）
volatile float g_left_target_speed = 0.0f, g_right_target_speed = 0.0f;
// 直接 PWM 覆盖，单位与旧上位机协议一致：[-5000,5000]
volatile float g_left_duty = 0.0f, g_right_duty = 0.0f;

// 运行标志
volatile sig_atomic_t g_running = 1;

// TCP 通信相关
int g_sockfd = -1;

// 持久化 fd
static std::atomic<int> gpio_fd1{-1};
static std::atomic<int> gpio_fd2{-1};

// ----------------------------------------------------------
// 工具函数：持久化 GPIO
// ----------------------------------------------------------
static bool gpio_open_persistent(std::atomic<int> &afd, const char *path) {
  int fd = open(path, O_WRONLY | O_CLOEXEC);
  if (fd == -1)
    return false;
  int expected = -1;
  if (!afd.compare_exchange_strong(expected, fd)) {
    close(fd);
  }
  return true;
}

static bool gpio_set_level_persistent(std::atomic<int> &afd, int level) {
  int fd = afd.load();
  if (fd == -1)
    return false;
  const char buf = (level ? '1' : '0');
  lseek(fd, 0, SEEK_SET);
  ssize_t n = write(fd, &buf, 1);
  return (n == 1);
}

static void gpio_close_persistent(std::atomic<int> &afd) {
  int fd = afd.exchange(-1);
  if (fd != -1)
    close(fd);
}

// ----------------------------------------------------------
// 信号处理：安全退出
// ----------------------------------------------------------
void signal_handler(int sig) {
  g_running = 0;
  if (sig == SIGINT || sig == SIGTERM) {
    gpio_set_level_persistent(gpio_fd1, 0);
    gpio_set_level_persistent(gpio_fd2, 0);
    exit(0);
  }
  if (sig == SIGSEGV) {
    gpio_set_level_persistent(gpio_fd1, 0);
    gpio_set_level_persistent(gpio_fd2, 0);
    std::_Exit(128 + sig);
  }
}

// ----------------------------------------------------------
// 解析并应用调参命令
// ----------------------------------------------------------
void apply_tuning_command(const char *buf) {
  char key[32] = {0};
  float value = 0;
  if (sscanf(buf, "%31[^:]:%f", key, &value) == 2) {
    if (strcmp(key, "left_kp") == 0) {
      g_left_kp = value;
    } else if (strcmp(key, "left_ki") == 0) {
      g_left_ki = value;
    } else if (strcmp(key, "left_kd") == 0) {
      g_left_kd = value;
    } else if (strcmp(key, "right_kp") == 0) {
      g_right_kp = value;
    } else if (strcmp(key, "right_ki") == 0) {
      g_right_ki = value;
    } else if (strcmp(key, "right_kd") == 0) {
      g_right_kd = value;
    } else if (strcmp(key, "left_speed") == 0) {
      g_left_target_speed = value;
    } else if (strcmp(key, "right_speed") == 0) {
      g_right_target_speed = value;
    } else if (strcmp(key, "left_duty") == 0) {
      g_left_duty = value;
    } else if (strcmp(key, "right_duty") == 0) {
      g_right_duty = value;
    }
  }
}

// ----------------------------------------------------------
// PID 计算函数（使用全局参数）
// ----------------------------------------------------------
float left_kp_func(float error) { return error * g_left_kp; }
float right_kp_func(float error) { return error * g_right_kp; }

// ----------------------------------------------------------
// 调度任务配置
// ----------------------------------------------------------
// 100ms 任务：读取编码器 + PID 计算 + PWM 输出
static constexpr TaskConfig pidConfigs[] = {
    {100, []() {
       static PID left_pid(10, left_kp_func, g_left_ki, g_left_kd, -5000, 5000);
       static PID right_pid(10, right_kp_func, g_right_ki, g_right_kd, -5000,
                            5000);
       static PWM left(0, 0);
       static PWM right(1, 0);
       static bool initialized = false;

       if (!initialized) {
         std::cout << "[PID] 初始化 PWM..." << std::endl;
         if (!left.config(17000, 50)) {
           std::cerr << "[PID] 配置 PWM0 失败！" << std::endl;
           return;
         }
         if (!right.config(17000, 50)) {
           std::cerr << "[PID] 配置 PWM1 失败！" << std::endl;
           return;
         }

         std::cout << "[PID] 打开 GPIO 方向控制..." << std::endl;
         gpio_open_persistent(gpio_fd1, MOTOR1_DIR);
         gpio_open_persistent(gpio_fd2, MOTOR2_DIR);
         if (gpio_fd1.load() == -1 || gpio_fd2.load() == -1) {
           std::cerr << "[PID] 打开 GPIO 设备失败！" << std::endl;
           return;
         }

         std::cout << "[PID] 设置 GPIO 高电平（正转）..." << std::endl;
         gpio_set_level_persistent(gpio_fd1, 1);
         gpio_set_level_persistent(gpio_fd2, 1);

         left.enable();
         right.enable();

         left_pid.set_integral_limit(2000, -2000);
         right_pid.set_integral_limit(2000, -2000);

         // 前向/反向死区补偿
         left_pid.set_deadzone(439, -553.5);
         right_pid.set_deadzone(415, -600);
         std::cout << "[PID] 初始化完成！" << std::endl;
         initialized = true;
       }

       if (!g_running) {
         left.set_duty_cycle(50.0);
         right.set_duty_cycle(50.0);
         return;
       }

       // 非阻塞读取 TCP 数据（单线程用 poll）
       if (g_sockfd >= 0) {
         struct pollfd pfd = {.fd = g_sockfd, .events = POLLIN, .revents = 0};
         if (poll(&pfd, 1, 0) > 0) {
           char buf[256] = {0};
           ssize_t len = recv(g_sockfd, buf, sizeof(buf) - 1, 0);
           if (len > 0) {
             apply_tuning_command(buf);
             printf("[PID] left_kp=%.2f left_ki=%.2f | "
                    "right_kp=%.2f right_ki=%.2f | "
                    "speed: left=%.1f right=%.1f\r",
                    g_left_kp, g_left_ki, g_right_kp, g_right_ki,
                    g_left_target_speed, g_right_target_speed);
             fflush(stdout);
           } else if (len == 0) {
             g_running = 0;
           }
         }
       }

       // 直接 PWM 覆盖优先级
       if (g_left_duty != 0.0f || g_right_duty != 0.0f) {
         float left_direct = (g_left_duty + 5000.0f) / 100.0f;
         float right_direct = (g_right_duty + 5000.0f) / 100.0f;
         left.set_duty_cycle(left_direct);
         right.set_duty_cycle(right_direct);
         printf("[PID] direct_pwm: left=%.1f right=%.1f\r", g_left_duty,
                g_right_duty);
         fflush(stdout);
         return;
       }

       // 读取编码器
       int16_t lc = encoder_get_count(ENCODER_1);
       int16_t rc = encoder_get_count(ENCODER_2);

       // 设置 PID 目标值
       left_pid.set_point(g_left_target_speed);
       right_pid.set_point(g_right_target_speed);

       // 更新 PID 参数（如果被调了）
       left_pid.set_pid(left_kp_func, g_left_ki, g_left_kd);
       right_pid.set_pid(right_kp_func, g_right_ki, g_right_kd);

       // 反馈编码器计数（左手电机反转，所以取负）
       left_pid.input_feedback((float)-lc);
       right_pid.input_feedback((float)rc);

       // 计算 PID 输出
       float left_output = left_pid.output();
       float right_output = right_pid.output();

       // 转换为 PWM 占空比：PID 输出范围 [-5000,5000] -> [0%,100%]
       float left_duty = (left_output + 5000.0f) / 100.0f; // 0~100%
       float right_duty = (right_output + 5000.0f) / 100.0f;

       left.set_duty_cycle(left_duty);
       right.set_duty_cycle(right_duty);

       // 通过 TCP 上报调试信息
       if (g_sockfd >= 0) {
         auto left_debug = left_pid.debug_output();
         auto right_debug = right_pid.debug_output();
         std::ostringstream oss;
         oss << left_debug.str() << ',' << right_debug.str() << '\n';
         send(g_sockfd, oss.str().c_str(), oss.str().size(), 0);
       }
     }}};

static auto get_time() -> uint64_t {
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  return static_cast<uint64_t>(tv.tv_sec) * 1000 + tv.tv_usec / 1000;
}
using schedule =
    StaticTimerManager<get_time, pidConfigs, std::size(pidConfigs)>;

// ----------------------------------------------------------
// 主函数
// ----------------------------------------------------------
int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "用法: " << argv[0] << " <服务器IP>" << std::endl;
    return -1;
  }
  setbuf(stdout, NULL);

  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);
  signal(SIGSEGV, signal_handler);

  // 建立 TCP 连接
  g_sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (g_sockfd < 0) {
    perror("[PID] socket 失败");
    return 1;
  }

  sockaddr_in server_addr;
  std::string_view ip_addr = argv[1];
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(1347);
  server_addr.sin_addr.s_addr = inet_addr(ip_addr.data());

  std::cout << "[PID] 连接到 " << ip_addr << ":1347 ..." << std::endl;
  if (connect(g_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) <
      0) {
    perror("[PID] connect 失败");
    close(g_sockfd);
    return 1;
  }
  std::cout << "[PID] TCP 已连接" << std::endl;

  // 主循环
  while (g_running) {
    schedule::poll();
  }

  // 清理
  close(g_sockfd);
  gpio_set_level_persistent(gpio_fd1, 0);
  gpio_set_level_persistent(gpio_fd2, 0);
  gpio_close_persistent(gpio_fd1);
  gpio_close_persistent(gpio_fd2);

  std::cout << std::endl << "[PID] 退出." << std::endl;
  return 0;
}
