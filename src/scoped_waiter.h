#pragma once

#include "scoped.h"

namespace lvk {
  struct ScopedWaiterDeleter {
    void operator()(vk::Device const device) const noexcept {
      device.waitIdle();
    }
  };

  using ScopedWaiter = Scoped<vk::Device, ScopedWaiterDeleter>;
}
