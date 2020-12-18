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
//
// The following macros allows one to surgically disable specific warnings
// around a block of code.  E.g.
//
// ```
// #include <lagrange/utils/warning.h>
//
// LA_DISABLE_WARNING_BEGIN
// LA_DISABLE_WARNING(unused-local-typedefs)
// ...
// <code that may cause warning>
// ...
// LA_DISABLE_WARNING_END
//

// clang-format off
#if defined(__clang__)
#define LA_PRAGMA(X) _Pragma(#X)
#define LA_DISABLE_WARNING_BEGIN LA_PRAGMA(clang diagnostic push)
#define LA_DISABLE_WARNING_END LA_PRAGMA(clang diagnostic pop)
#define LA_DISABLE_WARNING(warning_name) LA_PRAGMA(clang diagnostic ignored #warning_name)

// Ignore shadow warnings
#define LA_IGNORE_SHADOW_WARNING_BEGIN LA_DISABLE_WARNING_BEGIN \
    LA_DISABLE_WARNING(-Wshadow)
#define LA_IGNORE_SHADOW_WARNING_END LA_DISABLE_WARNING_END

// Ignore deprecation warnings
#define LA_IGNORE_DEPRECATION_WARNING_BEGIN LA_DISABLE_WARNING_BEGIN \
    LA_DISABLE_WARNING(-Wdeprecated-declarations)
#define LA_IGNORE_DEPRECATION_WARNING_END LA_DISABLE_WARNING_END

// Ignore unused local typedefs
#define LA_IGNORE_UNUSED_TYPEDEF_BEGIN LA_DISABLE_WARNING_BEGIN \
    LA_DISABLE_WARNING(-Wunused-local-typedefs)
#define LA_IGNORE_UNUSED_TYPEDEF_END LA_DISABLE_WARNING_END


#elif defined(__GNUC__)
#define LA_PRAGMA(X) _Pragma(#X)
#define LA_DISABLE_WARNING_BEGIN LA_PRAGMA(GCC diagnostic push)
#define LA_DISABLE_WARNING_END LA_PRAGMA(GCC diagnostic pop)
#define LA_DISABLE_WARNING(warning_name) LA_PRAGMA(GCC diagnostic ignored #warning_name)

// Ignore shadow warnings
#define LA_IGNORE_SHADOW_WARNING_BEGIN LA_DISABLE_WARNING_BEGIN \
    LA_DISABLE_WARNING(-Wshadow)
#define LA_IGNORE_SHADOW_WARNING_END LA_DISABLE_WARNING_END

// Ignore deprecation warnings
#define LA_IGNORE_DEPRECATION_WARNING_BEGIN LA_DISABLE_WARNING_BEGIN \
    LA_DISABLE_WARNING(-Wdeprecated-declarations)
#define LA_IGNORE_DEPRECATION_WARNING_END LA_DISABLE_WARNING_END

// Ignore unused local typedefs
#define LA_IGNORE_UNUSED_TYPEDEF_BEGIN LA_DISABLE_WARNING_BEGIN \
    LA_DISABLE_WARNING(-Wunused-local-typedefs)
#define LA_IGNORE_UNUSED_TYPEDEF_END LA_DISABLE_WARNING_END


#elif defined(_MSC_VER)
#define LA_DISABLE_WARNING_BEGIN __pragma(warning(push))
#define LA_DISABLE_WARNING_END __pragma(warning(pop))
#define LA_DISABLE_WARNING(warning_number) __pragma(warning( disable : warning_number ))

// Ignore shadow warnings
#define LA_IGNORE_SHADOW_WARNING_BEGIN
#define LA_IGNORE_SHADOW_WARNING_END

// Ignore deprecation warnings
#define LA_IGNORE_DEPRECATION_WARNING_BEGIN LA_DISABLE_WARNING_BEGIN \
    LA_DISABLE_WARNING(4996)
#define LA_IGNORE_DEPRECATION_WARNING_END LA_DISABLE_WARNING_END

// Ignore shadow warnings
#define LA_IGNORE_UNUSED_TYPEDEF_BEGIN
#define LA_IGNORE_UNUSED_TYPEDEF_END

#else
#define LA_DISABLE_WARNING_BEGIN
#define LA_DISABLE_WARNING_END
#define LA_DISABLE_WARNING(X)
#define LA_IGNORE_SHADOW_WARNING_BEGIN
#define LA_IGNORE_SHADOW_WARNING_END
#define LA_IGNORE_DEPRECATION_WARNING_BEGIN
#define LA_IGNORE_DEPRECATION_WARNING_END
#define LA_IGNORE_UNUSED_TYPEDEF_BEGIN
#define LA_IGNORE_UNUSED_TYPEDEF_END
#endif

#define LA_IGNORE(x) (void)x
// clang-format on
