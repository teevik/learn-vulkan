#include "window.h"
#include <print>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

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
    if (glfwInit() != GLFW_TRUE)
      throw std::runtime_error{"Failed to initialize GLFW"};

    // check for Vulkan support.
    if (glfwVulkanSupported() != GLFW_TRUE)
      throw std::runtime_error{"Vulkan not supported"};

    auto window = Window{};

    // Tell GLFW that we don't want an OpenGL context.
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window.reset(glfwCreateWindow(size.x, size.y, title, nullptr, nullptr));
    if (!window) throw std::runtime_error{"Failed to create GLFW Window"};

    return window;
  }

  auto instance_extensions() -> std::span<char const *const> {
    auto count = std::uint32_t{};
    auto const *extensions = glfwGetRequiredInstanceExtensions(&count);
    return {extensions, static_cast<std::size_t>(count)};
  }

  auto create_surface(GLFWwindow *window, vk::Instance const instance)
    -> vk::UniqueSurfaceKHR {
    auto *surface = VkSurfaceKHR{};

    auto const result =
      glfwCreateWindowSurface(instance, window, nullptr, &surface);
    if (result != VK_SUCCESS || surface == VkSurfaceKHR{}) {
      throw std::runtime_error{"Failed to create Vulkan Surface"};
    }

    return vk::UniqueSurfaceKHR{surface, instance};
  }

  auto framebuffer_size(GLFWwindow *window) -> glm::ivec2 {
    auto size = glm::ivec2{};
    glfwGetFramebufferSize(window, &size.x, &size.y);

    return size;
  }
} // namespace lvk::glfw
