#include "app.h"
#include <print>

auto main() -> int {
  try {
    lvk::App{}.run();
  } catch (std::exception const &e) {
    std::println(stderr, "PANIC: {}", e.what());
    return EXIT_FAILURE;
  } catch (...) {
    std::println("PANIC!");
    return EXIT_FAILURE;
  }
}
