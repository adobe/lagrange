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
#include <spdlog/fmt/ostr.h> // TODO: Remove this include from Lagrange's header, and include on a case-by-case basis
#include <spdlog/spdlog.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <sstream>

namespace lagrange {

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
/// Changes the level of logger to something else in a scope.
/// Mostly used in unit tests. Don't use in the library itself.
///
/// @param[in]  level  New level to set temporarily.
/// @param[in]  logger  Which logger to apply this to, defaults to the lagrange logger
///
class ScopedLogLevel
{
public:
    ScopedLogLevel(spdlog::level::level_enum, spdlog::logger& = logger());
    ~ScopedLogLevel();

private:
    spdlog::level::level_enum m_prev_level;
    spdlog::logger* m_logger;
};


} // namespace lagrange
