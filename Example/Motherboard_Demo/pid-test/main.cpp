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
#include <pthread.h>
#include <unistd.h>

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

char g_recv_buf[256] = {0};
int g_recv_ready = 0;
int g_sockfd = -1;

void* recv_thread(void*) {
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

float pid_kp_func(float error) { return error * g_kp; }

int main(int argc, char **argv) {
  setbuf(stdout, NULL);
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);

  PID left(100, pid_kp_func, 0, 0, -10000, 10000);
  PID right(100, pid_kp_func, 0, 0, -10000, 10000);

  g_sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (g_sockfd < 0) {
    perror("socket failed");
    return 1;
  }

  sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(1347);
  server_addr.sin_addr.s_addr = inet_addr("192.168.1.123");

  printf("Connecting to 192.168.1.123:1347...\n");
  if (connect(g_sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
    perror("connect failed");
    close(g_sockfd);
    return 1;
  }
  printf("TCP connected\n");
  fflush(stdout);

  pthread_t tid;
  pthread_create(&tid, NULL, recv_thread, NULL);

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
        if (strcmp(key, "kp") == 0) { g_kp = value; printf("set kp=%.2f\n", (double)value); }
        else if (strcmp(key, "ki") == 0) { g_ki = value; printf("set ki=%.2f\n", (double)value); }
        else if (strcmp(key, "kd") == 0) { g_kd = value; printf("set kd=%.2f\n", (double)value); }
        else if (strcmp(key, "left_speed") == 0) { g_left_speed = value; printf("set left_speed=%.2f\n", (double)value); }
        else if (strcmp(key, "right_speed") == 0) { g_right_speed = value; printf("set right_speed=%.2f\n", (double)value); }
        else if (strcmp(key, "left_duty") == 0) { g_left_duty = value; printf("set left_duty=%.2f\n", (double)value); }
        else if (strcmp(key, "right_duty") == 0) { g_right_duty = value; printf("set right_duty=%.2f\n", (double)value); }
      }
      fflush(stdout);
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
        printf("lc:%d rc:%d lo:%.1f ro:%.1f\r", lc, rc, (double)lo, (double)ro);
      }
    }

    usleep(10000);
  }

  close(g_sockfd);
  return 0;
}