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
 #include <lagrange/utils/assert.h>

#include <lagrange/Logger.h>
#include <lagrange/utils/Error.h>

#if !defined(LA_ASSERT_DEBUG_BREAK)
#if defined(_WIN32)
extern void __cdecl __debugbreak(void);
#define LA_ASSERT_DEBUG_BREAK() __debugbreak()
#else
#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif
#if defined(__clang__) && !TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
#define LA_ASSERT_DEBUG_BREAK() __builtin_debugtrap()
#elif defined(linux) || defined(__linux) || defined(__linux__) || defined(__APPLE__)
#include <signal.h>
#define LA_ASSERT_DEBUG_BREAK() raise(SIGTRAP)
#elif defined(__GNUC__)
#define LA_ASSERT_DEBUG_BREAK() __builtin_trap()
#else
#define LA_ASSERT_DEBUG_BREAK() ((void)0)
#endif
#endif
#endif

namespace lagrange {

bool assertion_failed(
    const char* function,
    const char* file,
    unsigned int line,
    const char* condition,
    const std::string& message)
{
#ifdef LA_ENABLE_ASSERT_DEBUG_BREAK
    // Insert a breakpoint programmatically to automatically step into the debugger
    LA_ASSERT_DEBUG_BREAK();
#endif
    throw Error(fmt::format(
        "Assertion failed: \"{}\"{}{}\n"
        "\tIn file: {}, line {};\n"
        "\tIn function: {};",
        condition,
        (message.empty() ? "" : ", reason: "),
        message,
        file,
        line,
        function));
}

} // namespace lagrange
