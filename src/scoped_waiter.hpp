#pragma once

#include "scoped.hpp"

namespace lvk {
  struct ScopedWaiterDeleter {
    void operator()(vk::Device const device) const noexcept {
      device.waitIdle();
    }
  };

  using ScopedWaiter = Scoped<vk::Device, ScopedWaiterDeleter>;
}
