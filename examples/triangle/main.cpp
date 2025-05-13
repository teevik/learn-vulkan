#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>
#include <filesystem>
#include <print>

import framework_module;

namespace fs = std::filesystem;

namespace {
  struct Vertex {
    glm::vec2 position{};
    glm::vec3 color{1.0f};
  };

  // Two vertex attributes: position at 0, color at 1.
  constexpr auto vertex_attributes = std::array{
    // the format matches the type and layout of data: vec2 => 2x 32-bit floats.
    vk::VertexInputAttributeDescription2EXT{
      0, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, position)
    },
    // vec3 => 3x 32-bit floats
    vk::VertexInputAttributeDescription2EXT{
      1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color)
    },
  };

  // One vertex binding at location 0.
  constexpr auto vertex_bindings = std::array{
    // we are using interleaved data with a stride of sizeof(Vertex).
    vk::VertexInputBindingDescription2EXT{
      0, sizeof(Vertex), vk::VertexInputRate::eVertex, 1
    },
  };

  template <typename T> [[nodiscard]] constexpr auto to_byte_array(T const &t) {
    return std::bit_cast<std::array<std::byte, sizeof(T)>>(t);
  }

  auto create_shader(
    framework_module::Renderer &app,
    const fs::path &vertex_path,
    const fs::path &fragment_path
  ) -> framework_module::ShaderProgram {
    auto const vertex_spirv = framework_module::read_spir_v(vertex_path);
    auto const fragment_spirv = framework_module::read_spir_v(fragment_path);

    static constexpr auto vertex_input = framework_module::ShaderVertexInput{
      .attributes = vertex_attributes,
      .bindings = vertex_bindings,
    };

    auto const shader_info = framework_module::ShaderProgram::CreateInfo{
      .device = *app.device,
      .vertex_spirv = vertex_spirv,
      .fragment_spirv = fragment_spirv,
      .vertex_input = vertex_input,
      .set_layouts = {},
    };

    return framework_module::ShaderProgram(shader_info);
  }

  auto create_vertex_buffer(framework_module::Renderer &app)
    -> framework_module::vma::Buffer {
    static constexpr auto vertices = std::array{
      Vertex{.position = {-0.5f, -0.5f}, .color = {1.0f, 0.0f, 0.0f}},
      Vertex{.position = {0.5f, -0.5f}, .color = {0.0f, 1.0f, 0.0f}},
      Vertex{.position = {0.0f, 0.5f}, .color = {0.0f, 0.0f, 1.0f}},
    };

    auto const buffer_info = framework_module::vma::BufferCreateInfo{
      .allocator = app.allocator.get(),
      .usage = vk::BufferUsageFlagBits::eVertexBuffer,
      .queue_family = app.gpu.queue_family,
    };

    auto vertex_buffer = framework_module::vma::create_buffer(
      buffer_info,
      framework_module::vma::BufferMemoryType::Host,
      sizeof(vertices)
    );

    std::memcpy(vertex_buffer.get().mapped, vertices.data(), sizeof(vertices));

    return vertex_buffer;
  }
} // namespace

auto main() -> int {
  glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);

  auto assets_dir = framework_module::locate_assets_dir();
  std::println("Using assaaaets directory: {}", assets_dir.string());

  auto app = framework_module::Renderer();

  auto shader =
    create_shader(app, assets_dir / "vert.spv", assets_dir / "frag.spv");
  auto vertex_buffer = create_vertex_buffer(app);

  auto draw =
    [&app, &shader, &vertex_buffer](vk::CommandBuffer const command_buffer) {
      shader.bind(command_buffer, app.framebuffer_size);
      command_buffer.bindVertexBuffers(
        0, vertex_buffer.get().buffer, vk::DeviceSize{}
      );
      command_buffer.draw(3, 1, 0, 0);
    };

  app.run(draw);
}
