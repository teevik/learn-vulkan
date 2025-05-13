module;

#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>
#include <print>

export module framework_module:swapchain;
import :gpu;

namespace {
  constexpr std::uint32_t min_images{3};

  constexpr auto srgb_formats = std::array{
    vk::Format::eR8G8B8A8Srgb,
    vk::Format::eB8G8R8A8Srgb,
  };

  // Color image with 1 layer and 1 mip-level (the default).
  constexpr auto subresource_range = [] {
    return vk::ImageSubresourceRange()
      .setAspectMask(vk::ImageAspectFlagBits::eColor)
      .setLayerCount(1)
      .setLevelCount(1);
  }();

  // Returns a SurfaceFormat with SrgbNonLinear color space and an sRGB format.
  [[nodiscard]] constexpr auto get_surface_format(
    std::span<vk::SurfaceFormatKHR const> supported
  ) -> vk::SurfaceFormatKHR {
    for (auto const desired : srgb_formats) {
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
      return std::max(min_images, capabilities.minImageCount);
    }
    return std::clamp(
      min_images, capabilities.minImageCount, capabilities.maxImageCount
    );
  }

  void require_success(vk::Result const result, char const *error_msg) {
    if (result != vk::Result::eSuccess) {
      throw std::runtime_error{error_msg};
    }
  }

  auto needs_recreation(vk::Result const result) -> bool {
    switch (result) {
      case vk::Result::eSuccess:
      case vk::Result::eSuboptimalKHR:
        return false;
      case vk::Result::eErrorOutOfDateKHR:
        return true;
      default:
        break;
    }

    throw std::runtime_error{"Swapchain Error"};
  }
} // namespace

namespace framework_module {
  struct RenderTarget {
    vk::Image image;
    vk::ImageView image_view;
    vk::Extent2D extent;
  };

  export class Swapchain {
  public:
    explicit Swapchain(
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

    auto recreate(glm::ivec2 size) -> bool {
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
        swapchain_create_info.minImageCount >= min_images
      );

      // Wait for the device to be idle before destroying the current swapchain.
      device.waitIdle();
      swapchain = device.createSwapchainKHRUnique(swapchain_create_info);
      image_index.reset();

      populate_images();
      create_image_views();

      size = get_size();
      std::println("[lvk] Swapchain [{}x{}]", size.x, size.y);
      return true;
    }

    [[nodiscard]]
    auto get_size() const -> glm::ivec2 {
      return {
        swapchain_create_info.imageExtent.width,
        swapchain_create_info.imageExtent.height
      };
    }

    [[nodiscard]]
    auto get_format() const -> vk::Format {
      return swapchain_create_info.imageFormat;
    }

    [[nodiscard]]
    auto acquire_next_image(vk::Semaphore const to_signal)
      -> std::optional<RenderTarget> {
      assert(!image_index);

      static constexpr auto timeout = std::numeric_limits<std::uint64_t>::max();
      // Avoid VulkanHPP ErrorOutOfDateKHR exceptions by using alternate API that
      // returns a Result.

      auto new_image_index = std::uint32_t{};
      auto const result = device.acquireNextImageKHR(
        *swapchain, timeout, to_signal, {}, &new_image_index
      );
      if (needs_recreation(result)) return {};

      image_index = static_cast<std::size_t>(new_image_index);

      return RenderTarget{
        .image = images.at(*image_index),
        .image_view = *image_views.at(*image_index),
        .extent = swapchain_create_info.imageExtent,
      };
    }

    [[nodiscard]]
    auto base_barrier() const -> vk::ImageMemoryBarrier2 {
      // fill up the parts common to all barriers.
      auto barrier = vk::ImageMemoryBarrier2()
                       .setImage(images.at(image_index.value()))
                       .setSubresourceRange(subresource_range)
                       .setSrcQueueFamilyIndex(gpu.queue_family)
                       .setDstQueueFamilyIndex(gpu.queue_family);

      return barrier;
    }

    [[nodiscard]]
    auto present(vk::Queue const queue, vk::Semaphore const to_wait) -> bool {
      auto const a_image_index =
        static_cast<std::uint32_t>(image_index.value());

      auto present_info = vk::PresentInfoKHR()
                            .setSwapchains(*swapchain)
                            .setImageIndices(a_image_index)
                            .setWaitSemaphores(to_wait);

      // avoid VulkanHPP ErrorOutOfDateKHR exceptions by using alternate API.
      auto const result = queue.presentKHR(&present_info);
      image_index.reset();

      return !needs_recreation(result);
    }

  private:
    void populate_images() {
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

    void create_image_views() {
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

    vk::Device device;
    Gpu gpu{};

    vk::SwapchainCreateInfoKHR swapchain_create_info;
    vk::UniqueSwapchainKHR swapchain;
    std::vector<vk::Image> images;
    std::vector<vk::UniqueImageView> image_views;
    std::optional<std::size_t> image_index;
  };
} // namespace framework_module
