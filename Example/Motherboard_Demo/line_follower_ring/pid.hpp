#pragma once
#include <sstream>
#ifdef __cplusplus
#include <stdint.h>
#include <stdio.h>

class PID {
  float (*function_kp_)(float error), ki_, kd_;
  float error_realtime_, error_lasttime_, error_sum_;
  float setpoint_;
  float output_min_, output_max_;
  float speed_, output_;
  // 双向死区补偿（当控制输出非零时应用）
  float dead_forward_ = 0.0f;  // 输出为正时加的补偿
  float dead_backward_ = 0.0f; // 输出为负时加的补偿（以正数表示幅度）

  bool enable_integral_limit_;
  float integral_limit_max_;
  float integral_limit_min_;

  float debug_buf_[8];
  const uint8_t debug_tail_[4] = {0x00, 0x00, 0x80, 0x7f};

public:
  //   PID(){};

  PID(float point, float (*function_kp)(float error), float ki = 0,
      float kd = 0, float output_min = -100.0f, float output_max = 100.0f) {
    function_kp_ = function_kp;
    ki_ = ki;
    kd_ = kd;
    error_realtime_ = 0.0f;
    error_lasttime_ = 0.0f;
    error_sum_ = 0.0f;
    setpoint_ = point;
    output_min_ = output_min;
    output_max_ = output_max;
    enable_integral_limit_ = false;
  };

  void set_pid(float (*function_kp)(float error), float ki = 0, float kd = 0) {
    function_kp_ = function_kp;
    ki_ = ki;
    kd_ = kd;
  }
  void set_point(float point) { setpoint_ = point; }
  void input_feedback(float feedback) {
    speed_ = feedback;
    error_lasttime_ = error_realtime_;
    error_realtime_ = setpoint_ - feedback;
    error_sum_ += error_realtime_;
    if (enable_integral_limit_ && (error_sum_ > integral_limit_max_ ||
                                   error_sum_ < integral_limit_min_)) {
      if (error_sum_ > integral_limit_max_)
        error_sum_ = integral_limit_max_;
      else if (error_sum_ < integral_limit_min_)
        error_sum_ = integral_limit_min_;
    }
  }
  float output() {
    float raw = function_kp_(error_realtime_) + ki_ * error_sum_ +
                kd_ * (error_realtime_ - error_lasttime_);

    // 应用双向死区补偿：当 raw 为正（正转）时加前向补偿；为负时减去反向补偿
    if (raw > 0.0f) {
      raw += dead_forward_;
    } else if (raw < 0.0f) {
      raw -= dead_backward_;
    }

    output_ = raw;
    if (output_ > output_max_)
      output_ = output_max_;
    else if (output_ < output_min_)
      output_ = output_min_;
    //		printf("output_:%d",(int)output_);
    return output_;
  }

  // 设置前向/反向死区补偿，单位与 PID 输出相同（例如 PWM 单位）
  void set_deadzone(float forward, float backward) {
    dead_forward_ = forward;
    dead_backward_ = backward;
  }
  void set_integral_limit(float limit_max, float limit_min) {
    enable_integral_limit_ = true;
    integral_limit_max_ = limit_max;
    integral_limit_min_ = limit_min;
  }
  std::ostringstream debug_output() {
    std::ostringstream debug;
    debug << setpoint_ << ',' << speed_ << ',' << output_ << ','<<error_sum_;
    return debug;
  }
};
#endif