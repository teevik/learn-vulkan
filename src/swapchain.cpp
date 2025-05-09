#include "swapchain.h"
#include <vulkan/vulkan_structs.hpp>
#include <print>

namespace {
  constexpr auto srgb_formats_v = std::array{
    vk::Format::eR8G8B8A8Srgb,
    vk::Format::eB8G8R8A8Srgb,
  };

  // returns a SurfaceFormat with SrgbNonLinear color space and an sRGB format.
  [[nodiscard]] constexpr auto get_surface_format(
    std::span<vk::SurfaceFormatKHR const> supported
  ) -> vk::SurfaceFormatKHR {
    for (auto const desired : srgb_formats_v) {
      auto const is_match = [desired](vk::SurfaceFormatKHR const &in) {
        return in.format == desired &&
          in.colorSpace == vk::ColorSpaceKHR::eVkColorspaceSrgbNonlinear;
      };
      auto const it = std::ranges::find_if(supported, is_match);
      if (it == supported.end()) {
        continue;
      }
      return *it;
    }
    return supported.front();
  }

  constexpr std::uint32_t min_images_v{3};

  // returns currentExtent if specified, else clamped size.
  [[nodiscard]] constexpr auto get_image_extent(
    vk::SurfaceCapabilitiesKHR const &capabilities, glm::uvec2 const size
  ) -> vk::Extent2D {
    constexpr auto limitless_v = 0xffffffff;
    if (capabilities.currentExtent.width < limitless_v &&
        capabilities.currentExtent.height < limitless_v) {
      return capabilities.currentExtent;
    }
    auto const x = std::clamp(
      size.x,
      capabilities.minImageExtent.width,
      capabilities.maxImageExtent.width
    );
    auto const y = std::clamp(
      size.y,
      capabilities.minImageExtent.height,
      capabilities.maxImageExtent.height
    );
    return vk::Extent2D{x, y};
  }

  [[nodiscard]] constexpr auto get_image_count(
    vk::SurfaceCapabilitiesKHR const &capabilities
  ) -> std::uint32_t {
    if (capabilities.maxImageCount < capabilities.minImageCount) {
      return std::max(min_images_v, capabilities.minImageCount);
    }
    return std::clamp(
      min_images_v, capabilities.minImageCount, capabilities.maxImageCount
    );
  }

  void require_success(vk::Result const result, char const *error_msg) {
    if (result != vk::Result::eSuccess) {
      throw std::runtime_error{error_msg};
    }
  }
} // namespace

namespace lvk {
  Swapchain::Swapchain(
    vk::Device const device,
    Gpu const &gpu,
    vk::SurfaceKHR const surface,
    glm::ivec2 const size
  ) : device(device), gpu(gpu) {
    auto const surface_format =
      get_surface_format(gpu.device.getSurfaceFormatsKHR(surface));

    swapchain_create_info =
      vk::SwapchainCreateInfoKHR()
        .setSurface(surface)
        .setImageFormat(surface_format.format)
        .setImageColorSpace(surface_format.colorSpace)
        .setImageArrayLayers(1)
        // Swapchain images will be used as color attachments (render targets).
        .setImageUsage(vk::ImageUsageFlagBits::eColorAttachment)
        // eFifo is guaranteed to be supported.
        .setPresentMode(vk::PresentModeKHR::eFifo);

    if (!recreate(size)) {
      throw std::runtime_error{"Failed to create Vulkan Swapchain"};
    }
  }

  auto Swapchain::recreate(glm::ivec2 size) -> bool {
    // Image sizes must be positive.
    if (size.x <= 0 || size.y <= 0) {
      return false;
    }

    auto const capabilities =
      gpu.device.getSurfaceCapabilitiesKHR(swapchain_create_info.surface);

    swapchain_create_info.setImageExtent(get_image_extent(capabilities, size))
      .setMinImageCount(get_image_count(capabilities))
      .setOldSwapchain(swapchain ? *swapchain : vk::SwapchainKHR{})
      .setQueueFamilyIndices(gpu.queue_family);

    assert(
      swapchain_create_info.imageExtent.width > 0 &&
      swapchain_create_info.imageExtent.height > 0 &&
      swapchain_create_info.minImageCount >= min_images_v
    );

    // Wait for the device to be idle before destroying the current swapchain.
    device.waitIdle();
    swapchain = device.createSwapchainKHRUnique(swapchain_create_info);

    populate_images();
    create_image_views();

    size = get_size();
    std::println("[lvk] Swapchain [{}x{}]", size.x, size.y);
    return true;
  }

  auto Swapchain::get_size() const -> glm::ivec2 {
    return {
      swapchain_create_info.imageExtent.width,
      swapchain_create_info.imageExtent.height
    };
  }

  void Swapchain::populate_images() {
    // we use the more verbose two-call API to avoid assigning `images` to a new
    // vector on every call.
    auto image_count = std::uint32_t{};
    auto result =
      device.getSwapchainImagesKHR(*swapchain, &image_count, nullptr);
    require_success(result, "Failed to get Swapchain Images");

    images.resize(image_count);
    result =
      device.getSwapchainImagesKHR(*swapchain, &image_count, images.data());
    require_success(result, "Failed to get Swapchain Images");
  }

  void Swapchain::create_image_views() {
    // this is a color image with 1 layer and 1 mip-level (the default).
    auto subresource_range = vk::ImageSubresourceRange()
                               .setAspectMask(vk::ImageAspectFlagBits::eColor)
                               .setLayerCount(1)
                               .setLevelCount(1);

    // set common parameters here (everything except the Image).
    auto image_view_info = vk::ImageViewCreateInfo()
                             .setViewType(vk::ImageViewType::e2D)
                             .setFormat(swapchain_create_info.imageFormat)
                             .setSubresourceRange(subresource_range);

    image_views.clear();
    image_views.reserve(images.size());

    for (auto const image : images) {
      image_view_info.setImage(image);
      image_views.push_back(device.createImageViewUnique(image_view_info));
    }
  }
} // namespace lvk
