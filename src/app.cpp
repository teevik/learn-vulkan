#include "app.h"
#include "gpu.h"
#include "window.h"
#include <vulkan/vulkan_structs.hpp>
#include <chrono>
#include <imgui.h>
#include <print>
#include <ranges>

using namespace std::chrono_literals;

namespace lvk {
  void App::run() {
    create_window();
    create_instance();
    create_surface();
    select_gpu();
    create_device();
    create_swapchain();
    create_render_sync();
    create_imgui();

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

  void App::create_imgui() {
    auto const imgui_info = DearImGui::CreateInfo{
      .window = window.get(),
      .api_version = vk_version,
      .instance = *instance,
      .physical_device = gpu.device,
      .queue_family = gpu.queue_family,
      .device = *device,
      .queue = queue,
      .color_format = swapchain->get_format(),
      .samples = vk::SampleCountFlagBits::e1,
    };

    imgui.emplace(imgui_info);
  };

  void App::main_loop() {
    while (glfwWindowShouldClose(window.get()) == GLFW_FALSE) {
      glfwPollEvents();

      if (!acquire_render_target()) continue;

      auto const command_buffer = begin_frame();
      transition_for_render(command_buffer);
      render(command_buffer);
      transition_for_present(command_buffer);
      submit_and_present();
    }
  }

  auto App::acquire_render_target() -> bool {
    framebuffer_size = glfw::framebuffer_size(window.get());

    // Skip loop if minimized
    if (framebuffer_size.x <= 0 || framebuffer_size.y <= 0) return false;

    auto &current_render_sync = render_sync.at(frame_index);

    // Wait for the fence to be signaled.
    static constexpr auto fence_timeout_v =
      static_cast<std::uint64_t>(std::chrono::nanoseconds{3s}.count());
    auto result = device->waitForFences(
      *current_render_sync.drawn, vk::True, fence_timeout_v
    );

    if (result != vk::Result::eSuccess)
      throw std::runtime_error{"Failed to wait for Render Fence"};

    render_target = swapchain->acquire_next_image(*current_render_sync.draw);
    if (!render_target) {
      // Acquire failure => ErrorOutOfDate. Recreate Swapchain.
      swapchain->recreate(framebuffer_size);

      return false;
    }

    // Reset fence _after_ acquisition of image: if it fails, the
    // fence remains signaled.
    device->resetFences(*current_render_sync.drawn);
    imgui->new_frame();

    return true;
  }

  void App::create_swapchain() {
    auto const size = glfw::framebuffer_size(window.get());

    swapchain.emplace(*device, gpu, *surface, size);
  }

  void App::create_render_sync() {
    auto command_pool_info =
      vk::CommandPoolCreateInfo()
        // Enables resetting command buffer
        .setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer)
        .setQueueFamilyIndex(gpu.queue_family);

    render_cmd_pool = device->createCommandPoolUnique(command_pool_info);

    auto command_buffer_info =
      vk::CommandBufferAllocateInfo()
        .setCommandPool(*render_cmd_pool)
        .setCommandBufferCount(static_cast<std::uint32_t>(resource_buffering))
        .setLevel(vk::CommandBufferLevel::ePrimary);

    auto const command_buffers =
      device->allocateCommandBuffers(command_buffer_info);

    assert(command_buffers.size() == render_sync.size());

    // We create Render Fences as pre-signaled so that on the first render for
    // each virtual frame we don't wait on their fences (since there's nothing
    // to wait for yet).
    static constexpr auto fence_create_info =
      vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled);

    for (auto [sync, command_buffer] :
         std::views::zip(render_sync, command_buffers)) {

      sync.command_buffer = command_buffer;
      sync.draw = device->createSemaphoreUnique({});
      sync.present = device->createSemaphoreUnique({});
      sync.drawn = device->createFenceUnique(fence_create_info);
    }
  }

  auto App::begin_frame() -> vk::CommandBuffer {
    auto const &current_render_sync = render_sync.at(frame_index);

    auto command_buffer_bi = vk::CommandBufferBeginInfo{};
    // this flag means recorded commands will not be reused.
    command_buffer_bi.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    current_render_sync.command_buffer.begin(command_buffer_bi);
    return current_render_sync.command_buffer;
  }

  void App::transition_for_render(
    vk::CommandBuffer const command_buffer
  ) const {
    auto barrier = swapchain->base_barrier();

    // Undefined => AttachmentOptimal
    // the barrier must wait for prior color attachment operations to complete,
    // and block subsequent ones.
    barrier.setOldLayout(vk::ImageLayout::eUndefined)
      .setNewLayout(vk::ImageLayout::eAttachmentOptimal)
      .setSrcAccessMask(
        vk::AccessFlagBits2::eColorAttachmentRead |
        vk::AccessFlagBits2::eColorAttachmentWrite
      )
      .setSrcStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
      .setDstAccessMask(barrier.srcAccessMask)
      .setDstStageMask(barrier.srcStageMask);

    auto dependency_info = vk::DependencyInfo().setImageMemoryBarriers(barrier);

    command_buffer.pipelineBarrier2(dependency_info);
  }

  void App::render(vk::CommandBuffer const command_buffer) {
    auto color_attachment =
      vk::RenderingAttachmentInfo()
        .setImageView(render_target->image_view)
        .setImageLayout(vk::ImageLayout::eAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        // temporarily red.
        .setClearValue(vk::ClearColorValue{1.0f, 0.0f, 0.0f, 1.0f});

    auto const render_area = vk::Rect2D{vk::Offset2D{}, render_target->extent};
    auto rendering_info = vk::RenderingInfo()
                            .setRenderArea(render_area)
                            .setColorAttachments(color_attachment)
                            .setLayerCount(1);

    command_buffer.beginRendering(rendering_info);
    // ImGui::ShowDemoWindow();

    ImGui::Text(
      "Application average %.3f ms/frame (%.1f FPS)",
      1000.0f / ImGui::GetIO().Framerate,
      ImGui::GetIO().Framerate
    );

    // TODO(teevik): draw stuff here

    command_buffer.endRendering();

    imgui->end_frame();

    // We don't want to clear the image again, instead load it intact after the
    // previous pass.
    color_attachment.setLoadOp(vk::AttachmentLoadOp::eLoad);
    rendering_info.setColorAttachments(color_attachment)
      .setPDepthAttachment(nullptr);
    command_buffer.beginRendering(rendering_info);
    imgui->render(command_buffer);
    command_buffer.endRendering();
  }

  void App::transition_for_present(
    vk::CommandBuffer const command_buffer
  ) const {
    auto barrier = swapchain->base_barrier();

    // AttachmentOptimal => PresentSrc
    // the barrier must wait for prior color attachment operations to complete,
    // and block subsequent ones.
    barrier.setOldLayout(vk::ImageLayout::eAttachmentOptimal)
      .setNewLayout(vk::ImageLayout::ePresentSrcKHR)
      .setSrcAccessMask(
        vk::AccessFlagBits2::eColorAttachmentRead |
        vk::AccessFlagBits2::eColorAttachmentWrite
      )
      .setSrcStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
      .setDstAccessMask(barrier.srcAccessMask)
      .setDstStageMask(barrier.srcStageMask);

    auto dependency_info = vk::DependencyInfo().setImageMemoryBarriers(barrier);
    command_buffer.pipelineBarrier2(dependency_info);
  }

  void App::submit_and_present() {
    auto const &current_render_sync = render_sync.at(frame_index);
    current_render_sync.command_buffer.end();

    auto submit_info = vk::SubmitInfo2{};
    auto const command_buffer_info =
      vk::CommandBufferSubmitInfo{current_render_sync.command_buffer};
    auto wait_semaphore_info = vk::SemaphoreSubmitInfo{};
    wait_semaphore_info.setSemaphore(*current_render_sync.draw)
      .setStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput);
    auto signal_semaphore_info = vk::SemaphoreSubmitInfo{};
    signal_semaphore_info.setSemaphore(*current_render_sync.present)
      .setStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput);
    submit_info.setCommandBufferInfos(command_buffer_info)
      .setWaitSemaphoreInfos(wait_semaphore_info)
      .setSignalSemaphoreInfos(signal_semaphore_info);
    queue.submit2(submit_info, *current_render_sync.drawn);

    frame_index = (frame_index + 1) % render_sync.size();
    render_target.reset();

    // an eErrorOutOfDateKHR result is not guaranteed if the
    // framebuffer size does not match the Swapchain image size, check it
    // explicitly.
    auto const fb_size_changed = framebuffer_size != swapchain->get_size();
    auto const out_of_date =
      !swapchain->present(queue, *current_render_sync.present);
    if (fb_size_changed || out_of_date) {
      swapchain->recreate(framebuffer_size);
    }
  }
} // namespace lvk
