#pragma once

#include "gpu.h"
namespace lvk {
  class Swapchain {
  public:
    explicit Swapchain(
      vk::Device device, Gpu const &gpu, vk::SurfaceKHR surface, glm::ivec2 size
    );

    auto recreate(glm::ivec2 size) -> bool;

    [[nodiscard]] auto get_size() const -> glm::ivec2;

  private:
    void populate_images();
    void create_image_views();

    vk::Device device;
    Gpu gpu{};

    vk::SwapchainCreateInfoKHR swapchain_create_info;
    vk::UniqueSwapchainKHR swapchain;
    std::vector<vk::Image> images;
    std::vector<vk::UniqueImageView> image_views;
  };
} // namespace lvk
