project(3-quad_new)

add_executable(${PROJECT_NAME} main.cpp)

target_link_libraries(${PROJECT_NAME}
    learn-vk::ext
    learn-vk::framework
)
