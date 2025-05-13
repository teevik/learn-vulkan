module;

#include <vulkan/vulkan.hpp>
#include <chrono>
#include <print>

export module framework:command_block;

using namespace std::chrono_literals;

namespace framework {
  export class CommandBlock {
  public:
    explicit CommandBlock(
      const vk::Device device,
      const vk::Queue queue,
      const vk::CommandPool command_pool
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

    [[nodiscard]] auto get_command_buffer() const -> vk::CommandBuffer {
      return *command_buffer;
    }

    void submit_and_wait() {
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

  private:
    vk::Device device;
    vk::Queue queue;
    vk::UniqueCommandBuffer command_buffer;
  };
} // namespace framework
