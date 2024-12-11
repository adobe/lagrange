/*
 * Copyright 2020 Adobe. All rights reserved.
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

/// @defgroup group-utils-warning Warnings
/// @ingroup group-utils
/// Managing compiler warnings.
///
/// The following macros allows one to surgically disable specific warnings
/// around a block of code.  E.g.
///
/// ```
/// #include <lagrange/utils/warning.h>
///
/// LA_IGNORE_DEPRECATION_WARNING_BEGIN
/// <code that may cause warning>
/// LA_IGNORE_DEPRECATION_WARNING_END
/// ```
/// @{

// clang-format off

/// @cond LA_INTERNAL_DOCS
#if defined(__clang__)
    #define LA_PRAGMA(X) _Pragma(#X)
    #define LA_DISABLE_WARNING_BEGIN LA_PRAGMA(clang diagnostic push)
    #define LA_DISABLE_WARNING_END LA_PRAGMA(clang diagnostic pop)
    #define LA_DISABLE_WARNING_CLANG(warning_name) LA_PRAGMA(clang diagnostic ignored #warning_name)
    #define LA_DISABLE_WARNING_GCC(warning_name)
    #define LA_DISABLE_WARNING_MSVC(warning_number)
    #define LA_NOSANITIZE_SIGNED_INTEGER_OVERFLOW __attribute__((no_sanitize("signed-integer-overflow")))
#elif defined(__GNUC__)
    #define LA_PRAGMA(X) _Pragma(#X)
    #define LA_DISABLE_WARNING_BEGIN LA_PRAGMA(GCC diagnostic push)
    #define LA_DISABLE_WARNING_END LA_PRAGMA(GCC diagnostic pop)
    #define LA_DISABLE_WARNING_CLANG(warning_name)
    #define LA_DISABLE_WARNING_GCC(warning_name) LA_PRAGMA(GCC diagnostic ignored #warning_name)
    #define LA_DISABLE_WARNING_MSVC(warning_number)
    #define LA_NOSANITIZE_SIGNED_INTEGER_OVERFLOW
#elif defined(_MSC_VER)
    #define LA_DISABLE_WARNING_BEGIN __pragma(warning(push))
    #define LA_DISABLE_WARNING_END __pragma(warning(pop))
    #define LA_DISABLE_WARNING_CLANG(warning_name)
    #define LA_DISABLE_WARNING_GCC(warning_name)
    #define LA_DISABLE_WARNING_MSVC(warning_number) __pragma(warning( disable : warning_number ))
    #define LA_NOSANITIZE_SIGNED_INTEGER_OVERFLOW
#else
    #define LA_DISABLE_WARNING_BEGIN
    #define LA_DISABLE_WARNING_END
    #define LA_DISABLE_WARNING_CLANG(warning_name)
    #define LA_DISABLE_WARNING_GCC(warning_name)
    #define LA_DISABLE_WARNING_MSVC(warning_number)
    #define LA_NOSANITIZE_SIGNED_INTEGER_OVERFLOW
#endif
/// @endcond

/// Ignore shadow warnings
/// @hideinitializer
#define LA_IGNORE_SHADOW_WARNING_BEGIN LA_DISABLE_WARNING_BEGIN \
    LA_DISABLE_WARNING_CLANG(-Wshadow) \
    LA_DISABLE_WARNING_GCC(-Wshadow)
/// @hideinitializer
#define LA_IGNORE_SHADOW_WARNING_END LA_DISABLE_WARNING_END

/// Ignore deprecation warnings
/// @hideinitializer
#define LA_IGNORE_DEPRECATION_WARNING_BEGIN LA_DISABLE_WARNING_BEGIN \
    LA_DISABLE_WARNING_CLANG(-Wdeprecated-declarations) \
    LA_DISABLE_WARNING_GCC(-Wdeprecated-declarations) \
    LA_DISABLE_WARNING_MSVC(4996)
/// @hideinitializer
#define LA_IGNORE_DEPRECATION_WARNING_END LA_DISABLE_WARNING_END

/// Ignore unused local typedefs
/// @hideinitializer
#define LA_IGNORE_UNUSED_TYPEDEF_BEGIN LA_DISABLE_WARNING_BEGIN \
    LA_DISABLE_WARNING_CLANG(-Wunused-local-typedefs) \
    LA_DISABLE_WARNING_GCC(-Wunused-local-typedefs)
/// @hideinitializer
#define LA_IGNORE_UNUSED_TYPEDEF_END LA_DISABLE_WARNING_END

/// Ignore exit time destructors
/// @hideinitializer
#define LA_IGNORE_EXIT_TIME_DTOR_WARNING_BEGIN LA_DISABLE_WARNING_BEGIN \
    LA_DISABLE_WARNING_CLANG(-Wexit-time-destructors)
/// @hideinitializer
#define LA_IGNORE_EXIT_TIME_DTOR_WARNING_END LA_DISABLE_WARNING_END

/// Ignore documentation errors
/// @hideinitializer
#define LA_IGNORE_DOCUMENTATION_WARNING_BEGIN LA_DISABLE_WARNING_BEGIN \
    LA_DISABLE_WARNING_CLANG(-Wdocumentation)
/// @hideinitializer
#define LA_IGNORE_DOCUMENTATION_WARNING_END LA_DISABLE_WARNING_END

/// Ignore self assign overloaded
/// @hideinitializer
#define LA_IGNORE_SELF_ASSIGN_WARNING_BEGIN LA_DISABLE_WARNING_BEGIN \
    LA_DISABLE_WARNING_CLANG(-Wself-assign-overloaded)
/// @hideinitializer
#define LA_IGNORE_SELF_ASSIGN_WARNING_END LA_DISABLE_WARNING_END

/// Ignore self move
/// @hideinitializer
#if defined(__GNUC__) && __GNUC__ >= 13
// -Wself-move is introduced in GCC 13.
#define LA_IGNORE_SELF_MOVE_GCC LA_DISABLE_WARNING_GCC(-Wself-move)
#else
#define LA_IGNORE_SELF_MOVE_GCC
#endif
/// @hideinitializer
#define LA_IGNORE_SELF_MOVE_WARNING_BEGIN LA_DISABLE_WARNING_BEGIN \
    LA_DISABLE_WARNING_CLANG(-Wself-move) \
    LA_IGNORE_SELF_MOVE_GCC \
    LA_DISABLE_WARNING_MSVC(26800)
/// @hideinitializer
#define LA_IGNORE_SELF_MOVE_WARNING_END LA_DISABLE_WARNING_END

/// Ignore switch enum
/// @hideinitializer
#define LA_IGNORE_SWITCH_ENUM_WARNING_BEGIN LA_DISABLE_WARNING_BEGIN \
    LA_DISABLE_WARNING_CLANG(-Wswitch-enum)
/// @hideinitializer
#define LA_IGNORE_SWITCH_ENUM_WARNING_END LA_DISABLE_WARNING_END

/// Ignore warning "function declared with 'noreturn' has non-void return type"
/// @hideinitializer
#define LA_IGNORE_NONVOID_NORETURN_WARNING_BEGIN LA_DISABLE_WARNING_BEGIN \
    LA_DISABLE_WARNING_MSVC(4646)
/// @hideinitializer
#define LA_IGNORE_NONVOID_NORETURN_WARNING_END LA_DISABLE_WARNING_END

/// Ignore x to avoid an unused variable warning
/// @hideinitializer
#define LA_IGNORE(x) (void)x

/// Ignore range loop variable may be a copy
/// @hideinitializer
#define LA_IGNORE_RANGE_LOOP_ANALYSIS_BEGIN LA_DISABLE_WARNING_BEGIN \
    LA_DISABLE_WARNING_CLANG(-Wrange-loop-analysis)
/// @hideinitializer
#define LA_IGNORE_RANGE_LOOP_ANALYSIS_END LA_DISABLE_WARNING_END

/// Ignore warning "out of bounds subscripts or offsets into arrays"
/// This is used to bypass the following GCC bug:
/// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=106247
/// @hideinitializer
#define LA_IGNORE_ARRAY_BOUNDS_BEGIN LA_DISABLE_WARNING_BEGIN \
    LA_DISABLE_WARNING_GCC(-Warray-bounds)
    LA_DISABLE_WARNING_GCC(-Wstringop-overread)
/// @hideinitializer
#define LA_IGNORE_ARRAY_BOUNDS_END LA_DISABLE_WARNING_END

// clang-format on

/// @}
