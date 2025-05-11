#include "assets.h"
#include <fstream>
#include <print>

namespace lvk {
  auto locate_assets_dir() -> fs::path {
    static constexpr std::string_view dir_name{"assets"};

    for (auto path = fs::current_path();
         !path.empty() && path.has_parent_path();
         path = path.parent_path()) {

      // Exit early if path is "/"
      if (path == fs::path("/")) break;

      auto assets_dir = path / dir_name;

      if (fs::is_directory(assets_dir)) return assets_dir;
    }

    std::println("[lvk] Warning: could not locate '{}' directory", dir_name);

    return fs::current_path();
  }

  auto read_spir_v(fs::path const &path) -> std::vector<std::uint32_t> {
    // Open the file at the end, to get the total size.
    auto file = std::ifstream{path, std::ios::binary | std::ios::ate};
    if (!file.is_open()) {
      throw std::runtime_error{
        std::format("Failed to open file: '{}'", path.generic_string())
      };
    }

    auto const size = file.tellg();
    auto const usize = static_cast<std::uint64_t>(size);
    // file data must be uint32 aligned.
    if (usize % sizeof(std::uint32_t) != 0) {
      throw std::runtime_error{std::format("Invalid SPIR-V size: {}", usize)};
    }

    // Seek to the beginning before reading.
    file.seekg({}, std::ios::beg);
    auto ret = std::vector<std::uint32_t>{};
    ret.resize(usize / sizeof(std::uint32_t));
    void *data = ret.data();
    file.read(static_cast<char *>(data), size);
    return ret;
  }
} // namespace lvk
