/*
 * Copyright 2026 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include "progress_bridge.h"

#include <utility>

namespace lagrange::xatlas::internal {

namespace {

const char* stage_name(::xatlas::ProgressCategory category) noexcept
{
    switch (category) {
    case ::xatlas::ProgressCategory::AddMesh: return "xatlas: Add mesh";
    case ::xatlas::ProgressCategory::ComputeCharts: return "xatlas: Compute charts";
    case ::xatlas::ProgressCategory::PackCharts: return "xatlas: Pack charts";
    case ::xatlas::ProgressCategory::BuildOutputMeshes: return "xatlas: Build output meshes";
    }
    return "xatlas";
}

int category_index(::xatlas::ProgressCategory category) noexcept
{
    switch (category) {
    case ::xatlas::ProgressCategory::AddMesh: return 0;
    case ::xatlas::ProgressCategory::ComputeCharts: return 1;
    case ::xatlas::ProgressCategory::PackCharts: return 2;
    case ::xatlas::ProgressCategory::BuildOutputMeshes: return 3;
    }
    return -1;
}

} // namespace

ProgressBridge::ProgressBridge(
    std::function<void(const std::string&, float)> notification_func,
    const std::atomic_bool* cancel) noexcept
    : m_notification_func(std::move(notification_func))
    , m_cancel(cancel)
{
    for (auto& last : m_last_progress) {
        last = -1;
    }
}

void ProgressBridge::install(::xatlas::Atlas* atlas) noexcept
{
    if (atlas == nullptr) return;
    if (!m_notification_func && m_cancel == nullptr) return;
    ::xatlas::SetProgressCallback(atlas, &ProgressBridge::progress_thunk, this);
}

void ProgressBridge::rethrow_if_pending()
{
    if (m_pending_exception) {
        auto e = m_pending_exception;
        m_pending_exception = nullptr;
        std::rethrow_exception(e);
    }
}

bool ProgressBridge::progress_thunk(
    ::xatlas::ProgressCategory category,
    int progress,
    void* user_data)
{
    auto* self = static_cast<ProgressBridge*>(user_data);
    if (self == nullptr) return true;

    if (self->m_cancel != nullptr && self->m_cancel->load(std::memory_order_relaxed)) {
        self->m_was_cancelled.store(true, std::memory_order_relaxed);
        return false;
    }

    if (self->m_notification_func) {
        std::unique_lock<std::mutex> lock(self->m_callback_mutex, std::try_to_lock);
        if (!lock.owns_lock()) {
            return true;
        }

        try {
            const int index = category_index(category);
            bool should_notify = true;
            if (index >= 0) {
                auto& last = self->m_last_progress[static_cast<size_t>(index)];
                should_notify = progress > last;
                if (should_notify) last = progress;
            }
            if (should_notify) {
                self->m_notification_func(
                    stage_name(category),
                    static_cast<float>(progress) / 100.f);
            }
        } catch (...) {
            if (!self->m_pending_exception) {
                self->m_pending_exception = std::current_exception();
            }
            self->m_was_cancelled.store(true, std::memory_order_relaxed);
            return false;
        }
    }

    return true;
}

} // namespace lagrange::xatlas::internal
