#pragma once

#include <GLFW/glfw3.h>
#include <vulkan/vulkan_handles.hpp>
#include <memory>

namespace lvk::glfw {
  struct Deleter {
    void operator()(GLFWwindow *window) const noexcept;
  };

  using Window = std::unique_ptr<GLFWwindow, Deleter>;

  /// Returns a valid Window if successful, else throws.
  [[nodiscard]]
  auto create_window(glm::ivec2 size, char const *title) -> Window;

  [[nodiscard]]
  auto instance_extensions() -> std::span<char const *const>;

  auto create_surface(GLFWwindow *window, vk::Instance instance)
    -> vk::UniqueSurfaceKHR;

  auto framebuffer_size(GLFWwindow *window) -> glm::ivec2;
} // namespace lvk::glfw
