cmake_minimum_required(VERSION 3.15)

get_filename_component(PROJECT_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
project(${PROJECT_NAME})

add_compile_options(-Ofast)

# find_line_lib_web 已在 libraries/CMakeLists.txt 中定义，直接 link 即可

add_executable(${PROJECT_NAME}
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    SMTC2GO_DEBUG 
    SMTC2GO_DEBUG_LOG 
    SMTC2GO_DEBUG_IMSHOW
    SMTC2GO_USE_WEB_IMSHOW
)

target_link_libraries(${PROJECT_NAME}
    find_line_lib
    -Wl,-Bstatic -lstdc++ -lgcc -Wl,-Bdynamic
    dl
)

install(TARGETS ${PROJECT_NAME} DESTINATION .)
