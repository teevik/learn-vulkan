#pragma once

#include "dear_imgui.h"
#include "gpu.h"
#include "resource_buffering.h"
#include "scoped_waiter.h"
#include "shader_program.h"
#include "swapchain.h"
#include "window.h"
#include <vulkan/vulkan_handles.hpp>
#include <filesystem>

namespace fs = std::filesystem;

namespace lvk {
  class App {
  private:
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

    fs::path assets_dir;

    glfw::Window window;
    vk::UniqueInstance instance;
    vk::UniqueSurfaceKHR surface;
    Gpu gpu{};
    vk::UniqueDevice device;
    vk::Queue queue;

    std::optional<Swapchain> swapchain;
    // Command pool for all render Command Buffers
    vk::UniqueCommandPool render_cmd_pool;
    // Sync and Command Buffer for virtual frames
    Buffered<RenderSync> render_sync{};
    // Current virtual frame index
    std::size_t frame_index{};

    glm::ivec2 framebuffer_size{};
    std::optional<RenderTarget> render_target;
    std::optional<DearImGui> imgui;

    std::optional<ShaderProgram> shader;

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
    void create_shader();

    void main_loop();

    auto acquire_render_target() -> bool;
    auto begin_frame() -> vk::CommandBuffer;
    void transition_for_render(vk::CommandBuffer command_buffer) const;
    void render(vk::CommandBuffer command_buffer);
    void transition_for_present(vk::CommandBuffer command_buffer) const;
    void submit_and_present();

    void inspect();
    void draw(vk::CommandBuffer command_buffer) const;

  public:
    void run();
  };
} // namespace lvk
