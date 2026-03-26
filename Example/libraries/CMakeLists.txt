cmake_minimum_required(VERSION 3.15)

set(ZF_LIBRARY_VERSION 1.0.0)
set(ZF_LIBRARY_DESCRIPTION "SeekFree LS2K0300 Driver Library")

add_library(zf_library INTERFACE)
add_library(zf::library ALIAS zf_library)

target_include_directories(zf_library INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/zf_common>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/zf_driver>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/zf_device>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/zf_components/seekfree_assistant>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/zf_components/ncnn/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/zf_components/tflm>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/zf_components/tflm/third_party/flatbuffers/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/zf_components/tflm/third_party/gemmlowp>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/zf_components/tflm/third_party/gemmlowp/fixedpoint>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/zf_components/tflm/third_party/ruy>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/zf_components/tflm/third_party/ruy/ruy>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/zf_components/tflm/tensorflow/lite/micro>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/zf_components/tflm/tensorflow/lite/micro/kernels>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/zf_components/tflm/tensorflow/lite/kernels/internal>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/zf_components/tflm/tensorflow/lite/core/api>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/zf_components/tflm/tensorflow/lite/core/c>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/zf_components/tflm/tensorflow/lite/schema>
)

target_link_directories(zf_library INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/zf_components/tflm/lib>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/zf_components/ncnn/lib>
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
    ${CMAKE_CURRENT_SOURCE_DIR}/zf_device/zf_device_uvc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/zf_components/seekfree_assistant/seekfree_assistant.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/zf_components/seekfree_assistant/seekfree_assistant_interface.cpp
)

target_link_libraries(zf_library INTERFACE
    pthread
    m
    tflm
    ncnn
)
