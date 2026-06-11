# cross.cmake文件
# 设置为1则表示交叉编译，设置为0则表示x86 gcc编译
SET(CROSS_COMPILE 1)

IF(CROSS_COMPILE)
SET(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR loongson)
SET(TOOLCHAIN_DIR "/opt/ls_2k0300_env/loongson-gnu-toolchain-8.3-x86_64-loongarch64-linux-gnu-rc1.6")
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_DIR}/bin/loongarch64-linux-gnu-g++)
set(CMAKE_C_COMPILER ${TOOLCHAIN_DIR}/bin/loongarch64-linux-gnu-gcc)
set(CMAKE_AR      ${TOOLCHAIN_DIR}/bin/loongarch64-linux-gnu-gcc-ar)
set(CMAKE_RANLIB  ${TOOLCHAIN_DIR}/bin/loongarch64-linux-gnu-gcc-ranlib)

# sysroot for the cross-toolchain
SET(CMAKE_SYSROOT "${TOOLCHAIN_DIR}/loongarch64-linux-gnu/sysroot" CACHE INTERNAL "")

# Search both toolchain sysroot and Buildroot target sysroot for headers/libraries/packages
SET(CMAKE_FIND_ROOT_PATH 
    ${TOOLCHAIN_DIR} 
    ${CMAKE_SYSROOT} 
    "/workspace/buildroot-2405/output/host/loongarch64-buildroot-linux-gnu/sysroot"
    "/workspace/ls_2k0300_env/opencv_4_10_build"
    )
SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g3 -rdynamic -pthread -Wall -Wno-delete-non-virtual-dtor -Wno-placement-new")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=loongarch64 -mtune=loongarch64 -fPIC -ffunction-sections -fdata-sections")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g3 -rdynamic -Wall -march=loongarch64 -mtune=loongarch64 -fPIC -ffunction-sections -fdata-sections")
SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gc-sections")
find_package(OpenCV REQUIRED)
ENDIF(CROSS_COMPILE)
