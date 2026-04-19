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

#define MOTOR1_DIR "/dev/zf_driver_gpio_motor_1"
#define MOTOR1_PWM "/dev/zf_device_pwm_motor_1"

#define MOTOR2_DIR "/dev/zf_driver_gpio_motor_2"
#define MOTOR2_PWM "/dev/zf_device_pwm_motor_2"

#define ENCODER_1 "/dev/zf_encoder_1"
#define ENCODER_2 "/dev/zf_encoder_2"

volatile std::atomic<bool> g_running{true};

std::atomic<float> g_left_duty{0}, g_right_duty{0};
std::atomic<float> g_left_speed{0}, g_right_speed{0};
std::atomic<float> g_kp{1.0f}, g_ki{0.0f}, g_kd{0.0f};

std::mutex g_pid_mutex;

float pid_kp_func(float error) { return error * g_kp.load(); }

int main(int argc, char **argv) {
  signal(SIGSEGV, [](int) { g_running = false; });
  signal(SIGINT, [](int) { g_running = false; });

  PID left(100, pid_kp_func, 0, 0, 10000, -10000);
  PID right(100, pid_kp_func, 0, 0, 10000, -10000);

  int16 left_count = 0, right_count = 0;

  TcpSocket* tcp_socket = nullptr;
  try {
    tcp_socket = new TcpSocket();
    tcp_socket->connect_to_server("192.168.1.123", 1347);
    std::cout << "[Main] TCP connected" << std::endl;
  } catch (const std::exception& e) {
    std::cerr << "[Error] TCP connection failed: " << e.what() << std::endl;
    if (tcp_socket) delete tcp_socket;
    return 1;
  }

  std::thread tcp_reader([&tcp_socket]() {
    char recv_buffer[256] = {0};
    while (g_running) {
      ssize_t recv_len = tcp_socket->recv_data(recv_buffer, sizeof(recv_buffer) - 1);
      if (recv_len > 0) {
        recv_buffer[recv_len] = '\0';
        std::cout << "[TCP] Received: " << recv_buffer << std::endl;

        char key[32] = {0};
        int value = 0;
        if (sscanf(recv_buffer, "%31[^:]:%d", key, &value) == 2) {
          bool updated = false;
          std::lock_guard<std::mutex> lock(g_pid_mutex);
          if (strcmp(key, "kp") == 0) { g_kp = value; updated = true; }
          else if (strcmp(key, "ki") == 0) { g_ki = value; updated = true; }
          else if (strcmp(key, "kd") == 0) { g_kd = value; updated = true; }
          else if (strcmp(key, "left_speed") == 0) { g_left_speed = value; updated = true; }
          else if (strcmp(key, "right_speed") == 0) { g_right_speed = value; updated = true; }
          else if (strcmp(key, "left_duty") == 0) { g_left_duty = value; updated = true; }
          else if (strcmp(key, "right_duty") == 0) { g_right_duty = value; updated = true; }
          if (updated) std::cout << "[Params] " << key << "=" << value << std::endl;
        }
      }
      std::this_thread::sleep_for(10ms);
    }
  });

  std::cout << "[Main] Starting main loop" << std::endl;

  while (g_running) {
    float left_speed_val, right_speed_val;
    float left_duty_val, right_duty_val;
    float ki_val, kd_val;
    {
      std::lock_guard<std::mutex> lock(g_pid_mutex);
      left_speed_val = g_left_speed.load();
      right_speed_val = g_right_speed.load();
      left_duty_val = g_left_duty.load();
      right_duty_val = g_right_duty.load();
      ki_val = g_ki.load();
      kd_val = g_kd.load();
    }

    left.set_point(left_speed_val);
    right.set_point(right_speed_val);
    left.set_pid(pid_kp_func, ki_val, kd_val);
    right.set_pid(pid_kp_func, ki_val, kd_val);

    left_count = encoder_get_count(ENCODER_1);
    right_count = encoder_get_count(ENCODER_2);

    if (left_duty_val != 0 || right_duty_val != 0) {
      std::cout << "[Main] Direct PWM mode: left=" << left_duty_val
                << " right=" << right_duty_val << std::endl;

      if (left_duty_val > 0) {
        gpio_set_level(MOTOR1_DIR, 1);
      } else {
        gpio_set_level(MOTOR1_DIR, 0);
      }
      if (right_duty_val > 0) {
        gpio_set_level(MOTOR2_DIR, 1);
      } else {
        gpio_set_level(MOTOR2_DIR, 0);
      }
      pwm_set_duty(MOTOR1_PWM, std::abs(left_duty_val));
      pwm_set_duty(MOTOR2_PWM, std::abs(right_duty_val));
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

  std::cout << "[Main] Exiting" << std::endl;
  tcp_reader.join();
  delete tcp_socket;
  return 0;
}