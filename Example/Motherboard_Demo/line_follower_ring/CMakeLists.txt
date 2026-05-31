cmake_minimum_required(VERSION 3.15)

get_filename_component(PROJECT_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

project(${PROJECT_NAME})

add_compile_options(-Ofast -flto)
add_link_options(-flto)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/smtcgo2)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/cpp_template_schedule)

add_executable(${PROJECT_NAME}
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)

add_executable(pid_debug
    ${CMAKE_CURRENT_SOURCE_DIR}/pid_debug.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cpp_template_schedule/schedule.hpp
)

# 只链接需要的驱动文件，避免链接整个 zf_library（包含需要 OpenCV的 zf_device）
target_sources(pid_debug PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libraries/zf_driver/zf_driver_encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libraries/zf_driver/zf_driver_gpio.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libraries/zf_driver/zf_driver_pwm.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libraries/zf_driver/zf_driver_file.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libraries/zf_common/zf_common_function.cpp
)

target_include_directories(pid_debug PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libraries/zf_driver
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libraries/zf_common
    ${CMAKE_CURRENT_SOURCE_DIR}/cpp_template_schedule
)

target_link_libraries(pid_debug PRIVATE
    pthread
)

target_link_libraries(${PROJECT_NAME}
    zf::library
    ${OpenCV_LIBS}
    find_line_lib
    template_schedule
)

install(TARGETS ${PROJECT_NAME} DESTINATION .)
