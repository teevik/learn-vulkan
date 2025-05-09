#pragma once

#include "gpu.h"
namespace lvk {
  struct RenderTarget {
    vk::Image image;
    vk::ImageView image_view;
    vk::Extent2D extent;
  };

  class Swapchain {
  public:
    explicit Swapchain(
      vk::Device device, Gpu const &gpu, vk::SurfaceKHR surface, glm::ivec2 size
    );

    auto recreate(glm::ivec2 size) -> bool;

    [[nodiscard]]
    auto get_size() const -> glm::ivec2;

    [[nodiscard]]
    auto acquire_next_image(vk::Semaphore to_signal)
      -> std::optional<RenderTarget>;

    [[nodiscard]]
    auto base_barrier() const -> vk::ImageMemoryBarrier2;

    [[nodiscard]]
    auto present(vk::Queue queue, vk::Semaphore to_wait) -> bool;

  private:
    void populate_images();
    void create_image_views();

    vk::Device device;
    Gpu gpu{};

    vk::SwapchainCreateInfoKHR swapchain_create_info;
    vk::UniqueSwapchainKHR swapchain;
    std::vector<vk::Image> images;
    std::vector<vk::UniqueImageView> image_views;
    std::optional<std::size_t> image_index;
  };
} // namespace lvk
