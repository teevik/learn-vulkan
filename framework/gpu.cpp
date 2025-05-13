module;

#include <vulkan/vulkan.hpp>
#include <algorithm>
#include <ranges>

export module framework:gpu;

namespace framework {
  export constexpr auto vk_version = VK_MAKE_VERSION(1, 3, 0);

  export struct Gpu {
    vk::PhysicalDevice device;
    vk::PhysicalDeviceProperties properties;
    vk::PhysicalDeviceFeatures features;
    uint32_t queue_family{};
  };

  export [[nodiscard]] auto get_suitable_gpu(
    vk::Instance instance, vk::SurfaceKHR surface
  ) -> Gpu {
    auto const supports_swapchain = [](Gpu const &gpu) {
      static constexpr std::string_view name_v =
        VK_KHR_SWAPCHAIN_EXTENSION_NAME;
      static constexpr auto is_swapchain =
        [](vk::ExtensionProperties const &properties) {
          return properties.extensionName.data() == name_v;
        };
      auto const properties = gpu.device.enumerateDeviceExtensionProperties();
      auto const it = std::ranges::find_if(properties, is_swapchain);
      return it != properties.end();
    };

    auto const set_queue_family = [](Gpu &out_gpu) {
      static constexpr auto queue_flags_v =
        vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eTransfer;
      for (auto const [index, family] :
           std::views::enumerate(out_gpu.device.getQueueFamilyProperties())) {
        if ((family.queueFlags & queue_flags_v) == queue_flags_v) {
          out_gpu.queue_family = static_cast<std::uint32_t>(index);
          return true;
        }
      }
      return false;
    };

    auto const can_present = [surface](Gpu const &gpu) {
      return gpu.device.getSurfaceSupportKHR(gpu.queue_family, surface) ==
        vk::True;
    };

    std::optional<Gpu> fallback;

    for (auto const &device : instance.enumeratePhysicalDevices()) {
      auto gpu = Gpu{
        .device = device,
        .properties = device.getProperties(),
        .features = device.getFeatures()
      };

      if (gpu.properties.apiVersion < vk_version) continue;
      if (!supports_swapchain(gpu)) continue;
      if (!set_queue_family(gpu)) continue;
      if (!can_present(gpu)) continue;

      if (gpu.properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
        return gpu;
      }

      fallback = gpu;
    }

    if (fallback) return fallback.value();

    throw std::runtime_error{"No suitable Vulkan Physical Devices"};
  }
} // namespace framework
