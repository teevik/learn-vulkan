#pragma once

#include "command_block.h"
#include "scoped.h"
#include <vk_mem_alloc.h>

namespace lvk::vma {
  struct Deleter {
    void operator()(VmaAllocator allocator) const noexcept;
  };

  using Allocator = Scoped<VmaAllocator, Deleter>;

  [[nodiscard]]
  auto create_allocator(
    vk::Instance instance, vk::PhysicalDevice physical_device, vk::Device device
  ) -> Allocator;

  struct RawBuffer {
    [[nodiscard]] auto mapped_span() const -> std::span<std::byte> {
      return std::span{static_cast<std::byte *>(mapped), size};
    }

    auto operator==(RawBuffer const &rhs) const -> bool = default;

    VmaAllocator allocator{};
    VmaAllocation allocation{};
    vk::Buffer buffer;
    vk::DeviceSize size{};
    void *mapped{};
  };

  struct BufferDeleter {
    void operator()(RawBuffer const &raw_buffer) const noexcept;
  };

  using Buffer = Scoped<RawBuffer, BufferDeleter>;

  struct BufferCreateInfo {
    VmaAllocator allocator;
    vk::BufferUsageFlags usage;
    std::uint32_t queue_family;
  };

  enum class BufferMemoryType : std::int8_t { Host, Device };

  [[nodiscard]]
  auto create_buffer(
    BufferCreateInfo const &create_info,
    BufferMemoryType memory_type,
    vk::DeviceSize size
  ) -> Buffer;

  // disparate byte spans.
  using ByteSpans = std::span<std::span<std::byte const> const>;

  // returns a Device Buffer with each byte span sequentially written.
  [[nodiscard]] auto create_device_buffer(
    BufferCreateInfo const &create_info,
    CommandBlock command_block,
    ByteSpans const &byte_spans
  ) -> Buffer;
} // namespace lvk::vma
