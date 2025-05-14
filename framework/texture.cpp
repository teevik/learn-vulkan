module;

#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.h>

export module framework:texture;
import :vma;

namespace {
  // 4-channels.
  constexpr auto white_pixel_v = std::array{
    std::byte{0xff}, std::byte{0xff}, std::byte{0xff}, std::byte{0xff}
  };
  // fallback bitmap.
  constexpr auto white_bitmap_v = framework::vma::Bitmap{
    .bytes = white_pixel_v,
    .size = {1, 1},
  };
} // namespace

namespace framework {
  [[nodiscard]] constexpr auto create_sampler_info(
    vk::SamplerAddressMode const wrap, vk::Filter const filter
  ) {
    auto sampler_info =
      vk::SamplerCreateInfo()
        .setAddressModeU(wrap)
        .setAddressModeV(wrap)
        .setAddressModeW(wrap)
        .setMinFilter(filter)
        .setMagFilter(filter)
        .setMaxLod(VK_LOD_CLAMP_NONE)
        .setBorderColor(vk::BorderColor::eFloatTransparentBlack)
        .setMipmapMode(vk::SamplerMipmapMode::eNearest);

    return sampler_info;
  }

  constexpr auto sampler_info = create_sampler_info(
    vk::SamplerAddressMode::eClampToEdge, vk::Filter::eLinear
  );

  struct TextureCreateInfo {
    vk::Device device;
    VmaAllocator allocator;
    std::uint32_t queue_family;
    CommandBlock command_block;
    vma::Bitmap bitmap;

    vk::SamplerCreateInfo sampler{sampler_info};
  };

  export class Texture {
  public:
    using CreateInfo = TextureCreateInfo;

    explicit Texture(CreateInfo create_info) {
      if (create_info.bitmap.bytes.empty() || create_info.bitmap.size.x <= 0 ||
          create_info.bitmap.size.y <= 0) {
        create_info.bitmap = white_bitmap_v;
      }

      auto const image_ci = vma::ImageCreateInfo{
        .allocator = create_info.allocator,
        .queue_family = create_info.queue_family,
      };
      image = vma::create_sampled_image(
        image_ci, std::move(create_info.command_block), create_info.bitmap
      );

      auto image_view_ci = vk::ImageViewCreateInfo{};
      auto subresource_range = vk::ImageSubresourceRange{};
      subresource_range.setAspectMask(vk::ImageAspectFlagBits::eColor)
        .setLayerCount(1)
        .setLevelCount(image.get().levels);

      image_view_ci.setImage(image.get().image)
        .setViewType(vk::ImageViewType::e2D)
        .setFormat(image.get().format)
        .setSubresourceRange(subresource_range);
      view = create_info.device.createImageViewUnique(image_view_ci);

      sampler = create_info.device.createSamplerUnique(create_info.sampler);
    }

    [[nodiscard]] auto descriptor_info() const -> vk::DescriptorImageInfo {
      return vk::DescriptorImageInfo()
        .setImageView(*view)
        .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
        .setSampler(*sampler);
    }

  private:
    vma::Image image;
    vk::UniqueImageView view;
    vk::UniqueSampler sampler;
  };
} // namespace framework
