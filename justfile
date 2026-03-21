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
    ./build.sh

build-example:
    cd Example && \
    ./replace_ip.sh && \
    ./build_all.sh

install-example: build-buildroot build-example
    find Example -type f -executable ! -name "*.*" -exec cp {} buildroot-2405/output/target/home/root \; && \
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

