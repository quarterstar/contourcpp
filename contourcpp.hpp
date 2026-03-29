#pragma once

#include <concepts>
#include <expected>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

template <typename T>
auto __get_error(T&& container) -> decltype(auto) {
  if constexpr (requires { container.error(); }) {
    return std::forward<T>(container).error();
  } else {
    return nullptr;
  }
}

template <typename Storage>
struct __maybe_failure_proxy {
private:
  using Self = __maybe_failure_proxy;

public:
  struct dummy_t {};
  std::variant<dummy_t, Storage> error_storage;

  __maybe_failure_proxy() : error_storage(dummy_t {}) {
  }

  __maybe_failure_proxy(Storage e) : error_storage(std::move(e)) {
  }

  template <typename T>
  operator std::optional<T>([[maybe_unused]] this const Self& _) {
    return std::nullopt;
  }

  template <typename T, typename E>
  operator std::expected<T, E>(this Self&& self) {
    if (std::holds_alternative<dummy_t>(self.error_storage)) {
      return std::unexpected(E {});
    }

    auto&& value = std::get<Storage>(std::move(self.error_storage));
    using value_type = std::decay_t<Storage>;

    if constexpr (__is_unexpected<value_type>::value) {
      return std::forward<value_type>(value);
    } else {
      return std::unexpected<E>(std::forward<value_type>(value));
    }
  }
};

/**
 * @brief Macro router to support different argument counts.
 */
#define GET_maybe_MACRO(_1, _2, NAME, ...) NAME
#define maybe(...) GET_maybe_MACRO(__VA_ARGS__, maybe_2, maybe_1)(__VA_ARGS__)

/**
 * @brief Case 1: maybe(expression)
 * Used for statements where the return value isn't captured.
 * Example: maybe(self.update());
 */
#define maybe_1(expr)                                                                              \
  ({                                                                                               \
    auto&& __result {(expr)};                                                                      \
    if (!__result) {                                                                               \
      auto&& __maybe_error = ::__get_error(__result);                                              \
      using __maybe_error_type = std::decay_t<decltype(__maybe_error)>;                            \
      if constexpr (requires { __result.error(); }) {                                              \
        return ::__maybe_failure_proxy<__maybe_error_type>(__result.error());                      \
      } else {                                                                                     \
        return ::__maybe_failure_proxy<__maybe_error_type>();                                      \
      }                                                                                            \
    }                                                                                              \
    *std::move(__result);                                                                          \
  })

/**
 * @brief Case 2: maybe(expression, fallback_expression)
 * Evaluates to the inner value of the expected/optional.
 * If an error occurs, returns the fallback_expression from the parent function.
 *
 * NOTE: `__maybe_error` is available within the fallback_expression.
 *
 * Example: auto val = maybe(get_val(), std::unexpected(__maybe_error));
 */
#define maybe_2(expr, fallback)                                                                    \
  ({                                                                                               \
    auto&& __result {(expr)};                                                                      \
    if (!__result) {                                                                               \
      [[maybe_unused]] auto&& __maybe_error = ::__get_error(__result);                             \
      return (fallback);                                                                           \
    }                                                                                              \
    *std::move(__result);                                                                          \
  })
