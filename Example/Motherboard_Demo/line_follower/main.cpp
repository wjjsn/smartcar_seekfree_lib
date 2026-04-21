#include "framebuffer.h"
#include "pid.hpp"
#include "zf_common_headfile.h"
#include "zf_driver_encoder.h"
#include "zf_driver_gpio.h"
#include <iostream>
#include <opencv2/opencv.hpp>
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
// 我们选择一个非常低的分辨率，以获得极高的处理速度
const int FRAME_WIDTH = 160;
const int FRAME_HEIGHT = 120;

// 二值化阈值。高于此值的像素变为白色（255），低于的变为黑色（0）。
// 注意：要求的是“两边黑中间白”，所以我们需要寻找白色区域。
// 如果环境光线变动大，这里需要调整，或者使用自适应阈值。
const int BINARY_THRESHOLD = 150;

// 感兴趣区域 (ROI) 设置。我们不需要看整张图，只看底部 1/3。
// 这样可以进一步提高速度，并过滤掉远处的干扰。
const int ROI_START_ROW = (FRAME_HEIGHT * 2) / 3; // 从图像的 2/3 处开始
const int ROI_HEIGHT = FRAME_HEIGHT - ROI_START_ROW; // 剩余的高度

// 车辆运动基础参数
const float BASE_SPEED = 50.0f; // 直线行驶时的基础速度
float TURN_SENSITIVITY = 0.8f; // 转向灵敏度 (非PID，只是简单的比例映射)

// ==========================================
// 核心函数：根据中线偏差计算轮速
// ==========================================
/**
 * @brief 根据图像中心偏差计算左轮和右轮的基础速度。
 * * @param frameCenter 图像的物理中轴线坐标 (例如 FRAME_WIDTH/2)
 * @param trackCenter 算法检测到的路径中线坐标
 * @param baseSpeed 直线行驶时的基础速度
 * @param sensitivity 转向灵敏度
 * @return cv::Point2f 返回一个点，x 表示左轮速度，y 表示右轮速度
 */
cv::Point2f calculateWheelSpeeds(float frameCenter, float trackCenter,
                                 float baseSpeed, float sensitivity) {
  // 1. 计算偏差 (Error)
  // Positive error -> means path is to the right -> we need to turn right.
  // Negative error -> means path is to the left -> we need to turn left.
  float error = trackCenter - frameCenter;

  // 2. 简单的差速计算 (这里是一个极其简化的 P 控制，没有积分和微分)
  // 目的只是演示如何得到轮速。
  float turnOffset = error * sensitivity;

  // 路径在右 (error>0)，右轮需要减速，左轮需要加速（产生右转力矩）
  float leftSpeed = baseSpeed + turnOffset;
  float rightSpeed = baseSpeed - turnOffset;

  // 3. 速度限幅 (防止转速过高或反转，根据实际电机调速范围设置)
  float maxSpeed = 100.0f;
  float minSpeed = -20.0f; // 允许低速反转以实现急转弯

  leftSpeed = std::max(minSpeed, std::min(leftSpeed, maxSpeed));
  rightSpeed = std::max(minSpeed, std::min(rightSpeed, maxSpeed));

  return cv::Point2f(leftSpeed, rightSpeed);
}

float g_kp = 1.0f, g_ki = 0.0f, g_kd = 0.0f;

float left_kp_func(float error) { return error * g_kp; }
float right_kp_func(float error) { return error * g_kp; }

int convert_to_output(int value) { return value + 5000; }

int main(int argc, char **argv) {
  // 1. 初始化摄像头
  cv::VideoCapture cap(0); // 0 号摄像头
  if (!cap.isOpened()) {
    std::cerr << "Error: Could not open camera." << std::endl;
    return -1;
  }

  // 2. 强制设置摄像头的硬件采集分辨率
  cap.set(cv::CAP_PROP_FRAME_WIDTH, FRAME_WIDTH);
  cap.set(cv::CAP_PROP_FRAME_HEIGHT, FRAME_HEIGHT);
  cap.set(cv::CAP_PROP_FPS, 180);

  std::cout << "Starting Line Follower..." << std::endl;
  std::cout << "Resolution: " << FRAME_WIDTH << "x" << FRAME_HEIGHT
            << std::endl;
  std::cout << "ROI Height: " << ROI_HEIGHT << " pixels" << std::endl;

  // 用于显示的图像，防止直接在原图上画图影响算法
  cv::Mat frame, gray, binary, displayFrame;
  int targetX = FRAME_WIDTH / 2; // 默认目标在中心

  struct pwm_info motor_1_pwm_info;
  struct pwm_info motor_2_pwm_info;
  pwm_get_dev_info(MOTOR1_PWM, &motor_1_pwm_info);
  pwm_get_dev_info(MOTOR2_PWM, &motor_2_pwm_info);

  PID left_pid(100, left_kp_func, 0, 0, -5000, 5000);
  PID right_pid(100, right_kp_func, 0, 0, -5000, 5000);
  left_pid.set_integral_limit(23333, -23333);
  right_pid.set_integral_limit(23333, -23333);

  while (true) {
    int16_t lc = encoder_get_count(ENCODER_1);
    int16_t rc = encoder_get_count(ENCODER_2);

    cap >> frame;
    if (frame.empty())
      break;

    if (0 == gpio_get_level(KEY_0)) {
      TURN_SENSITIVITY += 0.1;
      while (0 == gpio_get_level(KEY_0))
        ;
    }
    if (0 == gpio_get_level(KEY_1)) {
      TURN_SENSITIVITY -= 0.1;
      while (0 == gpio_get_level(KEY_1))
        ;
    }
    // --- B. 图像预处理 ---
    // 1. 转灰度 (灰度图处理速度比彩色快)
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);

    // 2. 提取感兴趣区域 (ROI)
    // Rect(x, y, width, height)
    cv::Rect roiRect(0, ROI_START_ROW, FRAME_WIDTH, ROI_HEIGHT);
    cv::Mat roi = gray(roiRect);

    // 3. 二值化 (重点)
    // 要求：两边黑中间白。我们用 THRESH_BINARY
    // 高于阈值的变成白(255)，低于的变成黑(0)。
    cv::threshold(roi, binary, BINARY_THRESHOLD, 255, cv::THRESH_BINARY);

    // --- C. 核心算法：寻找中线 ---
    // 我们在二值化后的 ROI 区域中寻找所有“白色”像素的质心。
    cv::Moments m = cv::moments(binary, true);
    if (m.m00 > 0) {
      // 计算质心的 X 坐标
      targetX = static_cast<int>(m.m10 / m.m00);
} else {
      std::cout << "Line lost!\r" << std::endl;
    }

    cv::Point2f speeds =
        calculateWheelSpeeds((float)FRAME_WIDTH / 2.0f, (float)targetX,
                             BASE_SPEED, TURN_SENSITIVITY);

    left_pid.set_point(speeds.x);
    right_pid.set_point(speeds.y);
    left_pid.set_pid(left_kp_func, g_ki, g_kd);
    right_pid.set_pid(right_kp_func, g_ki, g_kd);

    left_pid.input_feedback((float)-lc);
    right_pid.input_feedback((float)rc);

    float left_output = left_pid.output();
    float right_output = right_pid.output();

    int left_pwm = convert_to_output((int)left_output);
    int right_pwm = convert_to_output((int)right_output);

    if (left_pwm < 5000) {
      gpio_set_level(MOTOR1_DIR, 0);
      left_pwm = 5000 - left_pwm;
    } else if (left_pwm > 5000) {
      gpio_set_level(MOTOR1_DIR, 1);
      left_pwm = left_pwm - 5000;
    } else {
      left_pwm = 0;
    }

    if (right_pwm < 5000) {
      gpio_set_level(MOTOR2_DIR, 0);
      right_pwm = 5000 - right_pwm;
    } else if (right_pwm > 5000) {
      gpio_set_level(MOTOR2_DIR, 1);
      right_pwm = right_pwm - 5000;
    } else {
      right_pwm = 0;
    }

    pwm_set_duty(MOTOR1_PWM, left_pwm);
    pwm_set_duty(MOTOR2_PWM, right_pwm);
    // --- E. 输出结果
    // (这里在终端打印，实际使用中需要通过串口/GPIO发送给电机驱动) --- \r
    // 表示回到行首，实现动态刷新
    std::cout << "\rTrack Center X: " << targetX
              << " | Motor Speeds: L=" << speeds.x << " R=" << speeds.y
              << "    " << std::flush;
  }

  std::cout << std::endl << "Exiting..." << std::endl;
  return 0;
}