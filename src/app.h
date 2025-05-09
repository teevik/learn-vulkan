#pragma once

#include "gpu.h"
#include "scoped_waiter.hpp"
#include "swapchain.h"
#include "window.h"
#include <vulkan/vulkan_handles.hpp>

namespace lvk {
  class App {
  private:
    glfw::Window window;
    vk::UniqueInstance instance;
    vk::UniqueSurfaceKHR surface;
    Gpu gpu{};
    vk::UniqueDevice device;
    std::optional<Swapchain> swapchain;
    vk::Queue queue;

    ScopedWaiter waiter;

    void create_window();
    void create_instance();
    void create_surface();
    void select_gpu();
    void create_device();
    void create_swapchain();

    void main_loop();

  public:
    void run();
  };
} // namespace lvk
