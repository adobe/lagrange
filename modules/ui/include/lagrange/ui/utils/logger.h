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

#include <lagrange/Logger.h>
#include <lagrange/ui/Entity.h>

namespace lagrange {
namespace ui {

/// Log message once per registry lifetime
template <typename... Args>
void log_once(
    ui::Registry& r,
    spdlog::level::level_enum level,
    const std::string_view& fmt,
    Args&&... args)
{
    struct LogOnceCache
    {
        std::unordered_set<std::string> messages;
    };
    auto& msgs = r.ctx_or_set<LogOnceCache>().messages;
    if (msgs.count(fmt.data())) return;
    logger().log(level, fmt, std::forward<Args>(args)...);
    msgs.insert(fmt.data());
}


template <typename... Args>
void log_trace_once(ui::Registry& r, const std::string_view& fmt, Args&&... args)
{
    log_once(r, spdlog::level::trace, fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void log_debug_once(ui::Registry& r, const std::string_view& fmt, Args&&... args)
{
    log_once(r, spdlog::level::debug, fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void log_info_once(ui::Registry& r, const std::string_view& fmt, Args&&... args)
{
    log_once(r, spdlog::level::info, fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void log_warn_once(ui::Registry& r, const std::string_view& fmt, Args&&... args)
{
    log_once(r, spdlog::level::warn, fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void log_error_once(ui::Registry& r, const std::string_view& fmt, Args&&... args)
{
    log_once(r, spdlog::level::err, fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void log_critical_once(ui::Registry& r, const std::string_view& fmt, Args&&... args)
{
    log_once(r, spdlog::level::critical, fmt, std::forward<Args>(args)...);
}


} // namespace ui
} // namespace lagrange