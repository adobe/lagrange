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

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <spdlog/spdlog.h>
#include <lagrange/utils/warnon.h>
// clang-format on

namespace lagrange {

///
/// @defgroup   group-logger Logging
/// @ingroup    module-core
///
/// Functions related to logging.
///
/// To use the logger in your application, simply include this header, and call
///
/// @code
/// #include <lagrange/Logger.h>
///
/// lagrange::logger().info("This is a message");
/// lagrange::logger().warning("Invalid value for x: {}", x);
///
/// // set logger verbosity
/// lagrange::logger().set_level(spdlog::level::debug);
/// @endcode
///
/// The Lagrange logger is a plain spdlog logger, with a type-safe
/// printf formatting syntax provided by fmt. For more details,
/// please consult these libraries' respective documentations:
/// - [spdlog documentation](https://github.com/gabime/spdlog/wiki/1.-QuickStart).
/// - [fmt documentation](https://fmt.dev/latest/index.html).
///
/// @{

///
/// Retrieves the current logger.
///
/// @return     A const reference to Lagrange's logger object.
///
spdlog::logger& logger();

///
/// Setup a logger object to be used by Lagrange. Calling this function with other Lagrange function
/// is not thread-safe.
///
/// @param[in]  logger  New logger object to be used by Lagrange. Ownership is shared with Lagrange.
///
void set_logger(std::shared_ptr<spdlog::logger> logger);

///
/// Changes the level of logger to something else in a scope. Mostly used in unit tests. Don't use
/// in the library itself.
///
class ScopedLogLevel
{
public:
    ///
    /// Changes the level of logger to something else in a scope. Mostly used in unit tests. Don't
    /// use in the library itself.
    ///
    /// @param[in]  level   New level to set temporarily.
    /// @param[in]  logger  Which logger to apply this to, defaults to the Lagrange logger.
    ///
    ScopedLogLevel(spdlog::level::level_enum level, spdlog::logger& logger = lagrange::logger());
    ~ScopedLogLevel();

    ScopedLogLevel(ScopedLogLevel&&) = delete;
    ScopedLogLevel& operator=(ScopedLogLevel&&) = delete;
    ScopedLogLevel(const ScopedLogLevel&) = delete;
    ScopedLogLevel& operator=(const ScopedLogLevel&) = delete;

private:
    spdlog::level::level_enum m_prev_level;
    spdlog::logger* m_logger;
};

/// @}

} // namespace lagrange
