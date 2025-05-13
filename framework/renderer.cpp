module;

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_hpp_macros.hpp>
#include <chrono>
#include <format>
#include <functional>
#include <imgui.h>
#include <print>
#include <ranges>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

export module framework:renderer;
import :dear_imgui;
import :gpu;
import :resource_buffering;
import :scoped_waiter;
import :swapchain;
import :vma;
import :window;

using namespace std::chrono_literals;

namespace {
  [[nodiscard]] auto get_valid_layers(std::span<char const *const> desired)
    -> std::vector<char const *> {
    auto layers = std::vector<char const *>{};
    layers.reserve(desired.size());

    auto const available = vk::enumerateInstanceLayerProperties();

    for (char const *layer : desired) {
      auto const pred = [layer = std::string_view{layer}](
                          vk::LayerProperties const &properties
                        ) { return properties.layerName == layer; };

      if (std::ranges::find_if(available, pred) == available.end()) {
        std::println("[lvk] [WARNING] Vulkan Layer '{}' not found", layer);
        continue;
      }

      layers.push_back(layer);
    }

    return layers;
  }
} // namespace

namespace framework {
  export class Renderer {
  public:
    struct RenderSync {
      /// Signalled when Swapchain image has been acquired
      vk::UniqueSemaphore draw;
      /// Signalled when image is ready to be presented
      vk::UniqueSemaphore present;
      /// Signalled with present Semaphore, waited on before next render
      vk::UniqueFence drawn;
      /// Used to record rendering commands
      vk::CommandBuffer command_buffer;
    };

    glfw::Window window;
    vk::UniqueInstance instance;
    vk::UniqueSurfaceKHR surface;
    Gpu gpu{};
    vk::UniqueDevice device;
    vk::Queue queue;
    vma::Allocator allocator;

    std::optional<Swapchain> swapchain;
    // Command pool for all render Command Buffers
    vk::UniqueCommandPool render_cmd_pool;
    // Command pool for all Command Blocks.
    vk::UniqueCommandPool cmd_block_pool;
    // Sync and Command Buffer for virtual frames
    Buffered<RenderSync> render_sync{};
    // Current virtual frame index
    std::size_t frame_index{};

    glm::ivec2 framebuffer_size{};
    std::optional<RenderTarget> render_target;
    std::optional<DearImGui> imgui;

    ScopedWaiter waiter;

    bool wireframe = false;

    void create_window() {
      window = glfw::create_window({1280, 720}, "Learn Vulkan");
    }

    void create_instance() {
      // Initialize the dispatcher without any arguments.
      VULKAN_HPP_DEFAULT_DISPATCHER.init();

      auto app_info = vk::ApplicationInfo()
                        .setPApplicationName("Learn Vulkan")
                        .setApiVersion(vk_version);

      static constexpr auto layers = std::array{
        "VK_LAYER_KHRONOS_shader_object",
      };
      auto valid_layers = get_valid_layers(layers);

      auto const extensions = glfw::instance_extensions();

      auto instance_info = vk::InstanceCreateInfo()
                             .setPApplicationInfo(&app_info)
                             .setPEnabledLayerNames(valid_layers)
                             .setPEnabledExtensionNames(extensions);

      instance = vk::createInstanceUnique(instance_info);
      VULKAN_HPP_DEFAULT_DISPATCHER.init(*instance);
    }

    void create_surface() {
      surface = glfw::create_surface(window.get(), instance.get());
    }

    void select_gpu() {
      gpu = get_suitable_gpu(*instance, *surface);

      std::println(
        "Using GPU: {}", std::string_view{gpu.properties.deviceName}
      );
    }

    void create_device() {
      // since we use only one queue, it has the entire priority range, ie, 1.0
      static constexpr auto queue_priorities = std::array{1.0f};

      auto queue_info = vk::DeviceQueueCreateInfo()
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

      auto shader_object_feature =
        vk::PhysicalDeviceShaderObjectFeaturesEXT(vk::True);

      // Extra features that need to be explicitly enabled.
      auto dynamic_rendering_feature =
        vk::PhysicalDeviceDynamicRenderingFeatures(vk::True).setPNext(
          &shader_object_feature
        );

      // sync_feature.pNext => dynamic_rendering_feature,
      // and later device_ci.pNext => sync_feature.
      // this is 'pNext chaining'.
      auto sync_feature =
        vk::PhysicalDeviceSynchronization2Features(vk::True).setPNext(
          &dynamic_rendering_feature
        );

      static constexpr auto extensions =
        std::array{VK_KHR_SWAPCHAIN_EXTENSION_NAME, "VK_EXT_shader_object"};

      auto device_info = vk::DeviceCreateInfo()
                           .setPEnabledExtensionNames(extensions)
                           .setQueueCreateInfos(queue_info)
                           .setPEnabledFeatures(&enabled_features)
                           .setPNext(&sync_feature);

      device = gpu.device.createDeviceUnique(device_info);
      VULKAN_HPP_DEFAULT_DISPATCHER.init(*device);

      waiter = *device;

      static constexpr std::uint32_t queue_index{0};
      queue = device->getQueue(gpu.queue_family, queue_index);
    }

    void create_imgui() {
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

    void create_allocator() {
      allocator = vma::create_allocator(*instance, gpu.device, *device);
    }

    void create_cmd_block_pool() {
      auto command_pool_info =
        vk::CommandPoolCreateInfo()
          .setQueueFamilyIndex(gpu.queue_family)
          // This flag indicates that the allocated Command Buffers will be
          // short-lived.
          .setFlags(vk::CommandPoolCreateFlagBits::eTransient);

      cmd_block_pool = device->createCommandPoolUnique(command_pool_info);
    }

    auto acquire_render_target() -> bool {
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

    void create_swapchain() {
      auto const size = glfw::framebuffer_size(window.get());

      swapchain.emplace(*device, gpu, *surface, size);
    }

    void create_render_sync() {
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

    auto begin_frame() -> vk::CommandBuffer {
      auto const &current_render_sync = render_sync.at(frame_index);

      auto command_buffer_bi = vk::CommandBufferBeginInfo{};
      // this flag means recorded commands will not be reused.
      command_buffer_bi.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit
      );
      current_render_sync.command_buffer.begin(command_buffer_bi);
      return current_render_sync.command_buffer;
    }

    void transition_for_render(vk::CommandBuffer const command_buffer) const {
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

      auto dependency_info =
        vk::DependencyInfo().setImageMemoryBarriers(barrier);

      command_buffer.pipelineBarrier2(dependency_info);
    }

    void render(
      vk::CommandBuffer const command_buffer,
      const std::function<void(vk::CommandBuffer const)> &draw
    ) {
      auto color_attachment =
        vk::RenderingAttachmentInfo()
          .setImageView(render_target->image_view)
          .setImageLayout(vk::ImageLayout::eAttachmentOptimal)
          .setLoadOp(vk::AttachmentLoadOp::eClear)
          .setStoreOp(vk::AttachmentStoreOp::eStore)
          .setClearValue(vk::ClearColorValue{0.0f, 0.0f, 0.0f, 1.0f});

      auto const render_area =
        vk::Rect2D{vk::Offset2D{}, render_target->extent};
      auto rendering_info = vk::RenderingInfo()
                              .setRenderArea(render_area)
                              .setColorAttachments(color_attachment)
                              .setLayerCount(1);

      command_buffer.beginRendering(rendering_info);

      draw(command_buffer);

      command_buffer.endRendering();

      imgui->end_frame();

      // We don't want to clear the image again, instead load it intact after the
      // previous pass.
      color_attachment =
        color_attachment.setLoadOp(vk::AttachmentLoadOp::eLoad);
      rendering_info = rendering_info.setColorAttachments(color_attachment)
                         .setPDepthAttachment(nullptr);
      command_buffer.beginRendering(rendering_info);
      imgui->render(command_buffer);
      command_buffer.endRendering();
    }

    void transition_for_present(vk::CommandBuffer const command_buffer) const {
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

      auto dependency_info =
        vk::DependencyInfo().setImageMemoryBarriers(barrier);
      command_buffer.pipelineBarrier2(dependency_info);
    }

    void submit_and_present() {
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

  public:
    explicit Renderer() {
      create_window();
      create_instance();
      create_surface();
      select_gpu();
      create_device();
      create_allocator();
      create_swapchain();
      create_render_sync();
      create_imgui();
      create_cmd_block_pool();
    }

    void run(const std::function<void(vk::CommandBuffer const)> &draw) {
      while (glfwWindowShouldClose(window.get()) == GLFW_FALSE) {
        glfwPollEvents();

        if (!acquire_render_target()) continue;

        auto const command_buffer = begin_frame();
        transition_for_render(command_buffer);
        render(command_buffer, draw);
        transition_for_present(command_buffer);
        submit_and_present();
      }
    }
  };
} // namespace lvk
