#pragma once

#include <filesystem>

namespace fs = std::filesystem;

namespace lvk {
  /// Look for `<path>/assets/`, starting from the working
  /// directory and walking up the parent directory tree
  [[nodiscard]]
  auto locate_assets_dir() -> std::filesystem::path;

  /// Read a SPIR-V file from disk
  [[nodiscard]] auto read_spir_v(fs::path const &path)
    -> std::vector<std::uint32_t>;
} // namespace lvk
