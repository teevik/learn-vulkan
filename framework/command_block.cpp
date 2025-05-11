#include "command_block.h"
#include <chrono>
#include <print>

using namespace std::chrono_literals;

namespace lvk {
  auto CommandBlock::get_command_buffer() const -> vk::CommandBuffer {
    return *command_buffer;
  }

  CommandBlock::CommandBlock(
    vk::Device const device,
    vk::Queue const queue,
    vk::CommandPool const command_pool
  ) : device(device), queue(queue) {
    // Allocate a UniqueCommandBuffer which will free the underlying command
    // buffer from its owning pool on destruction.
    auto allocate_info = vk::CommandBufferAllocateInfo()
                           .setCommandPool(command_pool)
                           .setCommandBufferCount(1)
                           .setLevel(vk::CommandBufferLevel::ePrimary);

    // All the current VulkanHPP functions for UniqueCommandBuffer allocation
    // return vectors.
    auto command_buffers = device.allocateCommandBuffersUnique(allocate_info);
    command_buffer = std::move(command_buffers.front());

    // Start recording commands before returning.
    auto begin_info = vk::CommandBufferBeginInfo().setFlags(
      vk::CommandBufferUsageFlagBits::eOneTimeSubmit
    );

    command_buffer->begin(begin_info);
  }

  void CommandBlock::submit_and_wait() {
    if (!command_buffer) return;

    // end recording and submit.
    command_buffer->end();
    auto const command_buffer_info =
      vk::CommandBufferSubmitInfo{*command_buffer};
    auto submit_info =
      vk::SubmitInfo2KHR().setCommandBufferInfos(command_buffer_info);

    auto fence = device.createFenceUnique({});
    queue.submit2(submit_info, *fence);

    // Wait for submit fence to be signaled.
    static constexpr auto timeout =
      static_cast<std::uint64_t>(std::chrono::nanoseconds(30s).count());

    auto const result = device.waitForFences(*fence, vk::True, timeout);
    if (result != vk::Result::eSuccess)
      std::println(stderr, "Failed to submit Command Buffer");

    // Free the command buffer.
    command_buffer.reset();
  }
} // namespace lvk
