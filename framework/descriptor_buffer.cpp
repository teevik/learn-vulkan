module;

#include <vulkan/vulkan.hpp>
#include <cstdint>
#include <vk_mem_alloc.h>

export module framework:descriptor_buffer;
import :resource_buffering;
import :vma;

namespace framework {
  export class DescriptorBuffer {
  public:
    explicit DescriptorBuffer(
      VmaAllocator allocator,
      std::uint32_t const queue_family,
      vk::BufferUsageFlags const usage
    ) : allocator(allocator), queue_family(queue_family), usage(usage) {
      // Ensure buffers are created and can be bound after returning
      for (auto &buffer : buffers) {
        write_to(buffer, {});
      }
    }

    void write_at(
      std::size_t const frame_index, std::span<std::byte const> bytes
    ) {
      write_to(buffers.at(frame_index), bytes);
    }

    [[nodiscard]] auto descriptor_info_at(std::size_t const frame_index) const
      -> vk::DescriptorBufferInfo {
      auto const &buffer = buffers.at(frame_index);

      auto descriptor_buffer_info = vk::DescriptorBufferInfo()
                                      .setBuffer(buffer.buffer.get().buffer)
                                      .setRange(buffer.size);

      return descriptor_buffer_info;
    }

  private:
    struct Buffer {
      vma::Buffer buffer;
      vk::DeviceSize size{};
    };

    void write_to(Buffer &out, std::span<std::byte const> bytes) const {
      static constexpr auto blank_byte = std::array{std::byte{}};
      // Fallback to an empty byte if bytes is empty
      if (bytes.empty()) {
        bytes = blank_byte;
      }

      out.size = bytes.size();
      if (out.buffer.get().size < bytes.size()) {
        // Size is too small (or buffer doesn't exist yet), recreate buffer
        auto const buffer_info = vma::BufferCreateInfo{
          .allocator = allocator,
          .usage = usage,
          .queue_family = queue_family,
        };

        out.buffer = vma::create_buffer(
          buffer_info, vma::BufferMemoryType::Host, out.size
        );
      }
      std::memcpy(out.buffer.get().mapped, bytes.data(), bytes.size());
    };

    VmaAllocator allocator{};
    std::uint32_t queue_family{};
    vk::BufferUsageFlags usage;
    Buffered<Buffer> buffers{};
  };
} // namespace framework
