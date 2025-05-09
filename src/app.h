#pragma once

#include "window.h"

namespace lvk {
  class App {
  private:
    glfw::Window window{};

    void create_window();

    void main_loop();

  public:
    void run();
  };
}
