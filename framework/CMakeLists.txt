project(framework)

add_library(${PROJECT_NAME})
add_library(learn-vk::framework ALIAS ${PROJECT_NAME})

file(GLOB_RECURSE sources LIST_DIRECTORIES false "*.cpp")
target_sources(${PROJECT_NAME}
    PUBLIC
    FILE_SET CXX_MODULES FILES
        ${sources}
)

target_link_libraries(${PROJECT_NAME} learn-vk::ext)
