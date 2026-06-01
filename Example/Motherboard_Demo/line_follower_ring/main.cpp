#include "pid.hpp"
#include "schedule.hpp"
#include "zf_driver_encoder.h"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <execinfo.h>
#include <fcntl.h>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <pthread.h>
#include <thread>
#include <unistd.h>
#define KEY_0 "/dev/zf_driver_gpio_key_0"
#define KEY_1 "/dev/zf_driver_gpio_key_1"

#define MOTOR1_DIR "/dev/zf_driver_gpio_motor_1"

#define MOTOR2_DIR "/dev/zf_driver_gpio_motor_2"

#define ENCODER_1 "/dev/zf_encoder_1"
#define ENCODER_2 "/dev/zf_encoder_2"

#include "find_line_lib/calculate_wheel_speeds.h"
#include "pwm.hpp"

// ==========================================
// 配置参数
// ==========================================
const int FRAME_WIDTH = 160;
const int FRAME_HEIGHT = 120;


// ==========================================
// PID参数和全局变量
// ==========================================
float g_left_kp = 1.83f, g_left_ki = 0.73f, g_left_kd = 0.0f;
float g_right_kp = 2.30f, g_right_ki = 1.39f, g_right_kd = 0.0f;

volatile float g_left_target_speed = 0.0f, g_right_target_speed = 0.0f;

volatile sig_atomic_t g_running = 1;

// 持久化 fd API：两个 fd 存放在 std::atomic<int> 中，避免依赖 zf 库
static std::atomic<int> gpio_fd1{-1};
static std::atomic<int> gpio_fd2{-1};

// 打开并保存 fd 到 atomic 中，若已有 fd 则关闭新打开的 fd
static bool gpio_open_persistent(std::atomic<int> &afd, const char *path) {
  int fd = open(path, O_WRONLY | O_CLOEXEC);
  if (fd == -1) {
    // fprintf(stderr, "open(%s) failed: %s\n", path, strerror(errno));
    return false;
  }

  int expected = -1;
  if (!afd.compare_exchange_strong(expected, fd)) {
    // already had a fd, close the one we just opened
    close(fd);
  }
  return true;
}

// 写入 '0'/'1' 到已保存的 fd
static bool gpio_set_level_persistent(std::atomic<int> &afd, int level) {
  int fd = afd.load();
  if (fd == -1)
    return false;
  const char buf = (level ? '1' : '0');
  lseek(fd, 0, SEEK_SET);
  ssize_t n = write(fd, &buf, 1);
  if (n != 1) {
    // fprintf(stderr, "write failed on fd %d: %s\n", fd, strerror(errno));
    return false;
  }
  return true;
}

// 关闭并清除 atomic 中的 fd
static void gpio_close_persistent(std::atomic<int> &afd) {
  int fd = afd.exchange(-1);
  if (fd != -1)
    close(fd);
}

void signal_handler(int sig) {

  g_running = 0;

  gpio_set_level_persistent(gpio_fd1, 0);
  gpio_set_level_persistent(gpio_fd2, 0);
  switch (sig) {

    // void *array[32];
    // size_t size;
  case SIGINT:
    // std::cerr << "收到 SIGINT. Exiting safely." << std::endl;

    // 获取最近的 16 个调用栈帧
    // size = backtrace(array, 32);

    // std::cerr << "--- 调用栈 ---" << std::endl;
    // 将堆栈符号直接打印到标准错误输出 (stderr)
    // backtrace_symbols_fd(array, size, STDERR_FILENO);
    // std::cerr << "--------------------------------" << std::endl;
    exit(0);
  case SIGTERM:
    // std::cerr << "收到 SIGTERM. Exiting safely." << std::endl;

    // size = backtrace(array, 32);

    // std::cerr << "--- 调用栈 ---" << std::endl;
    // // 将堆栈符号直接打印到标准错误输出 (stderr)
    // backtrace_symbols_fd(array, size, STDERR_FILENO);
    // std::cerr << "--------------------------------" << std::endl;
    exit(0);
  case SIGSEGV:
    // std::cerr << "段错误. Exiting safely." << std::endl;
    // ===================================
    std::abort();
  }
}

float left_kp_func(float error) { return error * g_left_kp; }
float right_kp_func(float error) { return error * g_right_kp; }

float convert_to_pwm_output(float value) { return (value + 5000.0) / 100.0; }

static constexpr TaskConfig myConfigs[] = {
    {100,
     []() {
       static PID left_pid(100, left_kp_func, g_left_ki, g_left_kd, -1000,
                           1000);
       static PID right_pid(100, right_kp_func, g_right_ki, g_right_kd, -1000,
                            1000);

       static PWM left(0, 0);
       static PWM right(1, 0);
       static bool initialized = false;
       if (initialized == false) {
         std::cout << "正在初始化 PWM 设备..." << std::endl;

         // 2. 配置：频率 2kHz (2000Hz)，占空比 50%
         std::cout << "设置频率 17000Hz, 占空比 50%(速度为0)..." << std::endl;
         if (!left.config(17000, 50)) {
           std::cerr << "配置 PWM0 失败！" << std::endl;
           return;
         }
         if (!right.config(17000, 50)) {
           std::cerr << "配置 PWM1 失败！" << std::endl;
           return;
         }

         std::cout << "PWM 设备初始化成功！" << std::endl;
         std::cout << "正在打开 GPIO 设备..." << std::endl;
         gpio_open_persistent(gpio_fd1, MOTOR1_DIR);
         gpio_open_persistent(gpio_fd2, MOTOR2_DIR);

         if (gpio_fd1.load() == -1 || gpio_fd2.load() == -1) {
           std::cerr << "打开 GPIO 设备失败，请检查驱动或设备节点" << std::endl;
           return;
         }
         std::cout << "GPIO 设备打开成功！" << std::endl;

         std::cout << "设置 GPIO 输出高电平..." << std::endl;
         gpio_set_level_persistent(gpio_fd1, 1);
         gpio_set_level_persistent(gpio_fd2, 1);
         std::cout << "GPIO 输出设置成功！" << std::endl;
         // 3. 开启输出
         std::cout << "开启 PWM 输出..." << std::endl;
         left.enable();
         right.enable();
         std::cout << "开启 PWM 输出成功！" << std::endl;

         std::cout << "设置 PID 积分限幅..." << std::endl;
         left_pid.set_integral_limit(2000, -2000);
         right_pid.set_integral_limit(2000, -2000);
         std::cout << "PID 积分限幅设置成功！" << std::endl;

         // 前向/反向死区补偿
         std::cout << "设置 PID 死区补偿..." << std::endl;
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
    //    std::cout << "获取编码器计数..." << std::endl;
       int16_t lc = encoder_get_count(ENCODER_1);
       int16_t rc = encoder_get_count(ENCODER_2);
    //    std::cout << "编码器计数获取成功！ Left: " << lc << " Right: " << rc
    //              << std::endl;

    //    std::cout << "设置 PID 目标速度..." << std::endl;
       left_pid.set_point(g_left_target_speed);
       right_pid.set_point(g_right_target_speed);
    //    std::cout << "PID 目标速度设置成功！ Left: " << g_left_target_speed
    //              << " Right: " << g_right_target_speed << std::endl;

       {
        //  std::cout << "输入反馈到 PID..." << std::endl;
         left_pid.input_feedback((float)-lc);
         right_pid.input_feedback((float)rc);
        //  std::cout << "PID 输入反馈成功！ Left: " << -lc << " Right:" << rc
        //            << std::endl;

        //  std::cout << "计算 PID 输出..." << std::endl;
         float left_output = left_pid.output();
         float right_output = right_pid.output();
        //  std::cout << "PID 输出计算成功！ Left: " << left_output
        //            << " Right: " << right_output << std::endl;

        //  std::cout << "设置 PWM 占空比..." << std::endl;
         left.set_duty_cycle(convert_to_pwm_output(left_output));
         right.set_duty_cycle(convert_to_pwm_output(right_output));
        //  std::cout << "PWM 占空比设置成功！ Left: "
        //            << convert_to_pwm_output(left_output)
        //            << "% Right: " << convert_to_pwm_output(right_output) << "%"
        //            << std::endl;
         //     // printf("set_point: left=%.1f right=%.1f | pid_out: "
         //     //        "left=%.2f right=%.2f\r",
         //     //        g_left_target_speed, g_right_target_speed,
         //     left_output,
         //     //        right_output);
         //     // fflush(stdout);
       }
       
     }},
    {100, []() {
       static cv::VideoCapture cap(0);
       static cv::Mat frame;
       static bool initialized = false;
       if (!initialized) {
         if (!cap.isOpened()) {
           std::cerr << "Error: Could not open camera." << std::endl;
           return;
         }
         cap.set(cv::CAP_PROP_FRAME_WIDTH, FRAME_WIDTH);
         cap.set(cv::CAP_PROP_FRAME_HEIGHT, FRAME_HEIGHT);
         cap.set(cv::CAP_PROP_FPS, 180);

         initialized = true;
       }
       cap >> frame;
       if (frame.empty())
         return;
       auto [left_speed, right_speed] =
           find_line_lib::calculate_wheel_speeds(frame, 20.0,0.2  );
       std::cout << " | Target Speed: L=" << left_speed << " R=" << right_speed
                 << "    \r" << std::flush;
       g_left_target_speed = left_speed;
       g_right_target_speed = right_speed;
     }}};
static auto get_time() {
  return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count());
}
using schedule = StaticTimerManager<get_time, myConfigs, std::size(myConfigs)>;

int main(int argc, char **argv) {
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);
  signal(SIGSEGV, signal_handler);

  while (g_running) {
    schedule::poll();
  }

  gpio_set_level_persistent(gpio_fd1, 0);
  gpio_set_level_persistent(gpio_fd2, 0);
  gpio_close_persistent(gpio_fd1);
  gpio_close_persistent(gpio_fd2);

  std::cout << std::endl << "Exiting..." << std::endl;
  return 0;
}
