cmake_minimum_required(VERSION 3.31)

project(learn-vk)

# set C++ options
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_COMPILER_LAUNCHER "ccache")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-builtin")

# set other CMake options
set(CMAKE_DEBUG_POSTFIX "-d")
set(BUILD_SHARED_LIBS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY $<1:${PROJECT_SOURCE_DIR}/bin>)

add_subdirectory(ext)
add_subdirectory(framework)

add_subdirectory(examples/triangle)
add_subdirectory(examples/quad)
