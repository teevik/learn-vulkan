#include "app.h"
#include "gpu.h"
#include "window.h"
#include <vulkan/vulkan_structs.hpp>
#include <print>

namespace lvk {
  void App::run() {
    create_window();
    create_instance();
    create_surface();
    select_gpu();
    create_device();

    main_loop();
  }

  void App::create_window() {
    window = glfw::create_window({1280, 720}, "Learn Vulkan");
  }

  void App::create_instance() {
    // auto const loader_version = vk::enumerateInstanceVersion();

    auto app_info = vk::ApplicationInfo()
                      .setPApplicationName("Learn Vulkan")
                      .setApiVersion(vk_version);

    auto const extensions = glfw::instance_extensions();
    auto instance_info = vk::InstanceCreateInfo()
                           .setPApplicationInfo(&app_info)
                           .setPEnabledExtensionNames(extensions);

    instance = vk::createInstanceUnique(instance_info);
  }

  void App::create_surface() {
    surface = glfw::create_surface(window.get(), instance.get());
  }

  void App::select_gpu() {
    gpu = get_suitable_gpu(*instance, *surface);

    std::println("Using GPU: {}", std::string_view{gpu.properties.deviceName});
  }

  void App::create_device() {
    // since we use only one queue, it has the entire priority range, ie, 1.0
    static constexpr auto queue_priorities = std::array{1.0f};

    auto queue_ci = vk::DeviceQueueCreateInfo()
                      .setQueueFamilyIndex(gpu.queue_family)
                      .setQueueCount(1)
                      .setQueuePriorities(queue_priorities);

    // nice-to-have optional core features, enable if GPU supports them.
    auto enabled_features =
      vk::PhysicalDeviceFeatures()
        .setFillModeNonSolid(gpu.features.fillModeNonSolid)
        .setWideLines(gpu.features.wideLines)
        .setSamplerAnisotropy(gpu.features.samplerAnisotropy)
        .setSampleRateShading(gpu.features.sampleRateShading);

    // extra features that need to be explicitly enabled.
    auto sync_feature = vk::PhysicalDeviceSynchronization2Features(vk::True);
    auto dynamic_rendering_feature =
      vk::PhysicalDeviceDynamicRenderingFeatures{vk::True};
    // sync_feature.pNext => dynamic_rendering_feature,
    // and later device_ci.pNext => sync_feature.
    // this is 'pNext chaining'.
    sync_feature.setPNext(&dynamic_rendering_feature);

    static constexpr auto extensions =
      std::array{VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    auto device_info = vk::DeviceCreateInfo()
                         .setPEnabledExtensionNames(extensions)
                         .setQueueCreateInfos(queue_ci)
                         .setPEnabledFeatures(&enabled_features)
                         .setPNext(&sync_feature);

    device = gpu.device.createDeviceUnique(device_info);
    waiter = *device;

    static constexpr std::uint32_t queue_index{0};
    queue = device->getQueue(gpu.queue_family, queue_index);
  }

  void App::main_loop() {
    while (glfwWindowShouldClose(window.get()) == GLFW_FALSE) {
      glfwPollEvents();
    }
  }
} // namespace lvk
