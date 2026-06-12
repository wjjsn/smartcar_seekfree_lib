cmake_minimum_required(VERSION 3.15)

# Civetweb 嵌入式 HTTP 服务器库 (v1.16)
# 源码源自 https://github.com/civetweb/civetweb (MIT License)
#
# 该库在 Example 工程内部独立构建,使用与 buildroot 包配置
# (package/civetweb/civetweb.mk) 一致的编译宏:
#   - NO_SSL       : 例程不依赖 OpenSSL
#   - WITH_IPV6=1  : 启用 IPv6
#   - HAVE_POSIX_FALLOCATE=0 : glibc 较新版本中 posix_fallocate 行为变化
#   - WITH_CPP=1   : 启用 C++ 封装 (CivetServer)
#   - WITH_ZLIB=1  : 启用 gzip
# 这些宏见 https://github.com/civetweb/civetweb/blob/master/docs/Building.md
# 以及 buildroot 内的 CIVETWEB_CONF_OPTS / CIVETWEB_COPT。

set(CIVETWEB_DIR ${CMAKE_CURRENT_SOURCE_DIR})

add_library(civetweb STATIC
    ${CIVETWEB_DIR}/src/civetweb.c
    ${CIVETWEB_DIR}/src/CivetServer.cpp
)

# 与 buildroot CIVETWEB_CONF_OPTS / CIVETWEB_COPT 保持一致
# (排除 server / lua / mbedtls / wolfssl 等不需要的模块)
target_compile_definitions(civetweb PUBLIC
    TARGET_OS_LINUX
    NO_SSL
    NO_SSL_DL
    WITH_IPV6=1
    WITH_ZLIB=1
    HAVE_POSIX_FALLOCATE=0
    WITH_CPP=1
    # 库源文件里包含一个 .inl 形式的 mod_lua,使用 USE_LUA 关闭
    # (civetweb.c 内默认就是关闭,这里显式声明以便审计)
)

# 公共头文件目录
target_include_directories(civetweb
    PUBLIC
        $<BUILD_INTERFACE:${CIVETWEB_DIR}/include>
        $<BUILD_INTERFACE:${CIVETWEB_DIR}/src>
)

# 库自身依赖 pthread / m / dl
# (与 buildroot CIVETWEB_LIBS = -lpthread -lm 一致)
target_link_libraries(civetweb
    PUBLIC
        pthread
        m
)

# 抑制上游源里大量非关键告警,保持编译输出干净。
# 注意:这里仅作用于本库的源文件,不影响调用方。
target_compile_options(civetweb PRIVATE
    -Wno-unused-parameter
    -Wno-sign-compare
    -Wno-unused-function
    -Wno-missing-field-initializers
    -Wno-strict-aliasing
)
