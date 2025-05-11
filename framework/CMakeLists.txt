project(framework)

# add all source files target
file(GLOB_RECURSE sources LIST_DIRECTORIES false "*.cpp")
add_library(${PROJECT_NAME}
  ${sources}
)
add_library(learn-vk::framework ALIAS ${PROJECT_NAME})


# Setup precompiled header
target_precompile_headers(${PROJECT_NAME} PUBLIC
  <glm/glm.hpp>
  <vulkan/vulkan.hpp>
)

# Add current directory as include directory
target_include_directories(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/..)

target_link_libraries(${PROJECT_NAME} learn-vk::ext)
