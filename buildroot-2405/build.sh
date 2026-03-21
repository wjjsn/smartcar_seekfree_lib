#!/bin/bash 
echo "执行自动编译"

export PATH=/opt/ls_2k0300_env/loongson-gnu-toolchain-8.3-x86_64-loongarch64-linux-gnu-rc1.6/bin:$PATH

make ARCH=loongarch64 CROSS_COMPILE=loongarch64-linux-gnu-  menuconfig

make ARCH=loongarch64 CROSS_COMPILE=loongarch64-linux-gnu- -j$(nproc) "$@"
