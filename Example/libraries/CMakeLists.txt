cmake_minimum_required(VERSION 3.15)

set(ZF_LIBRARY_VERSION 1.0.0)
set(ZF_LIBRARY_DESCRIPTION "SeekFree LS2K0300 Driver Library")

add_library(zf_library INTERFACE)
add_library(zf::library ALIAS zf_library)

# 嵌入式 HTTP 库 civetweb (v1.16, MIT License)
# 独立构建为目标 zf::civetweb,需要时由调用方显式 link
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/civetweb)

target_include_directories(zf_library INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/zf_common>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/zf_driver>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/zf_device>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/zf_components/seekfree_assistant>
    # $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/zf_components/ncnn/include>
    # $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/zf_components/tflm>
    # $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/zf_components/tflm/third_party/flatbuffers/include>
    # $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/zf_components/tflm/third_party/gemmlowp>
    # $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/zf_components/tflm/third_party/gemmlowp/fixedpoint>
    # $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/zf_components/tflm/third_party/ruy>
    # $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/zf_components/tflm/third_party/ruy/ruy>
    # $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/zf_components/tflm/tensorflow/lite/micro>
    # $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/zf_components/tflm/tensorflow/lite/micro/kernels>
    # $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/zf_components/tflm/tensorflow/lite/kernels/internal>
    # $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/zf_components/tflm/tensorflow/lite/core/api>
    # $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/zf_components/tflm/tensorflow/lite/core/c>
    # $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/zf_components/tflm/tensorflow/lite/schema>
)

target_link_directories(zf_library INTERFACE
    # $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/zf_components/tflm/lib>
    # $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/zf_components/ncnn/lib>
)

target_sources(zf_library INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/zf_common/zf_common_fifo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/zf_common/zf_common_font.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/zf_common/zf_common_function.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/zf_driver/zf_driver_adc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/zf_driver/zf_driver_delay.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/zf_driver/zf_driver_encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/zf_driver/zf_driver_file.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/zf_driver/zf_driver_gpio.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/zf_driver/zf_driver_pit.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/zf_driver/zf_driver_pwm.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/zf_driver/zf_driver_tcp_client.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/zf_driver/zf_driver_udp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/zf_device/zf_device_imu_core.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/zf_device/zf_device_imu660ra.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/zf_device/zf_device_imu660rb.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/zf_device/zf_device_imu963ra.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/zf_device/zf_device_ips200_fb.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/zf_components/seekfree_assistant/seekfree_assistant.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/zf_components/seekfree_assistant/seekfree_assistant_interface.cpp
)

target_link_libraries(zf_library INTERFACE
    pthread
    m
    # tflm
    # ncnn
)

# ==================== find_line_lib (INTERFACE) ====================
# 默认不开启调试，各可执行文件通过 target_compile_definitions 自行开启
set(_SMTCGO2_DIR ${CMAKE_CURRENT_SOURCE_DIR}/smtcgo2)

add_library(find_line_lib INTERFACE)
add_library(find_line_lib::find_line_lib ALIAS find_line_lib)

target_sources(find_line_lib INTERFACE
    ${_SMTCGO2_DIR}/src/find_line_lib/hello.cpp
    ${_SMTCGO2_DIR}/src/find_line_lib/get_start_point.cpp
    ${_SMTCGO2_DIR}/src/find_line_lib/ring.cpp
    ${_SMTCGO2_DIR}/src/find_line_lib/targetboard.cpp
    ${_SMTCGO2_DIR}/src/find_line_lib/check_roadblock.cpp
    ${_SMTCGO2_DIR}/src/find_line_lib/calculate_wheel_speeds.cpp
    ${_SMTCGO2_DIR}/src/find_line_lib/skeleton.cpp
    ${_SMTCGO2_DIR}/src/find_line_lib/thinning.cpp
    ${_SMTCGO2_DIR}/src/find_line_lib/web_imshow.cpp
)

target_include_directories(find_line_lib INTERFACE ${_SMTCGO2_DIR}/include)

target_link_libraries(find_line_lib INTERFACE ${OpenCV_LIBS} civetweb)

if (NOT MSVC)
    target_link_libraries(find_line_lib INTERFACE stdc++fs)
endif()

# ==================== template_schedule ====================
add_library(template_schedule INTERFACE)
target_include_directories(template_schedule INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/cpp_template_schedule
)
