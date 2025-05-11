#pragma once

#include "framework/dear_imgui.h"
#include "framework/gpu.h"
#include "framework/resource_buffering.h"
#include "framework/scoped_waiter.h"
#include "framework/swapchain.h"
#include "framework/vma.h"
#include "framework/window.h"
#include <vulkan/vulkan_handles.hpp>
#include <functional>

namespace lvk {
  class Renderer {
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

    void create_window();
    void create_instance();
    void create_surface();
    void select_gpu();
    void create_device();
    void create_swapchain();
    void create_render_sync();
    void create_imgui();
    void create_allocator();
    void create_cmd_block_pool();

    auto acquire_render_target() -> bool;
    auto begin_frame() -> vk::CommandBuffer;
    void transition_for_render(vk::CommandBuffer command_buffer) const;
    void render(
      vk::CommandBuffer command_buffer,
      const std::function<void(vk::CommandBuffer const)> &draw
    );
    void transition_for_present(vk::CommandBuffer command_buffer) const;
    void submit_and_present();

    void inspect();
    void draw(vk::CommandBuffer command_buffer) const;

    explicit Renderer();
    void run(const std::function<void(vk::CommandBuffer const)> &draw);
  };
} // namespace lvk
