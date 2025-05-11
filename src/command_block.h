#pragma once

namespace lvk {
  class CommandBlock {
  public:
    explicit CommandBlock(
      vk::Device device, vk::Queue queue, vk::CommandPool command_pool
    );

    [[nodiscard]] auto get_command_buffer() const -> vk::CommandBuffer;

    void submit_and_wait();

  private:
    vk::Device device;
    vk::Queue queue;
    vk::UniqueCommandBuffer command_buffer;
  };
} // namespace lvk
