/*
 * Copyright 2017 Adobe. All rights reserved.
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

#include <chrono>
#include <string>

#include <lagrange/Logger.h>

namespace lagrange {

using timestamp_type = std::chrono::time_point<std::chrono::steady_clock>;

inline void get_timestamp(timestamp_type* t)
{
    *t = std::chrono::steady_clock::now();
}

inline timestamp_type get_timestamp()
{
    return std::chrono::steady_clock::now();
}

inline double timestamp_diff_in_seconds(timestamp_type start, timestamp_type end)
{
    std::chrono::duration<double> p = end - start;
    return p.count();
}

inline double timestamp_diff_in_seconds(timestamp_type start)
{
    std::chrono::duration<double> p = get_timestamp() - start;
    return p.count();
}

///
/// Creates a verbose timer that prints after tock().
///
class VerboseTimer
{
public:
    ///
    /// Constructs a new instance.
    ///
    /// @param[in]  prefix  Prefix name to prepend to each log message. Defaults to empty string.
    /// @param[in]  log     Optional logger to use. Defaults to the lagrange logger.
    /// @param[in]  level   Log level to use. Defaults to debug.
    ///
    VerboseTimer(
        std::string prefix = "",
        spdlog::logger* log = nullptr,
        spdlog::level::level_enum level = spdlog::level::debug)
        : m_prefix(std::move(prefix))
        , m_logger(log ? log : &logger())
        , m_level(level)
    {}

    ///
    /// Starts the timer.
    ///
    void tick() { m_start_time = get_timestamp(); }

    ///
    /// Stops the timer.
    ///
    /// @param[in]  name  Name of the message to show.
    ///
    /// @return     The elapsed time in seconds.
    ///
    double tock(const std::string name = "")
    {
        auto end_time = get_timestamp();
        auto duration = timestamp_diff_in_seconds(m_start_time, end_time);
        m_logger->log(m_level, "{}{} time: {} (s)", m_prefix, name, duration);
        return duration;
    }

private:
    std::string m_prefix;
    spdlog::logger* m_logger = nullptr;
    spdlog::level::level_enum m_level = spdlog::level::debug;
    timestamp_type m_start_time;
};

///
/// Similar to a VerboseTimer, but uses RAII to call tick()/tock().
///
class ScopedTimer
{
public:
    ///
    /// Constructs a new instance.
    ///
    /// @param[in]  prefix  Prefix name to prepend to each log message. Defaults to empty string.
    /// @param[in]  log     Optional logger to use. Defaults to the lagrange logger.
    /// @param[in]  level   Log level to use. Defaults to debug.
    ///
    ScopedTimer(
        std::string prefix,
        spdlog::logger* log = nullptr,
        spdlog::level::level_enum level = spdlog::level::debug)
        : m_timer(std::make_unique<VerboseTimer>(prefix, log, level))
    {
        m_timer->tick();
    }

    ~ScopedTimer()
    {
        if (m_timer) {
            m_timer->tock();
        }
    }

    ScopedTimer(ScopedTimer&&) = default;
    ScopedTimer& operator=(ScopedTimer&&) = default;

private:
    // Because RVO is not guaranteed until C++17, we need to store the content of the class in a
    // moveable unique_ptr. Otherwise we can't write helper functions that return a ScopedTimer.
    // Once we move to C++17, we should update this class and delete its move/copy operators.
    std::unique_ptr<VerboseTimer> m_timer;
};

///
/// A timer that does not print after tock()
///
class SilentTimer
{
public:
    ///
    /// Starts the timer.
    ///
    void tick() { m_start_time = get_timestamp(); }

    ///
    /// Stops the timer.
    ///
    /// @return     The elapsed time in seconds.
    ///
    double tock(const std::string name = "")
    {
        (void)name;
        auto end_time = get_timestamp();
        return timestamp_diff_in_seconds(m_start_time, end_time);
    }

private:
    timestamp_type m_start_time;
};

///
/// A timer that keeps track of a total time as well as intervals.
///
class SilentMultiTimer
{
public:
    ///
    /// Starts/restarts the timer.
    ///
    void reset() { m_start = m_last = get_timestamp(); }

    ///
    /// Returns current interval time (in seconds) and resets interval to now.
    ///
    /// @return     Current interval time.
    ///
    double interval()
    {
        auto last = m_last;
        m_last = get_timestamp();
        return timestamp_diff_in_seconds(last, m_last);
    }

    ///
    /// Returns total time. Does not reset.
    ///
    /// @return     Total time since the timer was started.
    ///
    double total() { return timestamp_diff_in_seconds(m_start); }

private:
    timestamp_type m_start;
    timestamp_type m_last;
};


} // namespace lagrange
