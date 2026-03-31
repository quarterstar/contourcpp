#pragma once

// ====================================================
// ContourCpp - Bringing fuctional programming into C++
// Version INDEV
// Made by Quarterstar
// ----------------------------------------------------
//
// Copyright (c) 2026 quarterstar
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
// ====================================================

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
auto __deref_or_void(T&& container) -> decltype(auto) {
  using value_type = typename std::decay_t<T>::value_type;
  if constexpr (std::is_void_v<value_type>) {
    return;
  } else {
    return *std::forward<T>(container);
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
  Storage value;

  template <typename T>
  operator std::optional<T>([[maybe_unused]] this const Self& _) {
    return std::nullopt;
  }

  template <typename T, typename E>
  operator std::expected<T, E>(this Self&& self) {
    using value_type = std::decay_t<Storage>;

    if constexpr (std::is_same_v<value_type, std::nullptr_t>) {
      static_assert(
        std::default_initializable<E>,
        "E must be default-initializable to convert from optional failure."
      );
      return std::unexpected(E {});
    } else if constexpr (__is_expected<value_type>::value) {
      return std::forward<value_type>(self.value);
    } else {
      return std::unexpected<E>(std::forward<value_type>(self.value));
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
    ::__deref_or_void(std::move(__result));                                                        \
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
      [[maybe_unused]] auto&& __e {::__get_error(__result)};                                       \
      return ::__maybe_failure_proxy {__get_result(fallback, __e)};                                \
    }                                                                                              \
    ::__deref_or_void(std::move(__result));                                                        \
  })

// Helpers

#define GET_maybe_twice_MACRO(_1, _2, NAME, ...) NAME
#define maybe_twice(...)                                                                           \
  GET_maybe_twice_MACRO(__VA_ARGS__, maybe_twice_2, maybe_twice_1)(__VA_ARGS__)

#define GET_maybe_thrice_MACRO(_1, _2, NAME, ...) NAME
#define maybe_thrice(...)                                                                          \
  GET_maybe_thrice_MACRO(__VA_ARGS__, maybe_thrice_2, maybe_thrice_1)(__VA_ARGS__)

#define maybe_twice_1(expr) maybe(maybe(expr))
#define maybe_twice_2(expr, fallback) maybe(maybe(expr, fallback), fallback)

#define maybe_thrice_1(expr) maybe(maybe(maybe(expr)))
#define maybe_thrice_2(expr, fallback) maybe(maybe(maybe(expr, fallback), fallback), fallback)
