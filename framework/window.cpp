module;

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>
#include <memory>
#include <print>

export module framework:window;

namespace framework::glfw {
  export struct Deleter {
    void operator()(GLFWwindow *window) const noexcept {
      glfwDestroyWindow(window);
      glfwTerminate();
    }
  };

  export using Window = std::unique_ptr<GLFWwindow, Deleter>;

  /// Returns a valid Window if successful, else throws.
  export [[nodiscard]] auto create_window(glm::ivec2 size, char const *title)
    -> Window {
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

  export [[nodiscard]] auto instance_extensions()
    -> std::span<char const *const> {
    auto count = std::uint32_t{};
    auto const *extensions = glfwGetRequiredInstanceExtensions(&count);
    return {extensions, static_cast<std::size_t>(count)};
  }

  export [[nodiscard]] auto create_surface(
    GLFWwindow *window, vk::Instance instance
  ) -> vk::UniqueSurfaceKHR {
    auto *surface = VkSurfaceKHR{};

    auto const result =
      glfwCreateWindowSurface(instance, window, nullptr, &surface);
    if (result != VK_SUCCESS || surface == VkSurfaceKHR{}) {
      throw std::runtime_error{"Failed to create Vulkan Surface"};
    }

    return vk::UniqueSurfaceKHR{surface, instance};
  }

  export auto framebuffer_size(GLFWwindow *window) -> glm::ivec2 {
    auto size = glm::ivec2{};
    glfwGetFramebufferSize(window, &size.x, &size.y);

    return size;
  }
} // namespace framework::glfw
