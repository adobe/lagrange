/*
 * Copyright 2022 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#pragma once

#include <string_view>


/// @defgroup group-utils Utilites
/// @ingroup module-core
/// Utility functions.
/// @{

///
/// @defgroup   group-utils-assert Assert and errors
/// @ingroup    group-utils
///
/// Assertions and exceptions.
///
/// A failed assertion will throw an exception lagrange::Error with a specific error message,
/// and can be caught by client code. Lagrange code uses two types of assertions:
///
/// - Runtime assertions la_runtime_assert(). Those are used to check the validity of user inputs as
///   a pre-condition to a function. For example, providing a function with an empty, or a mesh with
///   incorrect dimension, could result in such an assertion being raised. A runtime assert is
///   executed even in Release configuration, and indicates a problem with the function input.
///
/// - Debug assertions la_debug_assert(). Those are only checked in Debug code (macro `NDEBUG` is
///   undefined). A debug assert is used to check internal code validity, and should _not_ be
///   triggered under any circumstance by client code. A failed debug assert indicates an error in
///   the programmer's code, and should be fixed accordingly.
///
/// Note that assert macros behave as functions, meaning they expand to a `void` expression and can
/// be used in an expression such as `y = (la_debug_assert(true), x)`.
///
/// Finally, our assertion macros can take either 1 or 2 arguments, the second argument being an
/// optional error message. To conveniently format assertion messages with a printf-like syntax, use
/// `fmt::format`:
///
/// @code
/// #include <lagrange/utils/assert.h>
/// #include <spdlog/fmt/fmt.h>
///
/// la_debug_assert(x == 3);
/// la_debug_assert(x == 3, "Error message");
/// la_debug_assert(x == 3, fmt::format("Incorrect value of x: {}", x));
/// @endcode
///
/// @{

namespace lagrange {

/// @addtogroup group-utils-assert
/// @{

///
/// Sets whether to trigger a debugger breakpoint on assert failure. Use this function in unit test,
/// around lines that are supposed to throw, to avoid spurious breakpoints when running unit tests.
///
/// @param[in]  enabled  True to enable breakpoint debugging, false to disable.
///
void set_breakpoint_enabled(bool enabled);

///
/// Returns whether to trigger a debugger breakpoint on assert failure. Use this function in a unit
/// test to restore previous behavior when explicitly disabling triggering debugger breakpoint.
///
/// @return     True if assert failure triggers a debugger breakpoint, False otherwise.
///
bool is_breakpoint_enabled();

///
/// Call to explicitly trigger a debugger breakpoint.
///
void trigger_breakpoint();

///
/// Called in case of an assertion failure.
///
/// @param[in]  function   Function in which the assertion failed.
/// @param[in]  file       File in which the assertion failed.
/// @param[in]  line       Line where the assertion failed.
/// @param[in]  condition  Assert condition that failed.
/// @param[in]  message    Optional error message associated with the assertion.
///
/// @return     Boolean type returned by this function. While this function never returns, the
///             return boolean type allows it to be called in an expression such as `foo &&
///             assertion_failed(...)`
///
[[noreturn]] bool assertion_failed(
    const char* function,
    const char* file,
    unsigned int line,
    const char* condition,
    std::string_view message);

/// @}

} // namespace lagrange

// -----------------------------------------------------------------------------

/// @cond LA_INTERNAL_DOCS

#define LA_INTERNAL_EXPAND(x) x

#define LA_INTERNAL_NARG2(...) \
    LA_INTERNAL_EXPAND(LA_INTERNAL_NARG1(__VA_ARGS__, LA_INTERNAL_RSEQN()))
#define LA_INTERNAL_NARG1(...) LA_INTERNAL_EXPAND(LA_INTERNAL_ARGSN(__VA_ARGS__))
#define LA_INTERNAL_ARGSN(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, N, ...) N
#define LA_INTERNAL_RSEQN() 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0

#define LA_INTERNAL_FUNC2(name, n) name##n
#define LA_INTERNAL_FUNC1(name, n) LA_INTERNAL_FUNC2(name, n)

#define LA_INTERNAL_GET_MACRO(func, ...) \
    LA_INTERNAL_EXPAND(                  \
        LA_INTERNAL_FUNC1(func, LA_INTERNAL_EXPAND(LA_INTERNAL_NARG2(__VA_ARGS__)))(__VA_ARGS__))

// -----------------------------------------------------------------------------

#if defined(__GNUC__) || defined(__clang__)
#define LA_ASSERT_FUNCTION __PRETTY_FUNCTION__
#elif defined(_MSC_VER)
#define LA_ASSERT_FUNCTION __FUNCSIG__
#elif defined(__SUNPRO_CC)
#define LA_ASSERT_FUNCTION __func__
#else
#define LA_ASSERT_FUNCTION __FUNCTION__
#endif

// -----------------------------------------------------------------------------

// Note: In order for the `la_xxx_assert()` macro to behave as a function, it needs to expand to an
// expression. This means we cannot use `if` or `do ... while` statements. The only options are
// ternary operators ?: or logical boolean using &&.
#define LA_INTERNAL_ASSERT_ARGS_2(condition, message) \
    ((void)(!static_cast<bool>((condition)) && ::lagrange::assertion_failed(LA_ASSERT_FUNCTION, __FILE__, __LINE__, #condition, message)))
#define LA_INTERNAL_ASSERT_ARGS_1(condition) LA_INTERNAL_ASSERT_ARGS_2(condition, "")

#define LA_INTERNAL_IGNORE_ARGS_2(condition, message) ((void)(condition))
#define LA_INTERNAL_IGNORE_ARGS_1(condition) LA_INTERNAL_IGNORE_ARGS_2(condition, "")

/// @endcond

// -----------------------------------------------------------------------------

///
/// Runtime assertion check. This check is executed for both Debug and Release configurations, and
/// should be used, e.g., to check the validity of user-given inputs.
///
/// @hideinitializer
///
/// @param      condition  Condition to check at runtime.
/// @param      message    Optional message argument.
///
/// @return     Void expression.
///
#ifndef la_runtime_assert
#define la_runtime_assert(...) \
    LA_INTERNAL_EXPAND(LA_INTERNAL_GET_MACRO(LA_INTERNAL_ASSERT_ARGS_, __VA_ARGS__))
#endif

///
/// Debug assertion check. This check is executed only for Debug configurations, and should be used
/// as a sanity check for situations that should _never_ arise in the program's normal execution
/// (e.g., a doubled linked list is malformed).
/// @hideinitializer
///
/// @param      condition  Condition to check at runtime.
/// @param      message    Optional message argument.
///
/// @return     Void expression.
///
#ifndef la_debug_assert
#ifdef NDEBUG
#define la_debug_assert(...) \
    LA_INTERNAL_EXPAND(LA_INTERNAL_GET_MACRO(LA_INTERNAL_IGNORE_ARGS_, __VA_ARGS__))
#else
#define la_debug_assert(...) \
    LA_INTERNAL_EXPAND(LA_INTERNAL_GET_MACRO(LA_INTERNAL_ASSERT_ARGS_, __VA_ARGS__))
#endif // NDEBUG
#endif // la_debug_assert

/// @}
