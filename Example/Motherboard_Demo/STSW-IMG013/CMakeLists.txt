cmake_minimum_required(VERSION 3.15)

get_filename_component(PROJECT_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

project(${PROJECT_NAME})

option(POLLING "Enable polling mode" ON)

set(CORE_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/core/VL53L1X_api.c
    ${CMAKE_CURRENT_SOURCE_DIR}/core/VL53L1X_calibration.c
)

set(PLATFORM_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/vl53l1_linux_platform.c
)

add_executable(${PROJECT_NAME}
    ${CMAKE_CURRENT_SOURCE_DIR}/main.c
    ${CORE_SRC}
    ${PLATFORM_SRC}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/core
)

if(POLLING)
    target_compile_definitions(${PROJECT_NAME} PRIVATE POLLING)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE pthread)