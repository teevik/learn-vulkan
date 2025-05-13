module;

#include <vulkan/vulkan.hpp>

export module framework_module:scoped_waiter;
import :scoped;

namespace framework_module {
  export struct ScopedWaiterDeleter {
    void operator()(vk::Device const device) const noexcept {
      device.waitIdle();
    }
  };

  export using ScopedWaiter = Scoped<vk::Device, ScopedWaiterDeleter>;
}
