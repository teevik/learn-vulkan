#include "dear_imgui.h"
#include "resource_buffering.h"
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <glm/gtc/color_space.hpp>
#include <glm/mat4x4.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>
#include <imgui.h>

namespace lvk {
  DearImGui::DearImGui(CreateInfo const &create_info) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    static auto const load_vk_func = +[](char const *name, void *user_data) {
      return VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr(
        *static_cast<vk::Instance *>(user_data), name
      );
    };
    auto instance = create_info.instance;
    ImGui_ImplVulkan_LoadFunctions(
      create_info.api_version, load_vk_func, &instance
    );

    if (!ImGui_ImplGlfw_InitForVulkan(create_info.window, true)) {
      throw std::runtime_error{"Failed to initialize Dear ImGui"};
    }

    auto pipline_rendering_info =
      vk::PipelineRenderingCreateInfo()
        .setColorAttachmentCount(1)
        .setColorAttachmentFormats(create_info.color_format);

    auto init_info = ImGui_ImplVulkan_InitInfo{
      .ApiVersion = create_info.api_version,
      .Instance = create_info.instance,
      .PhysicalDevice = create_info.physical_device,
      .Device = create_info.device,
      .QueueFamily = create_info.queue_family,
      .Queue = create_info.queue,
      .DescriptorPool = {},
      .RenderPass = {},
      .MinImageCount = 2,
      .ImageCount = static_cast<std::uint32_t>(resource_buffering),
      .MSAASamples = static_cast<VkSampleCountFlagBits>(create_info.samples),
      .PipelineCache = {},
      .Subpass = {},
      .DescriptorPoolSize = 2,
      .UseDynamicRendering = true,
      .PipelineRenderingCreateInfo = pipline_rendering_info,
      .Allocator = {},
      .CheckVkResultFn = nullptr,
      .MinAllocationSize = {}
    };

    if (!ImGui_ImplVulkan_Init(&init_info))
      throw std::runtime_error{"Failed to initialize Dear ImGui"};

    ImGui_ImplVulkan_CreateFontsTexture();

    ImGui::StyleColorsDark();

    for (auto &colour : ImGui::GetStyle().Colors) {
      auto const linear = glm::convertSRGBToLinear(
        glm::vec4{colour.x, colour.y, colour.z, colour.w}
      );

      colour = ImVec4{linear.x, linear.y, linear.z, linear.w};
    }

    ImGui::GetStyle().Colors[ImGuiCol_WindowBg].w = 0.99f; // more opaque

    device = Scoped<vk::Device, Deleter>{create_info.device};
  };

  void DearImGui::new_frame() {
    if (state == State::Begun) end_frame();

    ImGui_ImplGlfw_NewFrame();
    ImGui_ImplVulkan_NewFrame();
    ImGui::NewFrame();
    state = State::Begun;
  }

  void DearImGui::end_frame() {
    if (state == State::Ended) return;

    ImGui::Render();
    state = State::Ended;
  }

  // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
  void DearImGui::render(vk::CommandBuffer const command_buffer) const {
    auto *data = ImGui::GetDrawData();
    if (data == nullptr) return;

    ImGui_ImplVulkan_RenderDrawData(data, command_buffer);
  }

  void DearImGui::Deleter::operator()(vk::Device const device) const {
    device.waitIdle();
    ImGui_ImplVulkan_DestroyFontsTexture();
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
  }
} // namespace lvk
