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

#include <tbb/spin_mutex.h>
#include <atomic>
#include <functional>
#include <string>

namespace lagrange {

///
/// A simple thread-safe progress callback.
///
class ProgressCallback
{
public:
    ///
    /// Constructs a new instance.
    ///
    /// @param[in]  func            Notification callback. This callback function will be called in
    ///                             a thread-safe manner by the update() method.
    /// @param[in]  name            Default section name.
    /// @param[in]  num_iterations  Number of iterations for this section.
    ///
    ProgressCallback(
        std::function<void(const std::string &, float)> func = nullptr,
        std::string name = "",
        size_t num_iterations = 1);

    ///
    /// Set notification callback.
    ///
    /// @param[in]  func  Notification callback. This callback function will be called in a
    ///                   thread-safe manner by the update() method.
    ///
    void set_callback(std::function<void(const std::string &, float)> func);

    ///
    /// Starts a new section, reset current iteration counter to 0, and notify. This method is not
    /// thread-safe.
    ///
    /// @param[in]  name            Section name.
    /// @param[in]  num_iterations  Number of iterations for this section.
    ///
    void set_section(std::string name, size_t num_iterations = 1);

    ///
    /// Retrieves current section name.
    ///
    /// @return     Name of the current section.
    ///
    const std::string & get_section() const { return m_section_name; }

    ///
    /// Sets the number iterations for this section. No notification is sent. This method is not
    /// thread-safe.
    ///
    /// @param[in]  num_iterations  Number of iterations for this section.
    ///
    void set_num_iterations(size_t num_iterations);

    ///
    /// Updates the current iteration number, and sends a notification. It is safe to call this
    /// method from multiple threads.
    ///
    void update();

    ///
    /// Updates the current progress to a fixed percentage.
    ///
    /// @param[in]  progress  Current progress between 0 and 1.
    ///
    void update(float progress);

    ///
    /// Sets the verbosity. A verbose progress callback will print the section name as debug info
    /// whenever `set_section` is called with a non-empty section name.
    ///
    /// @param[in]  verbose  Verbosity level.
    ///
    void set_verbose(bool verbose) { m_verbose = verbose; }

private:
    /// Callback function to be called by the update method.
    std::function<void(const std::string &, float)> m_callback;

    /// Name of the current section.
    std::string m_section_name;

    /// Total number of iterations for the current section.
    size_t m_num_iterations = 1;

    /// Current iteration number.
    std::atomic_size_t m_current_iteration = {0};

    /// Mutex to be tentatively locked before calling the callback function.
    tbb::spin_mutex m_mutex;

    /// Verbosity level
    bool m_verbose = false;
};

} // namespace lagrange
