/*********************************************************************************************************************
* LS2K0300 Opensourec Library 即（LS2K0300 开源库）是一个基于官方 SDK 接口的第三方开源库
* Copyright (c) 2022 SEEKFREE 逐飞科技
*
* 本文件是LS2K0300 开源库的一部分
*
* LS2K0300 开源库 是免费软件
* 您可以根据自由软件基金会发布的 GPL（GNU General Public License，即 GNU通用公共许可证）的条款
* 即 GPL 的第3版（即 GPL3.0）或（您选择的）任何后来的版本，重新发布和/或修改它
*
* 本开源库的发布是希望它能发挥作用，但并未对其作任何的保证
* 甚至没有隐含的适销性或适合特定用途的保证
* 更多细节请参见 GPL
*
* 您应该在收到本开源库的同时收到一份 GPL 的副本
* 如果没有，请参阅<https://www.gnu.org/licenses/>
*
* 额外注明：
* 本开源库使用 GPL3.0 开源许可证协议 以上许可申明为译文版本
* 许可申明英文版在 libraries/doc 文件夹下的 GPL3_permission_statement.txt 文件中
* 许可证副本在 libraries 文件夹下 即该文件夹下的 LICENSE 文件
* 欢迎各位使用并传播本程序 但修改内容时必须保留逐飞科技的版权声明（即本声明）
*
* 文件名称          main
* 公司名称          成都逐飞科技有限公司
* 适用平台          LS2K0300
* 店铺链接          https://seekfree.taobao.com/
*
* 修改记录
* 日期              作者           备注
* 2025-02-27        大W            first version
********************************************************************************************************************/

// *************************** 例程硬件连接说明 ***************************
//  久久派与主板使用54pin排线连接，再将久久派插到主板上面，确保插到底核心板与主板插座间没有缝隙即可
//  久久派与主板使用54pin排线连接，再将久久派插到主板上面，确保插到底核心板与主板插座间没有缝隙即可
//  久久派与主板使用54pin排线连接，再将久久派插到主板上面，确保插到底核心板与主板插座间没有缝隙即可
//  使用本历程，就需要使用我们逐飞科技提供的内核。
// 
// 使用久久派直接接线进行测试
//      模块管脚            单片机管脚
//      MOTOR1_DIR          GPIO73
//      MOTOR1_PWM          GPIO65
//   
// .   GND                 GND   
//      MOTOR2_DIR          GPIO72
//      MOTOR2_PWM          GPIO66
//      GND                 GND
//      接线端子 +          电池正极
//      接线端子 -          电池负极
// 
// 使用学习主板进行测试
//      将模块的电源接线端子与主板的驱动供电端子连接
//      将模块的信号接口使用配套灰排线与主板电机信号接口连接 请注意接线方向 不确定方向就是用万用表确认一下 引脚参考上方核心板连接
//      将主板与供电电池正确连接
// 
// *************************** 例程测试说明 ***************************
// 1.核心板烧录完成本例程 主板电池供电
// 
// 2.如果接了电机 可以看到电机周期正反转
// 
// 3.如果没有接电机 使用万用表可以在驱动电机输出端子上测量到输出电压变化
// 
// 如果发现现象与说明严重不符 请参照本文件最下方 例程常见问题说明 进行排查

// $:cat /sys/kernel/debug/pwm
// platform/16119000.pwm, 4 PWM devices
//  pwm-0   ((null)              ): period: 0 ns duty: 0 ns polarity: normal
//  pwm-1   ((null)              ): period: 0 ns duty: 0 ns polarity: normal
//  pwm-2   ((null)              ): period: 0 ns duty: 0 ns polarity: normal
//  pwm-3   ((null)              ): period: 0 ns duty: 0 ns polarity: normal

// platform/16118000.pwm, 7 PWM devices
//  pwm-0   ((null)              ): period: 0 ns duty: 0 ns polarity: normal
//  pwm-1   ((null)              ): period: 0 ns duty: 0 ns polarity: normal
//  pwm-2   ((null)              ): period: 0 ns duty: 0 ns polarity: normal
//  pwm-3   ((null)              ): period: 0 ns duty: 0 ns polarity: normal
//  pwm-4   ((null)              ): period: 0 ns duty: 0 ns polarity: normal
//  pwm-5   ((null)              ): period: 0 ns duty: 0 ns polarity: normal
//  pwm-6   ((null)              ): period: 0 ns duty: 0 ns polarity: normal

// platform/1611b020.pwm, 1 PWM device
//  pwm-0   ((null)              ): period: 0 ns duty: 0 ns polarity: normal

// platform/1611b010.pwm, 1 PWM device
//  pwm-0   ((null)              ): period: 0 ns duty: 0 ns polarity: normal
// $:ls -l /sys/class/pwm/
// pwmchip0 -> ../../devices/platform/soc/1611b010.pwm/pwm/pwmchip0
// pwmchip1 -> ../../devices/platform/soc/1611b020.pwm/pwm/pwmchip1
// pwmchip2 -> ../../devices/platform/soc/16118000.pwm/pwm/pwmchip2
// pwmchip9 -> ../../devices/platform/soc/16119000.pwm/pwm/pwmchip9

// **************************** 代码区域 ****************************
#include "pwm.hpp"
#include <atomic>
#include <chrono>
#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <string.h>
#include <thread>
#define MOTOR1_DIR "/dev/zf_driver_gpio_motor_1"

#define MOTOR2_DIR "/dev/zf_driver_gpio_motor_2"

// 持久化 fd API：两个 fd 存放在 std::atomic<int> 中，避免依赖 zf 库
static std::atomic<int> gpio_fd1{-1};
static std::atomic<int> gpio_fd2{-1};

// 打开并保存 fd 到 atomic 中，若已有 fd 则关闭新打开的 fd
static bool gpio_open_persistent(std::atomic<int> &afd, const char *path) {
  int fd = open(path, O_WRONLY | O_CLOEXEC);
  if (fd == -1) {
    fprintf(stderr, "open(%s) failed: %s\n", path, strerror(errno));
    return false;
  }

  int expected = -1;
  if (!afd.compare_exchange_strong(expected, fd)) {
    // already had a fd, close the one we just opened
    close(fd);
  }
  return true;
}

// 写入 '0'/'1' 到已保存的 fd
static bool gpio_set_level_persistent(std::atomic<int> &afd, int level) {
  int fd = afd.load();
  if (fd == -1)
    return false;
  const char buf = (level ? '1' : '0');
  lseek(fd, 0, SEEK_SET);
  ssize_t n = write(fd, &buf, 1);
  if (n != 1) {
    fprintf(stderr, "write failed on fd %d: %s\n", fd, strerror(errno));
    return false;
  }
  return true;
}

// 关闭并清除 atomic 中的 fd
static void gpio_close_persistent(std::atomic<int> &afd) {
  int fd = afd.exchange(-1);
  if (fd != -1)
    close(fd);
}
int main() {
  try {
    // 使用持久化 fd 的方式控制方向，避免依赖 zf 库
    gpio_open_persistent(gpio_fd1, MOTOR1_DIR);
    gpio_open_persistent(gpio_fd2, MOTOR2_DIR);

    if (gpio_fd1.load() == -1 || gpio_fd2.load() == -1) {
      std::cerr << "打开 GPIO 设备失败，请检查驱动或设备节点" << std::endl;
    }

    gpio_set_level_persistent(gpio_fd1, 1);
    gpio_set_level_persistent(gpio_fd2, 1);
    // 1. 初始化 pwmchip1, 通道 0 (对应你的 1611b010.pwm 硬件)
    std::cout << "正在初始化 PWM 设备..." << std::endl;
    PWM my_pwm0(0, 0);
    PWM my_pwm1(1, 0);

    // 2. 配置：频率 2kHz (2000Hz)，占空比 30%
    std::cout << "设置频率 17000Hz, 占空比 0%..." << std::endl;
    if (!my_pwm0.config(17000, 0)) {
      std::cerr << "配置 PWM0 失败！" << std::endl;
      return -1;
    }
    if (!my_pwm1.config(17000, 0)) {
      std::cerr << "配置 PWM1 失败！" << std::endl;
      return -1;
    }

    // 3. 开启输出
    std::cout << "开启 PWM 输出..." << std::endl;
    my_pwm0.enable();
    my_pwm1.enable();

    // 4. 让它跑 5 秒钟
    for (float duty = 0; duty <= 100; duty += 0.5) {
      my_pwm0.set_duty_cycle(duty);
      my_pwm1.set_duty_cycle(duty);
      std::cout << "占空比: " << duty << "%" << '\r' << std::flush;
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    gpio_set_level_persistent(gpio_fd1, 0);
    gpio_set_level_persistent(gpio_fd2, 0);

    double input;
    while (true) {
      std::cin >> input;
      my_pwm0.set_duty_cycle(input);
    }

  } catch (const std::exception &e) {
    std::cerr << "发生错误: " << e.what() << std::endl;
    return -1;
  }

  return 0;
}