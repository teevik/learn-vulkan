module;

#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>
#include <span>

export module framework:shader_program;
import :scoped_waiter;

namespace {
  constexpr auto to_vkbool(bool const value) {
    return value ? vk::True : vk::False;
  }
} // namespace

namespace framework {
  export struct ShaderVertexInput {
    std::span<vk::VertexInputAttributeDescription2EXT const> attributes;
    std::span<vk::VertexInputBindingDescription2EXT const> bindings;
  };

  export struct ShaderProgramCreateInfo {
    vk::Device device;
    std::span<std::uint32_t const> vertex_spirv;
    std::span<std::uint32_t const> fragment_spirv;
    ShaderVertexInput vertex_input;
    std::span<vk::DescriptorSetLayout const> set_layouts;
  };

  export class ShaderProgram {
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

    explicit ShaderProgram(CreateInfo const &create_info) :
      vertex_input(create_info.vertex_input) {
      auto const create_shader_info =
        [&create_info](std::span<std::uint32_t const> spirv) {
          auto shader_info = vk::ShaderCreateInfoEXT()
                               .setCodeSize(spirv.size_bytes())
                               .setPCode(spirv.data())
                               // set common parameters.
                               .setSetLayouts(create_info.set_layouts)
                               .setCodeType(vk::ShaderCodeTypeEXT::eSpirv)
                               .setPName("main");

          return shader_info;
        };

      auto vertex_shader_info =
        create_shader_info(create_info.vertex_spirv)
          .setStage(vk::ShaderStageFlagBits::eVertex)
          .setNextStage(vk::ShaderStageFlagBits::eFragment);

      auto fragment_shader_info =
        create_shader_info(create_info.fragment_spirv)
          .setStage(vk::ShaderStageFlagBits::eFragment);

      auto shader_create_infos =
        std::array{vertex_shader_info, fragment_shader_info};

      auto result =
        create_info.device.createShadersEXTUnique(shader_create_infos);

      if (result.result != vk::Result::eSuccess)
        throw std::runtime_error{"Failed to create Shader Objects"};

      shaders = std::move(result.value);
      waiter = create_info.device;
    }

    void bind(
      vk::CommandBuffer const command_buffer, glm::ivec2 const framebuffer_size
    ) const {
      set_viewport_scissor(command_buffer, framebuffer_size);
      set_static_states(command_buffer);
      set_common_states(command_buffer);
      set_vertex_states(command_buffer);
      set_fragment_states(command_buffer);
      bind_shaders(command_buffer);
    }

  private:
    ShaderVertexInput vertex_input{};
    std::vector<vk::UniqueShaderEXT> shaders;

    ScopedWaiter waiter;

    static void set_viewport_scissor(
      vk::CommandBuffer const command_buffer, glm::ivec2 const framebuffer_size
    ) {
      auto const fsize = glm::vec2{framebuffer_size};
      auto viewport = vk::Viewport{};
      // Flip the viewport about the X-axis (negative height):
      // https://www.saschawillems.de/blog/2019/03/29/flipping-the-vulkan-viewport/
      viewport.setX(0.0f).setY(fsize.y).setWidth(fsize.x).setHeight(-fsize.y);
      command_buffer.setViewportWithCount(viewport);

      auto const usize = glm::uvec2{framebuffer_size};
      auto const scissor =
        vk::Rect2D{vk::Offset2D{}, vk::Extent2D{usize.x, usize.y}};
      command_buffer.setScissorWithCount(scissor);
    }

    static void set_static_states(vk::CommandBuffer const command_buffer) {
      command_buffer.setRasterizerDiscardEnable(vk::False);
      command_buffer.setRasterizationSamplesEXT(vk::SampleCountFlagBits::e1);
      command_buffer.setSampleMaskEXT(vk::SampleCountFlagBits::e1, 0xff);
      command_buffer.setAlphaToCoverageEnableEXT(vk::False);
      command_buffer.setCullMode(vk::CullModeFlagBits::eNone);
      command_buffer.setFrontFace(vk::FrontFace::eCounterClockwise);
      command_buffer.setDepthBiasEnable(vk::False);
      command_buffer.setStencilTestEnable(vk::False);
      command_buffer.setPrimitiveRestartEnable(vk::False);
      command_buffer.setColorWriteMaskEXT(0, ~vk::ColorComponentFlags{});
    }

    void set_common_states(vk::CommandBuffer const command_buffer) const {
      auto const depth_test = to_vkbool((flags & DepthTest) == DepthTest);
      command_buffer.setDepthWriteEnable(depth_test);
      command_buffer.setDepthTestEnable(depth_test);
      command_buffer.setDepthCompareOp(depth_compare_op);
      command_buffer.setPolygonModeEXT(polygon_mode);
      command_buffer.setLineWidth(line_width);
    }

    void set_vertex_states(vk::CommandBuffer const command_buffer) const {
      command_buffer.setVertexInputEXT(
        vertex_input.bindings, vertex_input.attributes
      );
      command_buffer.setPrimitiveTopology(topology);
    }

    void set_fragment_states(vk::CommandBuffer const command_buffer) const {
      auto const alpha_blend = to_vkbool((flags & AlphaBlend) == AlphaBlend);

      command_buffer.setColorBlendEnableEXT(0, alpha_blend);
      command_buffer.setColorBlendEquationEXT(0, color_blend_equation);
    }

    void bind_shaders(vk::CommandBuffer const command_buffer) const {
      static constexpr auto stages_v = std::array{
        vk::ShaderStageFlagBits::eVertex,
        vk::ShaderStageFlagBits::eFragment,
      };

      auto const shaders = std::array{
        *this->shaders[0],
        *this->shaders[1],
      };

      command_buffer.bindShadersEXT(stages_v, shaders);
    }
  };
} // namespace framework
