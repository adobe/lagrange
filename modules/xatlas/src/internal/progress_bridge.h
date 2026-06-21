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
#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <exception>
#include <functional>
#include <mutex>
#include <string>

#include <xatlas/xatlas.h>

namespace lagrange::xatlas::internal {

///
/// Bridge between xatlas's C-style progress callback and the Lagrange notification + cancel API.
///
/// xatlas may call its progress callback from worker threads. The bridge forwards monotonic
/// per-category progress to the user callback and never lets exceptions propagate across the C ABI.
/// If another thread is already notifying the callback, the new signal is dropped. Any exception
/// thrown by the user callback is captured and rethrown by `rethrow_if_pending()` after the xatlas
/// call returns.
///
class ProgressBridge
{
public:
    ProgressBridge(
        std::function<void(const std::string&, float)> notification_func,
        const std::atomic_bool* cancel) noexcept;

    /// Install on an atlas (no-op if both members are null).
    void install(::xatlas::Atlas* atlas) noexcept;

    /// True if the user requested cancellation, or the bridge cancelled due to a user exception.
    bool was_cancelled() const noexcept { return m_was_cancelled.load(); }

    /// Rethrow any exception captured from the user notification callback.
    void rethrow_if_pending();

private:
    static bool progress_thunk(::xatlas::ProgressCategory category, int progress, void* user_data);

    std::function<void(const std::string&, float)> m_notification_func;
    const std::atomic_bool* m_cancel;
    std::atomic_bool m_was_cancelled{false};

    static constexpr size_t kNumCategories = 4;
    std::array<int, kNumCategories> m_last_progress;

    std::mutex m_callback_mutex;
    std::exception_ptr m_pending_exception;
};

} // namespace lagrange::xatlas::internal
