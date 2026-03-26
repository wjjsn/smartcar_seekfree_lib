cmake_minimum_required(VERSION 3.15)
project(SeekFree_LS2K0300_Examples VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)

set(CMAKE_PREFIX_PATH "/opt/ls_2k0300_env/opencv_4_10_build" CACHE PATH "OpenCV installation prefix")
find_package(OpenCV REQUIRED)

include(${CMAKE_CURRENT_SOURCE_DIR}/cross.cmake)

add_subdirectory(libraries)

add_subdirectory(Coreboard_Demo/E07_pit_demo)
add_subdirectory(Coreboard_Demo/E08_flash_demo)

add_subdirectory(Motherboard_Demo/E01_01_button_switch_buzzer_demo)
add_subdirectory(Motherboard_Demo/E01_02_beep_demo)
add_subdirectory(Motherboard_Demo/E02_01_drv8701e_double_motor_control_demo)
add_subdirectory(Motherboard_Demo/E02_02_servo_control_demo)
add_subdirectory(Motherboard_Demo/E02_03_brushless_electric_regulating_demo)
add_subdirectory(Motherboard_Demo/E03_01_encoder_dir_demo)
add_subdirectory(Motherboard_Demo/E04_01_adc_battery_volatage_detection_demo)
add_subdirectory(Motherboard_Demo/E05_01_imu_demo)
add_subdirectory(Motherboard_Demo/E06_01_ips200_display_demo)
add_subdirectory(Motherboard_Demo/E07_01_udp_demo)
add_subdirectory(Motherboard_Demo/E07_02_tcp_client_demo)
add_subdirectory(Motherboard_Demo/E07_03_tcp_client_oscilloscope_demo)
add_subdirectory(Motherboard_Demo/E07_04_tcp_client_uvc_camera_demo)
add_subdirectory(Motherboard_Demo/E08_01_uvc_cmera_ips200_display_demo)
add_subdirectory(Motherboard_Demo/E09_01_tflite_picture_classic)
add_subdirectory(Motherboard_Demo/E09_02_tflite_camera_classic)
add_subdirectory(Motherboard_Demo/opencv_check)

execute_process(
    COMMAND ${CMAKE_COMMAND} -E create_symlink
    ${CMAKE_BINARY_DIR}/compile_commands.json
    ${CMAKE_CURRENT_SOURCE_DIR}/compile_commands.json
)
