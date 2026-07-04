cmake_minimum_required(VERSION 3.16)
project(schedule VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(template_schedule INTERFACE)
target_include_directories(template_schedule INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})