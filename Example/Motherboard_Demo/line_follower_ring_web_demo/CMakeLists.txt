cmake_minimum_required(VERSION 3.15)

get_filename_component(PROJECT_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
project(${PROJECT_NAME})

# 禁用 SSL（civetweb 可选项；与 web_camera_demo 保持一致）
add_definitions(-DNO_SSL)

# 直接在本工程中构建 find_line_lib 静态库：
#   该库同时被 line_follower_ring 主工程使用；如果在多个工程里
#   add_subdirectory 同一份 smtcgo2/CMakeLists.txt，CMake 会因
#   binary dir 重复而报错。改为在每个使用方工程里独立构建更安全。
set(FIND_LINE_LIB_SRC_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../line_follower_ring/smtcgo2"
    CACHE PATH "Path to smtcgo2 (find_line_lib) source directory")

if(NOT EXISTS "${FIND_LINE_LIB_SRC_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "find_line_lib source dir not found at ${FIND_LINE_LIB_SRC_DIR}. "
        "请确认 line_follower_ring/smtcgo2 目录存在。")
endif()

add_library(find_line_lib_web STATIC
    ${FIND_LINE_LIB_SRC_DIR}/src/find_line_lib/hello.cpp
    ${FIND_LINE_LIB_SRC_DIR}/src/find_line_lib/get_start_point.cpp
    ${FIND_LINE_LIB_SRC_DIR}/src/find_line_lib/ring.cpp
    ${FIND_LINE_LIB_SRC_DIR}/src/find_line_lib/targetboard.cpp
    ${FIND_LINE_LIB_SRC_DIR}/src/find_line_lib/check_roadblock.cpp
    ${FIND_LINE_LIB_SRC_DIR}/src/find_line_lib/calculate_wheel_speeds.cpp
    ${FIND_LINE_LIB_SRC_DIR}/src/find_line_lib/skeleton.cpp
)

# 与 smtcgo2/CMakeLists.txt 保持一致：开启调试宏，让库画出
# 紫色 legacy_target / 黄色 folded_target 标注并填充 debug_image。
# 注意：库目标名取 find_line_lib_web，避免与
# line_follower_ring 主工程里的 find_line_lib 静态库冲突。
target_compile_definitions(find_line_lib_web PRIVATE
    -DSMTC2GO_DEBUG
    -DSMTC2GO_DEBUG_LOG
)

target_include_directories(find_line_lib_web
    PUBLIC ${FIND_LINE_LIB_SRC_DIR}/include
)

target_link_libraries(find_line_lib_web
    PUBLIC ${OpenCV_LIBS}
    PUBLIC stdc++fs
)

# civetweb + zlib（与 web_camera_demo 一致：优先静态，回退动态）
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

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CIVETWEB_INCLUDE}
)

target_link_libraries(${PROJECT_NAME}
    find_line_lib_web
    ${CIVETWEB_STATIC_LIBRARY}
    ${ZLIB_STATIC_LIBRARY}
    # libstdc++/libgcc 静态链接，pthread / dl 保持动态
    -Wl,-Bstatic -lstdc++ -lgcc -Wl,-Bdynamic
    pthread
    dl
)

install(TARGETS ${PROJECT_NAME} DESTINATION .)
