module;

#include <array>
#include <cstddef>

export module framework_module:resource_buffering;

namespace framework_module {
  /// Number of virtual frames
  inline constexpr std::size_t resource_buffering{2};

  /// Alias for N-buffered resources
  template <typename Type>
  using Buffered = std::array<Type, resource_buffering>;
}
