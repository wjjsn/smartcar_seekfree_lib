#include "zf_common_headfile.h"
#include "zf_driver_gpio.h"
#include "zf_driver_pwm.h"
#include "pid.hpp"
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <poll.h>

#define MOTOR1_DIR "/dev/zf_driver_gpio_motor_1"
#define MOTOR1_PWM "/dev/zf_device_pwm_motor_1"
#define MOTOR2_DIR "/dev/zf_driver_gpio_motor_2"
#define MOTOR2_PWM "/dev/zf_device_pwm_motor_2"
#define ENCODER_1 "/dev/zf_encoder_1"
#define ENCODER_2 "/dev/zf_encoder_2"

volatile sig_atomic_t g_running = 1;
void signal_handler(int) { g_running = 0; }

float g_kp = 1.0f, g_ki = 0.0f, g_kd = 0.0f;
float g_left_speed = 100.0f, g_right_speed = 100.0f;
float g_left_duty = 0.0f, g_right_duty = 0.0f;

float pid_kp_func(float error) { return error * g_kp; }

int main(int argc, char **argv) {
  setbuf(stdout, NULL);
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);

  PID left(100, pid_kp_func, 0, 0, -10000, 10000);
  PID right(100, pid_kp_func, 0, 0, -10000, 10000);

  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  int flags = fcntl(sockfd, F_GETFL, 0);
  fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

  sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(1347);
  server_addr.sin_addr.s_addr = inet_addr("192.168.1.123");

  connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
  printf("TCP connect called\n");
  fflush(stdout);

  int loop_count = 0;
  while (g_running) {
    loop_count++;

    struct pollfd pfd = {sockfd, POLLIN, 0};
    if (poll(&pfd, 1, 0) > 0 && (pfd.revents & POLLIN)) {
      char buf[256] = {0};
      ssize_t len = recv(sockfd, buf, sizeof(buf) - 1, 0);
      if (len > 0) {
        char key[32] = {0};
        int value = 0;
        if (sscanf(buf, "%31[^:]:%d", key, &value) == 2) {
          if (strcmp(key, "kp") == 0) { g_kp = value; printf("set kp=%d\n", value); fflush(stdout); }
          else if (strcmp(key, "ki") == 0) { g_ki = value; printf("set ki=%d\n", value); fflush(stdout); }
          else if (strcmp(key, "kd") == 0) { g_kd = value; printf("set kd=%d\n", value); fflush(stdout); }
          else if (strcmp(key, "left_speed") == 0) { g_left_speed = value; printf("set left_speed=%d\n", value); fflush(stdout); }
          else if (strcmp(key, "right_speed") == 0) { g_right_speed = value; printf("set right_speed=%d\n", value); fflush(stdout); }
          else if (strcmp(key, "left_duty") == 0) { g_left_duty = value; printf("set left_duty=%d\n", value); fflush(stdout); }
          else if (strcmp(key, "right_duty") == 0) { g_right_duty = value; printf("set right_duty=%d\n", value); fflush(stdout); }
        }
      }
    }

    int16_t lc = encoder_get_count(ENCODER_1);
    int16_t rc = encoder_get_count(ENCODER_2);

    left.set_point(g_left_speed);
    right.set_point(g_right_speed);
    left.set_pid(pid_kp_func, g_ki, g_kd);
    right.set_pid(pid_kp_func, g_ki, g_kd);

    if (g_left_duty != 0 || g_right_duty != 0) {
      gpio_set_level(MOTOR1_DIR, g_left_duty > 0 ? 1 : 0);
      gpio_set_level(MOTOR2_DIR, g_right_duty > 0 ? 1 : 0);
      pwm_set_duty(MOTOR1_PWM, (int)abs(g_left_duty));
      pwm_set_duty(MOTOR2_PWM, (int)abs(g_right_duty));
    } else {
      left.input_feedback((float)lc);
      right.input_feedback((float)rc);

      float lo = left.output();
      float ro = right.output();

      gpio_set_level(MOTOR1_DIR, lo > 0 ? 1 : 0);
      gpio_set_level(MOTOR2_DIR, ro > 0 ? 1 : 0);
      pwm_set_duty(MOTOR1_PWM, (int)abs(lo));
      pwm_set_duty(MOTOR2_PWM, (int)abs(ro));

      if (loop_count % 100 == 0) {
        printf("lc:%d rc:%d lo:%.1f ro:%.1f\n", lc, rc, (double)lo, (double)ro);
      }
    }
  }

  close(sockfd);
  return 0;
}