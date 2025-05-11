#pragma once

#include "scoped_waiter.h"
namespace lvk {
  struct ShaderVertexInput {
    std::span<vk::VertexInputAttributeDescription2EXT const> attributes;
    std::span<vk::VertexInputBindingDescription2EXT const> bindings;
  };

  struct ShaderProgramCreateInfo {
    vk::Device device;
    std::span<std::uint32_t const> vertex_spirv;
    std::span<std::uint32_t const> fragment_spirv;
    ShaderVertexInput vertex_input;
    std::span<vk::DescriptorSetLayout const> set_layouts;
  };

  class ShaderProgram {
  public:
    // Bit flags for various binary states.
    enum : std::uint8_t {
      None = 0,
      AlphaBlend = 1 << 0, // turn on alpha blending.
      DepthTest = 1 << 1, // turn on depth write and test.
    };

    static constexpr auto color_blend_function = [] {
      // Standard alpha blending:
      // (alpha * src) + (1 - alpha) * dst

      return vk::ColorBlendEquationEXT()
        .setColorBlendOp(vk::BlendOp::eAdd)
        .setSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha)
        .setDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha);
    }();

    vk::PrimitiveTopology topology{vk::PrimitiveTopology::eTriangleList};
    vk::PolygonMode polygon_mode{vk::PolygonMode::eFill};
    float line_width{1.0f};
    vk::ColorBlendEquationEXT color_blend_equation{color_blend_function};
    vk::CompareOp depth_compare_op{vk::CompareOp::eLessOrEqual};
    std::uint8_t flags = AlphaBlend | DepthTest;

    using CreateInfo = ShaderProgramCreateInfo;

    explicit ShaderProgram(CreateInfo const &create_info);

    void bind(
      vk::CommandBuffer command_buffer, glm::ivec2 framebuffer_size
    ) const;

  private:
    ShaderVertexInput vertex_input{};
    std::vector<vk::UniqueShaderEXT> shaders;

    ScopedWaiter waiter;

    static void set_viewport_scissor(
      vk::CommandBuffer command_buffer, glm::ivec2 framebuffer
    );
    static void set_static_states(vk::CommandBuffer command_buffer);
    void set_common_states(vk::CommandBuffer command_buffer) const;
    void set_vertex_states(vk::CommandBuffer command_buffer) const;
    void set_fragment_states(vk::CommandBuffer command_buffer) const;
    void bind_shaders(vk::CommandBuffer command_buffer) const;
  };
} // namespace lvk
