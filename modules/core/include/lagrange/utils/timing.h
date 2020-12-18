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

// ===================================
//   Timers
//   Creation examples
//     auto timer = create_verbose_timer(logger())
//     auto timer = create_silent_timer()
//   Start
//     timer.tick()
//   Stop
//     auto duration = timer.tock()
//     For verbose timer also prints the take time
// ===================================

// Timer that prints after tock()
class VerboseTimer
{
public:
    VerboseTimer() = delete;

    VerboseTimer(std::string prefix)
        : m_prefix(std::move(prefix))
        , m_logger(&logger())
    {}

    VerboseTimer(spdlog::logger* log = nullptr)
        : m_logger(log ? log : &logger())
    {}

    void tick() { m_start_time = get_timestamp(); }

    double tock(const std::string name = "")
    {
        auto end_time = get_timestamp();
        auto duration = timestamp_diff_in_seconds(m_start_time, end_time);
        m_logger->debug("{}{} time: {} (s)", m_prefix, name, duration);
        return duration;
    }

private:
    std::string m_prefix;
    spdlog::logger* m_logger;
    timestamp_type m_start_time;
};

// Timer that does not print after tock()
class SilentTimer
{
public:
    void tick() { m_start_time = get_timestamp(); }

    double tock(const std::string name = "")
    {
        (void)name;
        auto end_time = get_timestamp();
        return timestamp_diff_in_seconds(m_start_time, end_time);
    }

private:
    timestamp_type m_start_time;
};

inline VerboseTimer create_verbose_timer(spdlog::logger& logger)
{
    return VerboseTimer(&logger);
}

inline SilentTimer create_silent_timer()
{
    return SilentTimer();
}

// Timer that keeps track of total as well as intervals
class SilentMultiTimer
{
public:
    // Sets / resets starting time.
    void reset() { m_start = m_last = get_timestamp(); }

    // Returns current interval time (in seconds) and resets interval to now
    double interval()
    {
        auto last = m_last;
        m_last = get_timestamp();
        return timestamp_diff_in_seconds(last, m_last);
    }

    // Returns total time. Does not reset.
    double total() { return timestamp_diff_in_seconds(m_start); }

private:
    timestamp_type m_start;
    timestamp_type m_last;
};


} // namespace lagrange
