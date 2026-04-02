BUILDROOT_PATH := "buildroot-2405"
LINUX_PATH := "ls2k0300_linux_4.19"

RED:='\033[0;31m'
GREEN:='\033[0;32m'
NC:='\033[0m' # 无颜色 (No Color)

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
    ./build.sh 1>/dev/null

build-example:
    cd Example && \
    cmake -G Ninja -B build -DCMAKE_TOOLCHAIN_FILE=cross.cmake -DCMAKE_INSTALL_PREFIX=/workspaces/seekfree/buildroot-2405/output/target/home/root && \
    cd build && \
    ninja all

install-example: build-buildroot build-example
    ninja install -C Example/build 1>/dev/null && \
    echo '{{GREEN}}install done{{NC}}' && \
    just build-buildroot


# 生成fit镜像
make-fit-image: linux-build
    mkdir -p build && \
    rm -f build/* && \
    cp {{LINUX_PATH}}/arch/loongarch/boot/compressed/vmlinux.bin ./build && \
    mkimage -f multi.its build/image.itb

all:
    just install-example
    cp buildroot-2405/output/images/* ./build/
    just make-fit-image

