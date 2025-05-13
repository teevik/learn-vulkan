module;

#include <vulkan/vulkan.hpp>

export module framework:scoped_waiter;
import :scoped;

namespace framework {
  export struct ScopedWaiterDeleter {
    void operator()(vk::Device const device) const noexcept {
      device.waitIdle();
    }
  };

  export using ScopedWaiter = Scoped<vk::Device, ScopedWaiterDeleter>;
}
