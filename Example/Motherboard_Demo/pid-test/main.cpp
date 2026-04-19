#include "pid.hpp"
#include "zf_common_headfile.h"
#include "zf_driver_gpio.h"
#include "zf_driver_pwm.h"
#include "socket.hpp"
#include <thread>
#include <mutex>
#include <atomic>
#include <csignal>
using namespace std::chrono_literals;
#define KEY_0 "/dev/zf_driver_gpio_key_0"
#define KEY_1 "/dev/zf_driver_gpio_key_1"

#define MOTOR1_DIR "/dev/zf_driver_gpio_motor_1"
#define MOTOR1_PWM "/dev/zf_device_pwm_motor_1"

#define MOTOR2_DIR "/dev/zf_driver_gpio_motor_2"
#define MOTOR2_PWM "/dev/zf_device_pwm_motor_2"

#define ENCODER_1 "/dev/zf_encoder_1"
#define ENCODER_2 "/dev/zf_encoder_2"

std::mutex pid_mutex;
volatile std::atomic<bool> running{true};

std::atomic<float> left_duty{0}, right_duty{0};
std::atomic<float> left_speed{0}, right_speed{0};
std::atomic<float> kp{1}, ki{0}, kd{0};

float kp_func(float error) { return error * kp.load(); }
float ki_func(float error) { (void)error; return 0; }
float kd_func(float error) { (void)error; return 0; }

int main(int argc, char **argv) {
  signal(SIGSEGV, [](int) { running = false; });
  signal(SIGINT, [](int) { running = false; });

  PID left(100, +[](float error) { return (float)(error * 2.0f); }, 0, 0, 10000,
           -10000);
  PID right(100, +[](float error) { return (float)(error * 2.0f); }, 0, 0, 10000,
            -10000);

  int16 left_count = 0, right_count = 0;

  TcpSocket* tcp_socket = nullptr;
  try {
    tcp_socket = new TcpSocket();
    tcp_socket->connect_to_server("192.168.1.123", 1347);
  } catch (const std::exception& e) {
    std::cerr << "[Error] TCP connection failed: " << e.what() << std::endl;
    if (tcp_socket) delete tcp_socket;
    return 1;
  }

  std::thread tcp_reader([&]() {
    char recv_buffer[256] = {0};
    while (running) {
      if (!tcp_socket) break;
      ssize_t recv_len = tcp_socket->recv_data(recv_buffer, sizeof(recv_buffer) - 1);
      if (recv_len > 0) {
        recv_buffer[recv_len] = '\0';
        char key[32] = {0};
        int value = 0;
        if (sscanf(recv_buffer, "%31[^:]:%d", key, &value) == 2) {
          std::lock_guard<std::mutex> lock(pid_mutex);
          if (strcmp(key, "kp") == 0) {
            kp = value;
          } else if (strcmp(key, "ki") == 0) {
            ki = value;
          } else if (strcmp(key, "kd") == 0) {
            kd = value;
          } else if (strcmp(key, "left_speed") == 0) {
            left_speed = value;
          } else if (strcmp(key, "right_speed") == 0) {
            right_speed = value;
          } else if (strcmp(key, "left_duty") == 0) {
            left_duty = value;
          } else if (strcmp(key, "right_duty") == 0) {
            right_duty = value;
          }
        }
      }
      std::this_thread::sleep_for(10ms);
    }
  });

  while (running) {
    float current_kp, current_ki, current_kd;
    float current_left_speed, current_right_speed;
    float current_left_duty, current_right_duty;
    {
      std::lock_guard<std::mutex> lock(pid_mutex);
      current_kp = kp.load();
      current_ki = ki.load();
      current_kd = kd.load();
      current_left_speed = left_speed.load();
      current_right_speed = right_speed.load();
      current_left_duty = left_duty.load();
      current_right_duty = right_duty.load();
    }

    left.set_pid(+[](float error) { return error * kp.load(); }, current_ki, current_kd);
    right.set_pid(+[](float error) { return error * kp.load(); }, current_ki, current_kd);
    left.set_point(current_left_speed);
    right.set_point(current_right_speed);

    left_count = encoder_get_count(ENCODER_1);
    right_count = encoder_get_count(ENCODER_2);

    if (current_left_duty != 0 || current_right_duty != 0) {
      if (current_left_duty > 0) {
        gpio_set_level(MOTOR1_DIR, 1);
      } else {
        gpio_set_level(MOTOR1_DIR, 0);
      }
      if (current_right_duty > 0) {
        gpio_set_level(MOTOR2_DIR, 1);
      } else {
        gpio_set_level(MOTOR2_DIR, 0);
      }
      pwm_set_duty(MOTOR1_PWM, std::abs(current_left_duty));
      pwm_set_duty(MOTOR2_PWM, std::abs(current_right_duty));
    } else {
      left.input_feedback(left_count);
      right.input_feedback(right_count);

      float left_output = left.output();
      float right_output = right.output();

      if (left_output > 0) {
        gpio_set_level(MOTOR1_DIR, 1);
      } else {
        gpio_set_level(MOTOR1_DIR, 0);
      }
      if (right_output > 0) {
        gpio_set_level(MOTOR2_DIR, 1);
      } else {
        gpio_set_level(MOTOR2_DIR, 0);
      }
      pwm_set_duty(MOTOR1_PWM, std::abs(left_output));
      pwm_set_duty(MOTOR2_PWM, std::abs(right_output));

      std::cout << "left count:" << left_count << " right:" << right_count
                << " | left out:" << left_output << " right:" << right_output
                << '\r' << std::flush;
    }
    std::this_thread::sleep_for(10ms);
  }

  tcp_reader.join();
  delete tcp_socket;
  return 0;
}