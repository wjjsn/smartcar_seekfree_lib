#include "pid.hpp"
#include "zf_common_headfile.h"
#include "zf_driver_gpio.h"
#include "zf_driver_pwm.h"
#include <arpa/inet.h>
#include <csignal>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <sstream>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define MOTOR1_DIR "/dev/zf_driver_gpio_motor_1"
#define MOTOR1_PWM "/dev/zf_device_pwm_motor_1"
#define MOTOR2_DIR "/dev/zf_driver_gpio_motor_2"
#define MOTOR2_PWM "/dev/zf_device_pwm_motor_2"
#define ENCODER_1 "/dev/zf_encoder_1"
#define ENCODER_2 "/dev/zf_encoder_2"

volatile sig_atomic_t g_running = 1;
void signal_handler(int) {
  pwm_set_duty(MOTOR1_PWM, 5000);
  pwm_set_duty(MOTOR2_PWM, 5000);
  g_running = 0;
}

float g_left_kp = 1.0f, g_left_ki = 0.0f, g_left_kd = 0.0f;
float g_right_kp = 1.0f, g_right_ki = 0.0f, g_right_kd = 0.0f;
float g_left_speed = 100.0f, g_right_speed = 100.0f;
float g_left_duty = 0.0f, g_right_duty = 0.0f;

char g_recv_buf[256] = {0};
int g_recv_ready = 0;
int g_sockfd = -1;

void *recv_thread(void *) {
  while (g_running) {
    if (g_sockfd >= 0) {
      char buf[256] = {0};
      ssize_t len = recv(g_sockfd, buf, sizeof(buf) - 1, 0);
      if (len > 0) {
        memcpy(g_recv_buf, buf, sizeof(g_recv_buf) - 1);
        g_recv_ready = 1;
      } else if (len < 0) {
        break;
      } else {
        sleep(1);
      }
    }
  }
  return NULL;
}

//神秘电机PWM大于5000时正转，越大越快；小于5000时反转，越小越快，等于5000时停止
int convert_to_output(int value) {
  return value + 5000; // 将输入值转换为电机PWM输出值
}

float left_kp_func(float error) { return error * g_left_kp; }
float right_kp_func(float error) { return error * g_right_kp; }

int main(int argc, char **argv) {
  if (argc != 2) {
    printf("need arg:ip_addr");
    return -1;
  }
  setbuf(stdout, NULL);
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);
  signal(SIGSEGV, signal_handler);

  PID left(100, left_kp_func, 0, 0, -5000, 5000);
  PID right(100, right_kp_func, 0, 0, -5000, 5000);
  left.set_integral_limit(23333, -23333);
  right.set_integral_limit(23333, -23333);

  g_sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (g_sockfd < 0) {
    perror("socket failed");
    return 1;
  }

  sockaddr_in server_addr;
  std::string_view ip_addr = argv[1];
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(1347);
  server_addr.sin_addr.s_addr = inet_addr(ip_addr.data());

  printf("Connecting to %s:1347...\n", ip_addr.data());
  if (connect(g_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) <
      0) {
    perror("connect failed");
    close(g_sockfd);
    return 1;
  }
  printf("TCP connected\n");
  fflush(stdout);

  pthread_t tid;
  pthread_create(&tid, NULL, recv_thread, NULL);

  gpio_set_level(MOTOR1_DIR, 1);
  gpio_set_level(MOTOR2_DIR, 1);
  int loop_count = 0;
  while (g_running) {
    loop_count++;

    if (g_recv_ready) {
      g_recv_ready = 0;
      char buf[256] = {0};
      memcpy(buf, g_recv_buf, sizeof(g_recv_buf) - 1);
      printf("recv_count:%d buf:%s\n", (int)strlen(buf), buf);
      fflush(stdout);
      char key[32] = {0};
      float value = 0;
      if (sscanf(buf, "%31[^:]:%f", key, &value) == 2) {
        if (strcmp(key, "left_kp") == 0) {
          g_left_kp = value;
          printf("set left_kp=%.2f\n", (double)value);
        } else if (strcmp(key, "left_ki") == 0) {
          g_left_ki = value;
          printf("set left_ki=%.2f\n", (double)value);
        } else if (strcmp(key, "left_kd") == 0) {
          g_left_kd = value;
          printf("set left_kd=%.2f\n", (double)value);
        } else if (strcmp(key, "right_kp") == 0) {
          g_right_kp = value;
          printf("set right_kp=%.2f\n", (double)value);
        } else if (strcmp(key, "right_ki") == 0) {
          g_right_ki = value;
          printf("set right_ki=%.2f\n", (double)value);
        } else if (strcmp(key, "right_kd") == 0) {
          g_right_kd = value;
          printf("set right_kd=%.2f\n", (double)value);
        } else if (strcmp(key, "left_speed") == 0) {
          g_left_speed = value;
          printf("set left_speed=%.2f\n", (double)value);
        } else if (strcmp(key, "right_speed") == 0) {
          g_right_speed = value;
          printf("set right_speed=%.2f\n", (double)value);
        } else if (strcmp(key, "left_duty") == 0) {
          g_left_duty = value;
          printf("set left_duty=%.2f\n", (double)value);
        } else if (strcmp(key, "right_duty") == 0) {
          g_right_duty = value;
          printf("set right_duty=%.2f\n", (double)value);
        }
      }
      fflush(stdout);
    }

    int16_t lc = encoder_get_count(ENCODER_1);
    int16_t rc = encoder_get_count(ENCODER_2);

    left.set_point(g_left_speed);
    right.set_point(g_right_speed);
    left.set_pid(left_kp_func, g_left_ki, g_left_kd);
    right.set_pid(right_kp_func, g_right_ki, g_right_kd);
    printf("set_point: left=%.1f right=%.1f | duty: "
           "left=%.2f right=%.2f\r",
           g_left_speed, g_right_speed, g_left_duty,
           g_right_duty);

    if (g_left_duty != 0 || g_right_duty != 0) {
      pwm_set_duty(MOTOR1_PWM, convert_to_output((int)g_left_duty));
      pwm_set_duty(MOTOR2_PWM, convert_to_output((int)g_right_duty));
    } else {
      left.input_feedback((float)-lc);
      right.input_feedback((float)rc);

      float lo = left.output();
      float ro = right.output();

      pwm_set_duty(MOTOR1_PWM, convert_to_output((int)lo));
      pwm_set_duty(MOTOR2_PWM, convert_to_output((int)ro));

      auto left_debug = left.debug_output();
      auto right_debug = right.debug_output();
      std::ostringstream debug_stream;
      debug_stream << left_debug.str() << ',' << right_debug.str() << '\n'
                   << std::ends;
      send(g_sockfd, debug_stream.str().c_str(),
           strlen(debug_stream.str().c_str()), 0);
    }

    usleep(10000);
  }

  close(g_sockfd);
  return 0;
}