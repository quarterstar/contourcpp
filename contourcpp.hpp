#pragma once

#include <expected>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

// `maybe` macros:
//
// These macros are intended to resemble Rust's question mark operator, but with
// use within in the context of `std::expected` and `std::optional`. The correct
// overload is picked with a macro router.
//
// A "maybe bad value" is `std::optional<T>` or `std::expected<T, E>` or other
// types with a compatible interface.
//
// * First argument: the destination variable name
// * Second argument (optional): the expression that yields a modified bad value
//
// NOTE: Within the second argument, you can use the bad value (if it exists) by
// referring to
// `__maybe_error`

/**
 * @brief Internal helper to extract error data from optionals or expecteds.
 */
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
  operator std::expected<T, E>(this const Self& self) {
    if (std::holds_alternative<Storage>(self.error_storage)) {
      return std::unexpected(std::get<Storage>(self.error_storage));
    }
    return std::unexpected(E {});
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
    auto&& __res = (expr);                                                                         \
    if (!__res) {                                                                                  \
      auto&& __maybe_error = ::__get_error(__res);                                                 \
      using __maybe_error_type = std::decay_t<decltype(__maybe_error)>;                            \
      if constexpr (requires { __res.error(); }) {                                                 \
        return ::__maybe_failure_proxy<__maybe_error_type>(__res.error());                         \
      } else {                                                                                     \
        return ::__maybe_failure_proxy<__maybe_error_type>();                                      \
      }                                                                                            \
    }                                                                                              \
    *std::move(__res);                                                                             \
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
    auto&& __res = (expr);                                                                         \
    if (!__res) {                                                                                  \
      [[maybe_unused]] auto&& __maybe_error = ::__get_error(__res);                                \
      return (fallback);                                                                           \
    }                                                                                              \
    *std::move(__res);                                                                             \
  })
