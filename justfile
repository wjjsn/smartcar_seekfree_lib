# 定义变量
ARCH := "loongarch"
CROSS_COMPILE := "/workspaces/buildroot-devcontainer/buildroot/output/host/bin/loongarch64-buildroot-linux-musl-"

BUILDROOT_PATH := "buildroot-2405"
LINUX_PATH := "ls2k0300_linux_4.19"

# 展示该帮助页面
default:
    just --list

# 构建内核
linux-build:
    cd {{LINUX_PATH}} && \
    ./build.sh

# 构建根文件系统
build-buildroot:
    cd {{BUILDROOT_PATH}} && \
    make olddefconfig && \
    make && \
    cp .config ../.config-buildroot

# 生成fit镜像
make-fit-image: linux-build build-buildroot
    mkdir -p build && \
    rm -f build/* && \
    cp {{LINUX_PATH}}/arch/loongarch/boot/vmlinux.bin ./build && \
    cp {{LINUX_PATH}}/arch/loongarch/boot/dts/*.dtb ./build && \
    mkimage -f multi.its build/image.itb
