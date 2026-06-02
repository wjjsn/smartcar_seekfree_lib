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

# 优先查找静态库 .a
set(_OLD_FIND_LIBRARY_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES})
set(CMAKE_FIND_LIBRARY_SUFFIXES .a)
find_library(CIVETWEB_STATIC_LIBRARY NAMES civetweb)
find_library(ZLIB_STATIC_LIBRARY NAMES z)
set(CMAKE_FIND_LIBRARY_SUFFIXES ${_OLD_FIND_LIBRARY_SUFFIXES})

if(NOT CIVETWEB_STATIC_LIBRARY)
    set(CIVETWEB_STATIC_LIBRARY ${CIVETWEB_LIBRARY})
    message(WARNING "civetweb static library not found, falling back to shared library: ${CIVETWEB_STATIC_LIBRARY}")
endif()
if(NOT ZLIB_STATIC_LIBRARY)
    set(ZLIB_STATIC_LIBRARY ${ZLIB_LIBRARY})
    message(WARNING "zlib static library not found, falling back to shared library: ${ZLIB_STATIC_LIBRARY}")
endif()

add_executable(${PROJECT_NAME}
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE ${CIVETWEB_INCLUDE})

# 静态链接 civetweb, zlib; 仅把 libstdc++/libgcc 静态链接，pthread 和 dl 保持动态
# 因为 libpthread.a 的 init 依赖 ld-linux 提供的 _dl_* 符号。
target_link_libraries(${PROJECT_NAME}
    # zf::library
    ${OpenCV_LIBS}
    ${CIVETWEB_STATIC_LIBRARY}
    ${ZLIB_STATIC_LIBRARY}
    -Wl,-Bstatic -lstdc++ -lgcc -Wl,-Bdynamic
    pthread
    dl
)

install(TARGETS ${PROJECT_NAME} DESTINATION .)