#include "window.h"
#include <print>

namespace lvk::glfw {
  void Deleter::operator()(GLFWwindow *window) const noexcept {
    glfwDestroyWindow(window);
    glfwTerminate();
  }

  auto create_window(glm::ivec2 const size, char const *title) -> Window {
    static auto const on_error = [](int const code, char const *description) {
      std::println(stderr, "[GLFW] Error {}: {}", code, description);
    };

    glfwSetErrorCallback(on_error);
    if (glfwInit() != GLFW_TRUE) {
      throw std::runtime_error{"Failed to initialize GLFW"};
    }

    // check for Vulkan support.
    if (glfwVulkanSupported() != GLFW_TRUE) {
      throw std::runtime_error{"Vulkan not supported"};
    }

    auto window = Window{};
    // tell GLFW that we don't want an OpenGL context.
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window.reset(glfwCreateWindow(size.x, size.y, title, nullptr, nullptr));
    if (!window) {
      throw std::runtime_error{"Failed to create GLFW Window"};
    }
    return window;
  }
}
