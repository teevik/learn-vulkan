#include "app.h"

namespace lvk {
  void App::run() {
    create_window();

    main_loop();
  }

  void App::create_window() {
    window = glfw::create_window({1280, 720}, "Learn Vulkan");
  }

  void App::main_loop() {
    while (glfwWindowShouldClose(window.get()) == GLFW_FALSE) {
      glfwPollEvents();
    }
  }
}
