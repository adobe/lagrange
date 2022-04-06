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
#include <lagrange/Logger.h>

#include <lagrange/utils/warning.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <sstream>

namespace lagrange {

namespace {

// Custom logger instance defined by the user, if any
std::shared_ptr<spdlog::logger>& get_shared_logger()
{
    LA_IGNORE_EXIT_TIME_DTOR_WARNING_BEGIN
    static std::shared_ptr<spdlog::logger> logger;
    LA_IGNORE_EXIT_TIME_DTOR_WARNING_END
    return logger;
}

} // namespace

// Retrieve current logger
spdlog::logger& logger()
{
    if (get_shared_logger()) {
        return *get_shared_logger();
    } else {
        // When using factory methods provided by spdlog (_st and _mt functions),
        // names must be unique, since the logger is registered globally.
        // Otherwise, you will need to create the logger manually. See
        // https://github.com/gabime/spdlog/wiki/2.-Creating-loggers
        LA_IGNORE_EXIT_TIME_DTOR_WARNING_BEGIN
        static auto default_logger = spdlog::stdout_color_mt("lagrange");
        LA_IGNORE_EXIT_TIME_DTOR_WARNING_END
        return *default_logger;
    }
}

// Use a custom logger
void set_logger(std::shared_ptr<spdlog::logger> x)
{
    get_shared_logger() = std::move(x);
}

ScopedLogLevel::ScopedLogLevel(spdlog::level::level_enum lvl, spdlog::logger& logger_)
{
    m_logger = &logger_;
    m_prev_level = m_logger->level();
    m_logger->set_level(lvl);
}

ScopedLogLevel::~ScopedLogLevel()
{
    m_logger->set_level(m_prev_level);
}

} // namespace lagrange
