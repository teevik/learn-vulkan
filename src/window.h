#pragma once

#include <GLFW/glfw3.h>
#include <memory>

namespace lvk::glfw {
  struct Deleter {
    void operator()(GLFWwindow *window) const noexcept;
  };

  using Window = std::unique_ptr<GLFWwindow, Deleter>;

  /// Returns a valid Window if successful, else throws.
  [[nodiscard]] auto create_window(glm::ivec2 size, char const *title)
    -> Window;
}
