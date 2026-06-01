cmake_minimum_required(VERSION 3.15)

get_filename_component(PROJECT_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

project(${PROJECT_NAME})

# 禁用 SSL
add_definitions(-DNO_SSL)

find_path(CIVETWEB_INCLUDE civetweb.h)
find_library(CIVETWEB_LIBRARY civetweb)
find_library(ZLIB_LIBRARY z)

if(NOT CIVETWEB_INCLUDE)
    message(FATAL_ERROR "Could not find civetweb include directory")
endif()
if(NOT CIVETWEB_LIBRARY)
    message(FATAL_ERROR "Could not find civetweb library")
endif()
if(NOT ZLIB_LIBRARY)
    message(FATAL_ERROR "Could not find zlib library")
endif()

add_executable(${PROJECT_NAME}
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE ${CIVETWEB_INCLUDE})

target_link_libraries(${PROJECT_NAME}
    zf::library
    ${OpenCV_LIBS}
    ${CIVETWEB_LIBRARY}
    ${ZLIB_LIBRARY}
    pthread
    dl
)

install(TARGETS ${PROJECT_NAME} DESTINATION .)