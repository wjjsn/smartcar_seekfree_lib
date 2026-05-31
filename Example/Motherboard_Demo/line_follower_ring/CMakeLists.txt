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

target_link_libraries(${PROJECT_NAME}
    zf::library
    ${OpenCV_LIBS}
    find_line_lib
    template_schedule
)

install(TARGETS ${PROJECT_NAME} DESTINATION .)
