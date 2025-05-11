#include "vma.h"
#include <numeric>
#include <print>

namespace lvk::vma {
  void Deleter::operator()(VmaAllocator allocator) const noexcept {
    vmaDestroyAllocator(allocator);
  }

  auto create_allocator(
    vk::Instance const instance,
    vk::PhysicalDevice const physical_device,
    vk::Device const device
  ) -> Allocator {
    auto const &dispatcher = VULKAN_HPP_DEFAULT_DISPATCHER;

    // Need to zero initialize C structs, unlike VulkanHPP.
    auto vma_vk_funcs = VmaVulkanFunctions{};
    vma_vk_funcs.vkGetInstanceProcAddr = dispatcher.vkGetInstanceProcAddr;
    vma_vk_funcs.vkGetDeviceProcAddr = dispatcher.vkGetDeviceProcAddr;

    auto allocator_info = VmaAllocatorCreateInfo{};
    allocator_info.physicalDevice = physical_device;
    allocator_info.device = device;
    allocator_info.pVulkanFunctions = &vma_vk_funcs;
    allocator_info.instance = instance;

    VmaAllocator allocator{};
    auto const result = vmaCreateAllocator(&allocator_info, &allocator);
    if (result == VK_SUCCESS) return allocator;

    throw std::runtime_error{"Failed to create Vulkan Memory Allocator"};
  }

  void BufferDeleter::operator()(RawBuffer const &raw_buffer) const noexcept {
    vmaDestroyBuffer(
      raw_buffer.allocator, raw_buffer.buffer, raw_buffer.allocation
    );
  }

  auto create_buffer(
    BufferCreateInfo const &create_info,
    BufferMemoryType const memory_type,
    vk::DeviceSize const size
  ) -> Buffer {
    if (size == 0) {
      std::println(stderr, "Buffer cannot be 0-sized");
      return {};
    }

    auto allocation_ci = VmaAllocationCreateInfo{};
    allocation_ci.flags =
      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

    auto usage = create_info.usage;
    if (memory_type == BufferMemoryType::Device) {
      allocation_ci.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
      // Device buffers need to support TransferDst.
      usage |= vk::BufferUsageFlagBits::eTransferDst;
    } else {
      allocation_ci.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
      // Host buffers can provide mapped memory.
      allocation_ci.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
    }

    auto buffer_info = vk::BufferCreateInfo()
                         .setQueueFamilyIndices(create_info.queue_family)
                         .setSize(size)
                         .setUsage(usage);
    auto vma_buffer_info = static_cast<VkBufferCreateInfo>(buffer_info);

    VmaAllocation allocation{};
    VkBuffer buffer{};
    auto allocation_info = VmaAllocationInfo{};

    auto const result = vmaCreateBuffer(
      create_info.allocator,
      &vma_buffer_info,
      &allocation_ci,
      &buffer,
      &allocation,
      &allocation_info
    );

    if (result != VK_SUCCESS) {
      std::println(stderr, "Failed to create VMA Buffer");
      return {};
    }

    return RawBuffer{
      .allocator = create_info.allocator,
      .allocation = allocation,
      .buffer = buffer,
      .size = size,
      .mapped = allocation_info.pMappedData,
    };
  }

  auto create_device_buffer(
    BufferCreateInfo const &create_info,
    CommandBlock command_block,
    ByteSpans const &byte_spans
  ) -> Buffer {
    auto const total_size = std::accumulate(
      byte_spans.begin(),
      byte_spans.end(),
      0uz,
      [](std::size_t const n, std::span<std::byte const> bytes) {
        return n + bytes.size();
      }
    );

    auto staging_info = create_info;
    staging_info.usage = vk::BufferUsageFlagBits::eTransferSrc;

    // Create staging Host Buffer with TransferSrc usage.
    auto staging_buffer =
      create_buffer(staging_info, BufferMemoryType::Host, total_size);

    // Create the Device Buffer.
    auto device_buffer =
      create_buffer(create_info, BufferMemoryType::Device, total_size);

    // Can't do anything if either buffer creation failed.
    if (!staging_buffer.get().buffer || !device_buffer.get().buffer) return {};

    // Copy byte spans into staging buffer.
    auto dst = staging_buffer.get().mapped_span();
    for (auto const bytes : byte_spans) {
      std::memcpy(dst.data(), bytes.data(), bytes.size());
      dst = dst.subspan(bytes.size());
    }

    // Record buffer copy operation.
    auto buffer_copy = vk::BufferCopy2{};
    buffer_copy.setSize(total_size);
    auto copy_buffer_info = vk::CopyBufferInfo2{};
    copy_buffer_info.setSrcBuffer(staging_buffer.get().buffer)
      .setDstBuffer(device_buffer.get().buffer)
      .setRegions(buffer_copy);
    command_block.get_command_buffer().copyBuffer2(copy_buffer_info);

    // Submit and wait.
    // Waiting here is necessary to keep the staging buffer alive while the GPU
    // accesses it through the recorded commands.
    // this is also why the function takes ownership of the passed CommandBlock
    // instead of just referencing it / taking a vk::CommandBuffer.
    command_block.submit_and_wait();

    return device_buffer;
  }
} // namespace lvk::vma
