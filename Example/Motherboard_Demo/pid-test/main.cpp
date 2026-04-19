
#include "pid.hpp"
#include "zf_common_headfile.h"
#include "zf_driver_gpio.h"
#include "zf_driver_pwm.h"
#include <thread>
using namespace std::chrono_literals;
#define KEY_0 "/dev/zf_driver_gpio_key_0"
#define KEY_1 "/dev/zf_driver_gpio_key_1"

#define MOTOR1_DIR "/dev/zf_driver_gpio_motor_1"
#define MOTOR1_PWM "/dev/zf_device_pwm_motor_1"

#define MOTOR2_DIR "/dev/zf_driver_gpio_motor_2"
#define MOTOR2_PWM "/dev/zf_device_pwm_motor_2"

#define ENCODER_1 "/dev/zf_encoder_1"
#define ENCODER_2 "/dev/zf_encoder_2"

// ==========================================
// 配置参数
// ==========================================
//      模块管脚            单片机管脚
//      MOTOR1_DIR          GPIO73
//      MOTOR1_PWM          GPIO65
//
// .   GND                 GND
//      MOTOR2_DIR          GPIO72
//      MOTOR2_PWM          GPIO66
//      GND                 GND

// ==========================================
// 主程序
// ==========================================
int main(int argc, char **argv) {
  PID left(
      100, +[](float error) { return (float)(error * 2.0f); }, 0, 0, 10000,
      -10000);
  PID right(
      100, +[](float error) { return (float)(error * 2.0f); }, 0, 0, 10000,
      -10000);

  // atexit([]() {
  //   pwm_set_duty(MOTOR1_PWM, 0);
  //   pwm_set_duty(MOTOR2_PWM, 0);
  // });

  int16 left_count,right_count;

  
  while (true) {
    left_count = encoder_get_count(ENCODER_1);
    right_count = encoder_get_count(ENCODER_2);

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

    std::cout << "left count:" << left_count << "right" << right_count
              << " | left out:" << left_output << " right:" << right_output
              << '\r' << std::flush;
    std::this_thread::sleep_for(10ms);
  }
}