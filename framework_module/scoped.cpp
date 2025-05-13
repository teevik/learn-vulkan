module;

#include <concepts>
#include <utility>

export module framework_module:scoped;

namespace framework_module {
  export template <typename Type>
  concept Scopeable =
    std::equality_comparable<Type> && std::is_default_constructible_v<Type>;

  export template <Scopeable Type, typename Deleter> class Scoped {
  public:
    Scoped(Scoped const &) = delete;
    auto operator=(Scoped const &) = delete;

    Scoped() = default;

    constexpr Scoped(Scoped &&rhs) noexcept :
      m_t(std::exchange(rhs.m_t, Type{})) {}

    constexpr auto operator=(Scoped &&rhs) noexcept -> Scoped & {
      if (&rhs != this) {
        std::swap(m_t, rhs.m_t);
      }
      return *this;
    }

    explicit(false) constexpr Scoped(Type t) : m_t(std::move(t)) {}

    constexpr ~Scoped() {
      if (m_t == Type{}) {
        return;
      }
      Deleter{}(m_t);
    }

    [[nodiscard]] auto get() const -> Type const & {
      return m_t;
    }
    [[nodiscard]] auto get() -> Type & {
      return m_t;
    }

  private:
    Type m_t{};
  };
} // namespace framework_module
