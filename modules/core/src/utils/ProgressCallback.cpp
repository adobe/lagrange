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
#include <lagrange/utils/ProgressCallback.h>
#include <lagrange/Logger.h>

#include <algorithm>
#include <cassert>

namespace lagrange {

ProgressCallback::ProgressCallback(
    std::function<void(const std::string &, float)> func,
    std::string name,
    size_t num_iterations)
{
    set_callback(func);
    set_section(name, num_iterations);
}

void ProgressCallback::set_section(std::string name, size_t num_iterations)
{
    m_section_name = std::move(name);
    m_num_iterations = std::max<size_t>(num_iterations, 1);
    m_current_iteration = 0;
    if (m_verbose && !m_section_name.empty()) {
        logger().debug("[progress] {}", m_section_name);
    }
    if (m_callback && !m_section_name.empty()) {
        m_callback(m_section_name, 0.f);
    }
}

void ProgressCallback::set_num_iterations(size_t num_iterations)
{
    m_num_iterations = std::max<size_t>(num_iterations, 1);
    m_current_iteration = 0;
}

void ProgressCallback::update()
{
    size_t iter = ++m_current_iteration;
    if (iter > m_num_iterations) {
        assert(false);
        m_current_iteration = m_num_iterations;
    }
    update(static_cast<float>(iter) / static_cast<float>(m_num_iterations));
}

void ProgressCallback::update(float progress)
{
    assert(progress >= 0 && progress <= 1);
    if (m_mutex.try_lock()) {
        if (m_callback) {
            m_callback(m_section_name, progress);
        }
        m_mutex.unlock();
    }
}

void ProgressCallback::set_callback(std::function<void(const std::string &, float)> func)
{
    m_callback = std::move(func);
}

} // namespace lagrange
