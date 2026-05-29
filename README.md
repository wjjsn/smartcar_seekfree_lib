⚠️提示：逐飞的驱动有问题（commit哈希:05e9f2bf36fb40297598bf26a22cddfd68310d11），多线程环境下使用`pwm_set_duty`会导致`SIGSEGV`异常！

# 龙芯LS2K0300智能车开源库

## 简介

本仓库是从[逐飞科技LS2K0300开源库](https://gitee.com/seekfree/LS2K0300_Library)fork而来的，专注于**智能车竞赛应用**，在原库基础上进行了大量定制开发。

主要特性：
- 智能车图像分类（TFLite实时推理）
- PID巡线控制
- TCP/UDP网络通信
- UVC摄像头采集
- 电机PWM控制
- 自动构建FIT镜像

## 硬件平台

- **核心板**: LS2K0300久久派（LoongArch64）
- **传感器**: USB摄像头、编码器

## 快速开始

### 1. 进入开发容器

```bash
docker compose -f .devcontainer/docker-compose.yml run --rm loongson_devcontainer bash
```

### 2. 编译所有示例

```bash
cd /workspace
just all
```

这会依次完成：
- 构建buildroot根文件系统
- 编译Example中的所有示例程序
- 生成FIT镜像到`build/`目录

### 3. 部署到设备

编译产物在`build/`目录下，包括：
- `image.itb` - FIT镜像（内核+根文件系统）
- 打包好的根文件系统 - 各示例程序已包含在内

## 示例说明

| 示例 | 功能 |
|------|------|
| `line_follower` | 视觉巡线，使用PID控制电机 |
| `pid-test` | TCP调参PID，实时网络调试 |
| `E11_smartcar_realtime_tflite` | A4纸检测+TFLite图像分类推理 |
| `E10_tflite_test` | TFLite批量图片测试 |
| `E07_*` | UDP/TCP网络通信示例 |
| `E08_01_uvc_cmera_ips200_display_demo` | UVC摄像头显示示例 |

## 项目结构

```
.
├── Example/                    # 示例程序
│   ├── Motherboard_Demo/      # 主板示例
│   │   ├── line_follower/     # 巡线程序
│   │   ├── pid-test/          # PID调试工具
│   │   └── E11_smartcar_realtime_tflite/  # 智能车推理
│   └── libraries/             # 驱动库
│       └── zf_components/     # 组件库(TFLite等)
├── buildroot-2405/            # Buildroot根文件系统
├── ls2k0300_linux_4.19/       # Linux内核源码
├── .devcontainer/             # 开发容器配置
├── justfile                   # 构建脚本
└── build/                     # 编译输出目录
```

## 构建命令

| 命令 | 说明 |
|------|------|
| `just all` | 完整构建（根文件系统+示例+镜像） |
| `just build-buildroot` | 仅构建根文件系统 |
| `just build-example` | 仅编译示例程序 |
| `just make-fit-image` | 生成FIT镜像 |

## 开发记录

主要开发工作（基于git log）：
- 完成TFLite模型在LS2K0300上的部署与实时推理
- 实现智能车A4纸检测+透视变换+分类的完整流程
- 调试PID参数，实现稳定巡线
- 修复TFLM静态库AddressSanitizer问题
- 配置GitHub Actions自动编译Release

## 原仓库

本仓库fork自: https://gitee.com/seekfree/LS2K0300_Library

原仓库有两个分支：
- `master` - LS2K0300核心板
- `2k0300_99pi_wifi` - 久久派

## License

GPL-3.0