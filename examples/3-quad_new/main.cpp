#include "imgui.h"
#include "glm/ext/matrix_clip_space.hpp"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>
#include <filesystem>
#include <print>

import framework;

namespace fs = std::filesystem;

namespace {
  struct Vertex {
    glm::vec2 position{};
    glm::vec3 color{1.0f};
    glm::vec2 uv{};
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
    // vec2 => 2x 32-bit floats
    vk::VertexInputAttributeDescription2EXT{
      2, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, uv)
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
    framework::Renderer &app,
    const fs::path &vertex_path,
    const fs::path &fragment_path,
    std::span<vk::DescriptorSetLayout> set_layouts
  ) -> framework::ShaderProgram {
    auto const vertex_spirv = framework::read_spir_v(vertex_path);
    auto const fragment_spirv = framework::read_spir_v(fragment_path);

    static constexpr auto vertex_input = framework::ShaderVertexInput{
      .attributes = vertex_attributes,
      .bindings = vertex_bindings,
    };

    auto const shader_info = framework::ShaderProgram::CreateInfo{
      .device = *app.device,
      .vertex_spirv = vertex_spirv,
      .fragment_spirv = fragment_spirv,
      .vertex_input = vertex_input,
      .set_layouts = set_layouts,
    };

    return framework::ShaderProgram(shader_info);
  }

  auto create_vertex_buffer(framework::Renderer &app)
    -> std::pair<framework::vma::Buffer, framework::DescriptorBuffer> {
    static constexpr auto vertices = std::array{
      Vertex{.position = {-200.0f, -200.0f}, .uv = {0.0f, 1.0f}},
      Vertex{.position = {200.0f, -200.0f}, .uv = {1.0f, 1.0f}},
      Vertex{.position = {200.0f, 200.0f}, .uv = {1.0f, 0.0f}},
      Vertex{.position = {-200.0f, 200.0f}, .uv = {0.0f, 0.0f}},
    };

    static constexpr auto indices = std::array{
      0u,
      1u,
      2u,
      2u,
      3u,
      0u,
    };

    static constexpr auto vertices_bytes = to_byte_array(vertices);
    static constexpr auto indices_bytes = to_byte_array(indices);

    static constexpr auto total_bytes =
      std::array<std::span<std::byte const>, 2>{
        vertices_bytes,
        indices_bytes,
      };

    auto const buffer_info = framework::vma::BufferCreateInfo{
      .allocator = app.allocator.get(),
      .usage = vk::BufferUsageFlagBits::eVertexBuffer |
        vk::BufferUsageFlagBits::eIndexBuffer,
      .queue_family = app.gpu.queue_family,
    };

    auto command_block =
      framework::CommandBlock{*app.device, app.queue, *app.cmd_block_pool};

    auto device_buffer = framework::vma::create_device_buffer(
      buffer_info, std::move(command_block), total_bytes
    );

    auto view_ubo = framework::DescriptorBuffer(
      app.allocator.get(),
      app.gpu.queue_family,
      vk::BufferUsageFlagBits::eUniformBuffer
    );

    return std::pair(std::move(device_buffer), std::move(view_ubo));
  }

  constexpr auto layout_binding(
    std::uint32_t binding, vk::DescriptorType const type
  ) {
    return vk::DescriptorSetLayoutBinding{
      binding, type, 1, vk::ShaderStageFlagBits::eAllGraphics
    };
  }
} // namespace

auto main() -> int {
  // TODO(teevik) Configurable
  glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);

  auto assets_dir = framework::locate_assets_dir();
  std::println("Using assets directory: {}", assets_dir.string());

  auto app = framework::Renderer();
  auto [vertex_buffer, view_ubo] = create_vertex_buffer(app);

  static constexpr auto pool_sizes = std::array{
    vk::DescriptorPoolSize{vk::DescriptorType::eUniformBuffer, 2},
    vk::DescriptorPoolSize{vk::DescriptorType::eCombinedImageSampler, 2},
  };

  // Allow 16 sets to be allocated from this pool
  auto pool_info =
    vk::DescriptorPoolCreateInfo().setPoolSizes(pool_sizes).setMaxSets(16);
  auto descriptor_pool = app.device->createDescriptorPoolUnique(pool_info);

  static constexpr auto set_0_bindings_v = std::array{
    layout_binding(0, vk::DescriptorType::eUniformBuffer),
  };

  static constexpr auto set_1_bindings_v = std::array{
    layout_binding(0, vk::DescriptorType::eCombinedImageSampler),
  };

  auto set_layout_cis = std::array<vk::DescriptorSetLayoutCreateInfo, 2>{};
  set_layout_cis[0].setBindings(set_0_bindings_v);
  set_layout_cis[1].setBindings(set_1_bindings_v);

  std::vector<vk::UniqueDescriptorSetLayout> m_set_layouts{};
  std::vector<vk::DescriptorSetLayout> m_set_layout_views{};

  for (auto const &set_layout_ci : set_layout_cis) {
    m_set_layouts.push_back(
      app.device->createDescriptorSetLayoutUnique(set_layout_ci)
    );
    m_set_layout_views.push_back(*m_set_layouts.back());
  }

  auto pipeline_layout_ci = vk::PipelineLayoutCreateInfo{};
  pipeline_layout_ci.setSetLayouts(m_set_layout_views);
  auto m_pipeline_layout =
    app.device->createPipelineLayoutUnique(pipeline_layout_ci);

  framework::Buffered<std::vector<vk::DescriptorSet>> m_descriptor_sets{};
  for (auto &descriptor_sets : m_descriptor_sets) {
    auto allocate_info = vk::DescriptorSetAllocateInfo()
                           .setDescriptorPool(*descriptor_pool)
                           .setSetLayouts(m_set_layout_views);

    descriptor_sets = app.device->allocateDescriptorSets(allocate_info);
  }

  auto shader = create_shader(
    app,
    assets_dir / "shader2.vert.spv",
    assets_dir / "shader2.frag.spv",
    m_set_layout_views
  );

  using Pixel = std::array<std::byte, 4>;
  static constexpr auto rgby_pixels_v = std::array{
    Pixel{std::byte{0xff}, {}, {}, std::byte{0xff}},
    Pixel{std::byte{}, std::byte{0xff}, {}, std::byte{0xff}},
    Pixel{std::byte{}, {}, std::byte{0xff}, std::byte{0xff}},
    Pixel{std::byte{0xff}, std::byte{0xff}, {}, std::byte{0xff}},
  };
  static constexpr auto rgby_bytes_v =
    std::bit_cast<std::array<std::byte, sizeof(rgby_pixels_v)>>(rgby_pixels_v);
  static constexpr auto rgby_bitmap_v = framework::vma::Bitmap{
    .bytes = rgby_bytes_v,
    .size = {2, 2},
  };

  auto command_block =
    framework::CommandBlock{*app.device, app.queue, *app.cmd_block_pool};

  auto texture_ci = framework::Texture::CreateInfo{
    .device = *app.device,
    .allocator = app.allocator.get(),
    .queue_family = app.gpu.queue_family,
    .command_block = std::move(command_block),
    .bitmap = rgby_bitmap_v,
  };
  // use Nearest filtering instead of Linear (interpolation).
  texture_ci.sampler.setMagFilter(vk::Filter::eNearest);
  auto texture = framework::Texture(std::move(texture_ci));

  auto use_wireframe = false;
  framework::Transform view_transform{};

  auto draw = [&app,
               &shader,
               &vertex_buffer,
               &use_wireframe,
               &view_ubo,
               &m_descriptor_sets,
               &m_pipeline_layout,
               &texture,
               &view_transform](vk::CommandBuffer const command_buffer) {
    ImGui::SetNextWindowSize({200.0f, 100.0f}, ImGuiCond_Once);
    if (ImGui::Begin("Inspect")) {
      if (ImGui::TreeNode("View")) {
        ImGui::DragFloat2("position", &view_transform.position.x);
        ImGui::DragFloat("rotation", &view_transform.rotation);
        ImGui::DragFloat2("scale", &view_transform.scale.x);
        ImGui::TreePop();
      }

      ImGui::Separator();

      if (ImGui::Checkbox("wireframe", &use_wireframe)) {
        shader.polygon_mode =
          use_wireframe ? vk::PolygonMode::eLine : vk::PolygonMode::eFill;
      }

      if (use_wireframe) {
        auto const &line_width_range = app.gpu.properties.limits.lineWidthRange;
        ImGui::SetNextItemWidth(100.0f);
        ImGui::DragFloat(
          "line width",
          &shader.line_width,
          0.25f,
          line_width_range[0],
          line_width_range[1]
        );
      }
    }
    ImGui::End();
    // Update view
    auto const half_size = 0.5f * glm::vec2{app.framebuffer_size};
    auto const mat_projection =
      glm::ortho(-half_size.x, half_size.x, -half_size.y, half_size.y);
    auto const mat_view = view_transform.view_matrix();
    auto const mat_vp = mat_projection * mat_view;
    auto const bytes =
      std::bit_cast<std::array<std::byte, sizeof(mat_vp)>>(mat_vp);

    view_ubo.write_at(app.frame_index, bytes);

    shader.bind(command_buffer, app.framebuffer_size);

    // Bind view ubo
    auto writes = std::array<vk::WriteDescriptorSet, 2>{};
    auto const &descriptor_set = m_descriptor_sets.at(app.frame_index);
    auto const set0 = descriptor_set[0];
    auto write = vk::WriteDescriptorSet{};
    auto const view_ubo_info = view_ubo.descriptor_info_at(app.frame_index);
    write.setBufferInfo(view_ubo_info)
      .setDescriptorType(vk::DescriptorType::eUniformBuffer)
      .setDescriptorCount(1)
      .setDstSet(set0)
      .setDstBinding(0);
    writes[0] = write;

    auto const set1 = descriptor_set[1];
    auto const image_info = texture.descriptor_info();
    write.setImageInfo(image_info)
      .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
      .setDescriptorCount(1)
      .setDstSet(set1)
      .setDstBinding(0);
    writes[1] = write;

    app.device->updateDescriptorSets(writes, {});

    command_buffer.bindDescriptorSets(
      vk::PipelineBindPoint::eGraphics,
      *m_pipeline_layout,
      0,
      descriptor_set,
      {}
    );

    // Single VBO at binding 0 at no offset
    command_buffer.bindVertexBuffers(
      0, vertex_buffer.get().buffer, vk::DeviceSize{}
    );

    // u32 indices after offset of 4 vertices
    command_buffer.bindIndexBuffer(
      vertex_buffer.get().buffer, 4 * sizeof(Vertex), vk::IndexType::eUint32
    );

    // command_buffer.draw(3, 1, 0, 0);
    command_buffer.drawIndexed(6, 1, 0, 0, 0);
  };

  app.run(draw);
}
