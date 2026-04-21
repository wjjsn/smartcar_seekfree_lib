#include "framebuffer.h"
#include "pid.hpp"
#include "zf_common_headfile.h"
#include "zf_driver_encoder.h"
#include "zf_driver_gpio.h"
#include <iostream>
#include <opencv2/opencv.hpp>
#include <pthread.h>
#include <vector>

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
const int FRAME_WIDTH = 160;
const int FRAME_HEIGHT = 120;
const int BINARY_THRESHOLD = 150;
const int ROI_START_ROW = (FRAME_HEIGHT * 2) / 3;
const int ROI_HEIGHT = FRAME_HEIGHT - ROI_START_ROW;
const float BASE_SPEED = 50.0f;
float TURN_SENSITIVITY = 0.8f;

cv::Point2f calculateWheelSpeeds(float frameCenter, float trackCenter,
                                 float baseSpeed, float sensitivity) {
  float error = trackCenter - frameCenter;
  float turnOffset = error * sensitivity;
  float leftSpeed = baseSpeed + turnOffset;
  float rightSpeed = baseSpeed - turnOffset;
  float maxSpeed = 100.0f;
  float minSpeed = -20.0f;
  leftSpeed = std::max(minSpeed, std::min(leftSpeed, maxSpeed));
  rightSpeed = std::max(minSpeed, std::min(rightSpeed, maxSpeed));
  return cv::Point2f(leftSpeed, rightSpeed);
}

// ==========================================
// PID参数和全局变量
// ==========================================
float g_left_kp = 1.0f, g_left_ki = 0.0f, g_left_kd = 0.0f;
float g_right_kp = 1.0f, g_right_ki = 0.0f, g_right_kd = 0.0f;

volatile float g_left_target_speed = 0.0f, g_right_target_speed = 0.0f;
volatile float g_left_duty = 0.0f, g_right_duty = 0.0f;

volatile sig_atomic_t g_running = 1;

void signal_handler(int) {
  pwm_set_duty(MOTOR1_PWM, 5000);
  pwm_set_duty(MOTOR2_PWM, 5000);
  g_running = 0;
}

float left_kp_func(float error) { return error * g_left_kp; }
float right_kp_func(float error) { return error * g_right_kp; }

int convert_to_output(int value) { return value + 5000; }

void *pid_thread(void *) {
  PID left_pid(100, left_kp_func, 0, 0, -5000, 5000);
  PID right_pid(100, right_kp_func, 0, 0, -5000, 5000);
  left_pid.set_integral_limit(23333, -23333);
  right_pid.set_integral_limit(23333, -23333);

  gpio_set_level(MOTOR1_DIR, 1);
  gpio_set_level(MOTOR2_DIR, 1);

  while (g_running) {
    int16_t lc = encoder_get_count(ENCODER_1);
    int16_t rc = encoder_get_count(ENCODER_2);

    left_pid.set_point(g_left_target_speed);
    right_pid.set_point(g_right_target_speed);
    left_pid.set_pid(left_kp_func, g_left_ki, g_left_kd);
    right_pid.set_pid(right_kp_func, g_right_ki, g_right_kd);

    if (g_left_duty != 0 || g_right_duty != 0) {
      pwm_set_duty(MOTOR1_PWM, convert_to_output((int)g_left_duty));
      pwm_set_duty(MOTOR2_PWM, convert_to_output((int)g_right_duty));
    } else {
      left_pid.input_feedback((float)-lc);
      right_pid.input_feedback((float)rc);

      float left_output = left_pid.output();
      float right_output = right_pid.output();

      pwm_set_duty(MOTOR1_PWM, convert_to_output((int)left_output));
      pwm_set_duty(MOTOR2_PWM, convert_to_output((int)right_output));

      printf("set_point: left=%.1f right=%.1f | pid_out: "
             "left=%.2f right=%.2f\r",
             g_left_target_speed, g_right_target_speed, left_output,
             right_output);
      fflush(stdout);
    }

    usleep(10000);
  }

  pwm_set_duty(MOTOR1_PWM, 5000);
  pwm_set_duty(MOTOR2_PWM, 5000);
  return NULL;
}

int main(int argc, char **argv) {
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);
  signal(SIGSEGV, signal_handler);

  cv::VideoCapture cap(0);
  if (!cap.isOpened()) {
    std::cerr << "Error: Could not open camera." << std::endl;
    return -1;
  }

  cap.set(cv::CAP_PROP_FRAME_WIDTH, FRAME_WIDTH);
  cap.set(cv::CAP_PROP_FRAME_HEIGHT, FRAME_HEIGHT);
  cap.set(cv::CAP_PROP_FPS, 180);

  std::cout << "Starting Line Follower with PID..." << std::endl;
  std::cout << "Resolution: " << FRAME_WIDTH << "x" << FRAME_HEIGHT << std::endl;
  std::cout << "ROI Height: " << ROI_HEIGHT << " pixels" << std::endl;

  pthread_t pid_tid;
  pthread_create(&pid_tid, NULL, pid_thread, NULL);

  cv::Mat frame, gray, binary;
  int targetX = FRAME_WIDTH / 2;

  while (g_running) {
    cap >> frame;
    if (frame.empty())
      break;

    if (0 == gpio_get_level(KEY_0)) {
      g_left_kp += 0.1f;
      while (0 == gpio_get_level(KEY_0))
        ;
      printf("left_kp=%.2f\r\n", (double)g_left_kp);
    }
    if (0 == gpio_get_level(KEY_1)) {
      g_right_kp += 0.1f;
      while (0 == gpio_get_level(KEY_1))
        ;
      printf("right_kp=%.2f\r\n", (double)g_right_kp);
    }

    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    cv::Rect roiRect(0, ROI_START_ROW, FRAME_WIDTH, ROI_HEIGHT);
    cv::Mat roi = gray(roiRect);
    cv::threshold(roi, binary, BINARY_THRESHOLD, 255, cv::THRESH_BINARY);

    cv::Moments m = cv::moments(binary, true);
    if (m.m00 > 0) {
      targetX = static_cast<int>(m.m10 / m.m00);
    } else {
      std::cout << "Line lost!" << std::endl;
    }

    cv::Point2f speeds =
        calculateWheelSpeeds((float)FRAME_WIDTH / 2.0f, (float)targetX,
                             BASE_SPEED, TURN_SENSITIVITY);

    g_left_target_speed = speeds.x;
    g_right_target_speed = speeds.y;

    std::cout << "\rTrack Center X: " << targetX
              << " | Target Speed: L=" << speeds.x << " R=" << speeds.y
              << "    " << std::flush;
  }

  pthread_join(pid_tid, NULL);
  std::cout << std::endl << "Exiting..." << std::endl;
  return 0;
}
