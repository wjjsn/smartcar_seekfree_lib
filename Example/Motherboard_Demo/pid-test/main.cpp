#include "pid.hpp"
#include "zf_common_headfile.h"
#include "zf_driver_gpio.h"
#include "zf_driver_pwm.h"
#include "socket.hpp"
#include <thread>
#include <mutex>
#include <atomic>
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
std::atomic<float> left_duty{0}, right_duty{0};
std::atomic<float> left_speed{0}, right_speed{0};
std::atomic<float> kp{0}, ki{0}, kd{0};

int main(int argc, char **argv) {
  PID left(100, +[](float error) { return (float)(error * 2.0f); }, 0, 0, 10000,
           -10000);
  PID right(100, +[](float error) { return (float)(error * 2.0f); }, 0, 0, 10000,
            -10000);

  int16 left_count, right_count;

  auto tcp_socket = std::make_unique<TcpSocket>();
  tcp_socket->connect_to_server("192.168.1.123", 1347);

  std::thread tcp_reader([&]() {
    char recv_buffer[256] = {0};
    while (true) {
      ssize_t recv_len = tcp_socket->recv_data(recv_buffer, sizeof(recv_buffer) - 1);
      if (recv_len > 0) {
        char key[32];
        int value;
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

  while (true) {
    {
      std::lock_guard<std::mutex> lock(pid_mutex);
      left.set_pid(+[](float error) { return error * kp.load(); }, ki.load(), kd.load());
      right.set_pid(+[](float error) { return error * kp.load(); }, ki.load(), kd.load());
      left.set_point(left_speed.load());
      right.set_point(right_speed.load());
    }

    left_count = encoder_get_count(ENCODER_1);
    right_count = encoder_get_count(ENCODER_2);

    float ld = left_duty.load();
    float rd = right_duty.load();

    if (ld != 0 || rd != 0) {
      if (ld > 0) {
        gpio_set_level(MOTOR1_DIR, 1);
      } else {
        gpio_set_level(MOTOR1_DIR, 0);
      }
      if (rd > 0) {
        gpio_set_level(MOTOR2_DIR, 1);
      } else {
        gpio_set_level(MOTOR2_DIR, 0);
      }
      pwm_set_duty(MOTOR1_PWM, std::abs(ld));
      pwm_set_duty(MOTOR2_PWM, std::abs(rd));
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
}