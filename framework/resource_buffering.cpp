module;

#include <array>
#include <cstddef>

export module framework:resource_buffering;

namespace framework {
  /// Number of virtual frames
  inline constexpr std::size_t resource_buffering{2};

  /// Alias for N-buffered resources
  export template <typename Type>
  using Buffered = std::array<Type, resource_buffering>;
}
