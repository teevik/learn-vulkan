#pragma once

#include "scoped.hpp"
#include <GLFW/glfw3.h>

namespace lvk {
  struct DearImGuiCreateInfo {
    GLFWwindow *window{};
    std::uint32_t api_version{};
    vk::Instance instance;
    vk::PhysicalDevice physical_device;
    std::uint32_t queue_family{};
    vk::Device device;
    vk::Queue queue;
    vk::Format color_format{}; // single color attachment.
    vk::SampleCountFlagBits samples{};
  };

  class DearImGui {
  public:
    using CreateInfo = DearImGuiCreateInfo;

    explicit DearImGui(CreateInfo const &create_info);

    void new_frame();
    void end_frame();
    void render(vk::CommandBuffer command_buffer) const;

  private:
    enum class State : std::int8_t { Ended, Begun };

    struct Deleter {
      void operator()(vk::Device device) const;
    };

    State state{};

    vk::UniqueDescriptorPool descriptor_pool;
    Scoped<vk::Device, Deleter> device;
  };
} // namespace lvk
