cmake_minimum_required(VERSION 3.15)

get_filename_component(PROJECT_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
project(${PROJECT_NAME})

# 嵌入式 HTTP 库 (civetweb v1.16) 已集成进 libraries/civetweb,
# 在 libraries/CMakeLists.txt 中 add_subdirectory 注册。
# 这里直接 link,不再走外部 find_library。

add_executable(${PROJECT_NAME}
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)

# 静态链接 civetweb;仅把 libstdc++/libgcc 静态链接,pthread / dl 保持动态
# 因为 libpthread.a 的 init 依赖 ld-linux 提供的 _dl_* 符号。
# pthread / m 由 civetweb PUBLIC 传递性带入,dl 单独声明。
target_link_libraries(${PROJECT_NAME}
    # zf::library
    ${OpenCV_LIBS}
    civetweb
    -Wl,-Bstatic -lstdc++ -lgcc -Wl,-Bdynamic
    dl
)

install(TARGETS ${PROJECT_NAME} DESTINATION .)