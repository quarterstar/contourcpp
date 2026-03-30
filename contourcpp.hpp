#pragma once

#include <concepts>
#include <expected>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

template <typename T>
struct __is_expected : std::false_type {};

template <typename E>
struct __is_expected<std::unexpected<E>> : std::true_type {};

template <typename T, typename E>
struct __is_expected<std::expected<T, E>> : std::true_type {};

// `maybe` macros:
//
// These macros are intended to resemble Rust's question mark operator, but with use within in the
// context of `std::expected` and `std::optional`. The correct overload is picked with a macro
// router.
//
// A "maybe bad value" is `std::optional<T>` or `std::expected<T, E>` or other types with a
// compatible interface.
//
// * First argument: the destination variable name
// * Second argument (optional): the expression that yields a modified bad value
//
// NOTE: Within the second argument, you can use the bad value (if it exists) by referring to
// `__maybe_error`

/// @brief Internal helper to extract error data from optionals or expecteds.
template <typename T>
auto __get_error(T&& container) -> decltype(auto) {
  if constexpr (requires { container.error(); }) {
    return std::forward<T>(container).error();
  } else {
    return nullptr;
  }
}

template <typename F, typename T>
auto __get_result(F&& fallback, T&& error) -> decltype(auto) {
  if constexpr (std::invocable<F, T>) {
    return std::forward<F>(fallback)(std::forward<T>(error));
  } else {
    return std::forward<F>(fallback);
  }
}

template <typename T>
concept __contextual_bool = requires(T&& v) { static_cast<bool>(std::forward<T>(v)); };

template <typename T>
concept __referencable = std::is_reference_v<std::add_lvalue_reference_t<T>>;

template <typename Storage>
struct __maybe_failure_proxy {
private:
  using Self = __maybe_failure_proxy;

public:
  struct dummy_t {};
  std::variant<dummy_t, Storage> value;

  __maybe_failure_proxy() : value {dummy_t {}} {
  }

  __maybe_failure_proxy(Storage e) : value {std::move(e)} {
  }

  template <typename T>
  operator std::optional<T>([[maybe_unused]] this const Self& _) {
    return std::nullopt;
  }

  template <typename T, typename E>
  operator std::expected<T, E>(this Self&& self) {
    if (std::holds_alternative<dummy_t>(self.value)) {
      return std::unexpected(E {});
    }

    auto&& value = std::get<Storage>(std::move(self.value));
    using value_type = std::decay_t<Storage>;

    if constexpr (__is_expected<value_type>::value) {
      return std::forward<value_type>(value);
    } else {
      return std::unexpected<E>(std::forward<value_type>(value));
    }
  }
};

template <typename T>
__maybe_failure_proxy(T&&) -> __maybe_failure_proxy<std::decay_t<T>>;

/// @brief Macro router to support different argument counts.
#define GET_maybe_MACRO(_1, _2, NAME, ...) NAME
#define maybe(...) GET_maybe_MACRO(__VA_ARGS__, maybe_2, maybe_1)(__VA_ARGS__)

/// @brief Case 1: maybe(expression)
/// @details
/// Used for statements where the return value isn't captured.
/// Example: maybe(self.update());
#define maybe_1(expr)                                                                              \
  ({                                                                                               \
    auto&& __result {(expr)};                                                                      \
    if (!__result) {                                                                               \
      return ::__maybe_failure_proxy {::__get_error(__result)};                                    \
    }                                                                                              \
    using __container_type = std::decay_t<decltype(__result)>;                                     \
    ([&] -> auto {                                                                                 \
      if constexpr (std::is_void_v<__container_type::value_type>) {                                \
        return;                                                                                    \
      } else {                                                                                     \
        return *std::move(__result);                                                               \
      }                                                                                            \
    })();                                                                                          \
  })

/// @brief Case 2: maybe(expression, fallback_expression)
/// @details
/// Evaluates to the inner value of the expected/optional.
/// If an error occurs, returns the fallback_expression from the parent function.
///
/// NOTE: `__e` is the error and available within the fallback_expression.
///
/// Example: auto val = maybe(get_val(), __e);
#define maybe_2(expr, fallback)                                                                    \
  ({                                                                                               \
    auto&& __result {(expr)};                                                                      \
    if (!__result) {                                                                               \
      [[maybe_unused]] auto&& __e = ::__get_error(__result);                                       \
      return ::__maybe_failure_proxy {__get_result(fallback, __e)};                                \
    }                                                                                              \
    using __container_type = std::decay_t<decltype(__result)>;                                     \
    ([&] -> auto {                                                                                 \
      if constexpr (std::is_void_v<__container_type::value_type>) {                                \
        return;                                                                                    \
      } else {                                                                                     \
        return *std::move(__result);                                                               \
      }                                                                                            \
    })();                                                                                          \
  })
