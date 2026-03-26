cmake_minimum_required(VERSION 3.15)

get_filename_component(PROJECT_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

project(${PROJECT_NAME})

add_executable(${PROJECT_NAME}
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)

target_link_libraries(${PROJECT_NAME}
    zf::library
    ${OpenCV_LIBS}
)

install(TARGETS ${PROJECT_NAME} DESTINATION .)
