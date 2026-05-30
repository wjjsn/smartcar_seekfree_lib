#ifndef PWM_HPP
#define PWM_HPP

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unistd.h>

class PWM {
private:
  int m_chip_id;      // pwmchipX 的编号
  int m_channel;      // 通道号，龙芯上通常为 0
  std::string m_path; // pwm 通道控制目录路径
  bool m_exported;    // 是否是当前对象导出的通道

  // 内部私有函数：向指定文件写入字符串
  bool write_sysfs(const std::string &sub_path, const std::string &value) {
    std::ofstream file(m_path + sub_path);
    if (!file.is_open()) {
      return false;
    }
    file << value;
    return true;
  }

  // 内部私有函数：向指定文件写入整数字符串
  bool write_sysfs(const std::string &sub_path, long value) {
    return write_sysfs(sub_path, std::to_string(value));
  }

  // 内部私有函数：从指定文件读取字符串
  bool read_sysfs(const std::string &sub_path, std::string &value) {
    std::ifstream file(m_path + sub_path);
    if (!file.is_open()) {
      return false;
    }
    std::getline(file, value);
    return true;
  }

  // 检查路径是否存在
  bool dir_exists(const std::string &dir_path) {
    return access(dir_path.c_str(), F_OK) == 0;
  }

public:
  /**
   * @brief PWM 构造函数
   * @param chip_id pwmchip 的编号（例如 1 代表 pwmchip1）
   * @param channel 通道号（默认 0）
   */
  PWM(int chip_id, int channel = 0)
      : m_chip_id(chip_id), m_channel(channel), m_exported(false) {

    std::string chip_path =
        "/sys/class/pwm/pwmchip" + std::to_string(m_chip_id) + "/";
    m_path = chip_path + "pwm" + std::to_string(m_channel) + "/";

    // 如果该通道还没有被导出，则尝试导出
    if (!dir_exists(m_path)) {
      std::ofstream export_file(chip_path + "export");
      if (!export_file.is_open()) {
        throw std::runtime_error(
            "无法打开 export 文件，请检查 root 权限或 chip_id 是否正确！");
      }
      export_file << m_channel;
      export_file.close();

      // 等待系统创建完设备文件（sysfs 有时会有微小延迟）
      usleep(100000);
      m_exported = true;
    }

    if (!dir_exists(m_path)) {
      throw std::runtime_error("PWM 导出失败，无法访问路径: " + m_path);
    }
  }

  /**
   * @brief 析构函数：如果在构造时导出了通道，离开作用域时自动关闭并销毁
   */
  ~PWM() {
    disable();
    if (m_exported) {
      std::string unexport_path =
          "/sys/class/pwm/pwmchip" + std::to_string(m_chip_id) + "/unexport";
      std::ofstream unexport_file(unexport_path);
      if (unexport_file.is_open()) {
        unexport_file << m_channel;
      }
    }
  }

  /**
   * @brief 设置周期 (Period)
   * @param period_ns 周期，单位：纳秒 (ns)
   */
  bool set_period(long period_ns) { return write_sysfs("period", period_ns); }

  /**
   * @brief 设置占空比（两种重载）
   * - `set_duty_cycle(long duty_ns)`：保留原始接口，直接以纳秒写入
   * - `set_duty_cycle(double duty_percent)`：以 0.0-100.0
   * 百分比设置，占空比由当前 period 计算
   */
  // 保留原始接口：以纳秒为单位直接设置 duty_cycle
  bool set_duty_cycle(long duty_ns) {
    return write_sysfs("duty_cycle", duty_ns);
  }

  // 新增接口：以百分比设置占空比（0.0 - 100.0）
  bool set_duty_cycle(double duty_percent) {
    if (duty_percent < 0.0 || duty_percent > 100.0) {
      return false;
    }

    std::string period_str;
    if (!read_sysfs("period", period_str)) {
      return false;
    }

    long period_ns = 0;
    try {
      period_ns = std::stol(period_str);
    } catch (const std::exception &) {
      return false;
    }

    // 反转映射以与示波器一致（软件层翻转）
    long duty_ns =
        static_cast<long>(period_ns * ((100.0 - duty_percent) / 100.0));
    return write_sysfs("duty_cycle", duty_ns);
  }

  /**
   * @brief 通过频率和占空比百分比来设置 PWM
   * @param freq_hz 频率（Hz），如 1000 代表 1kHz
   * @param duty_percent 占空比百分比 (0.0 到 100.0)
   */
  bool config(double freq_hz, double duty_percent) {
    if (freq_hz <= 0 || duty_percent < 0.0 || duty_percent > 100.0)
      return false;

    long period_ns = static_cast<long>(1000000000.0 / freq_hz);
    long duty_ns = static_cast<long>(period_ns * (duty_percent / 100.0));

    // 核心安全规则：先设置大的，再设置小的，防止内核拒绝写入
    // 这里采用先设一个安全的占空比 0，再设周期，最后设新占空比
    write_sysfs("duty_cycle", 0);
    if (!write_sysfs("period", period_ns))
      return false;
    return write_sysfs("duty_cycle", duty_ns);
  }

  /**
   * @brief 使能 PWM 输出
   */
  bool enable() { return write_sysfs("enable", "1"); }

  /**
   * @brief 关闭 PWM 输出
   */
  bool disable() { return write_sysfs("enable", "0"); }

  /**
   * @brief 设置极性
   * @param inversed true 为反相，false 为正常(normal)
   */
  bool set_polarity(bool inversed) {
    // 修改极性前，某些内核要求必须先 disable
    return write_sysfs("polarity", inversed ? "inversed" : "normal");
  }
};

#endif // PWM_HPP