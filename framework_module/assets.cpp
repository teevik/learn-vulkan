module;

#include <filesystem>
#include <fstream>
#include <print>
#include <vector>

export module framework_module:assets;

namespace fs = std::filesystem;

namespace framework_module {
  /// Look for `<path>/assets/`, starting from the working
  /// directory and walking up the parent directory tree
  export [[nodiscard]] auto locate_assets_dir() -> fs::path {
    static constexpr std::string_view dir_name{"assets"};

    for (auto path = fs::current_path();
         !path.empty() && path.has_parent_path();
         path = path.parent_path()) {

      // Exit early if path is "/"
      if (path == fs::path("/")) break;

      auto assets_dir = path / dir_name;

      if (fs::is_directory(assets_dir)) return assets_dir;
    }

    std::println(
      "[framework] Warning: could not locate '{}' directory", dir_name
    );

    return fs::current_path();
  }

  /// Read a SPIR-V file from disk
  export [[nodiscard]] auto read_spir_v(fs::path const &path)
    -> std::vector<std::uint32_t> {
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
} // namespace framework_module
