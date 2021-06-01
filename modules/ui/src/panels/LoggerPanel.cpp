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
#include <lagrange/common.h>
#include <lagrange/ui/utils/uipanel.h>
#include <lagrange/ui/panels/LoggerPanel.h>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <spdlog/details/null_mutex.h>
#include <spdlog/sinks/base_sink.h>
#include <deque>


namespace lagrange {
namespace ui {

struct LoggerPanel
{
    size_t last_frame_size = 0;
};

#define LOGUI_LIMIT 16 * 1024

class LogData
{
public:
    using ColorType = std::remove_const_t<decltype(ImGui::Spectrum::GRAY800)>;
    std::deque<std::pair<ColorType, std::string>> data;
    std::mutex mutex;
};

////////////////////////////////////////////////////////////////////////////////

namespace {

// spdlog to LogUI
template <typename Mutex>
class SpdlogUISink : public spdlog::sinks::base_sink<Mutex>
{
public:
    SpdlogUISink(LogData& data)
        : m_data(data)
    {}

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override
    {
        LogData::ColorType color = ImGui::Spectrum::GRAY800;
        switch (msg.level) {
        case spdlog::level::trace: color = ImGui::Spectrum::GRAY500; break;
        case spdlog::level::debug: color = ImGui::Spectrum::BLUE400; break;
        case spdlog::level::info: color = ImGui::Spectrum::GREEN500; break;
        case spdlog::level::warn: color = ImGui::Spectrum::YELLOW500; break;
        case spdlog::level::err: color = ImGui::Spectrum::RED500; break;
        case spdlog::level::critical: color = ImGui::Spectrum::PURPLE500; break;
        case spdlog::level::off: break;
        default: break;
        }

        // If needed (very likely but not mandatory), the sink formats the message before sending it
        // to its final destination:
        spdlog::memory_buf_t formatted;
        spdlog::sinks::base_sink<Mutex>::formatter_->format(msg, formatted);

        std::lock_guard<std::mutex> lock(m_data.mutex);
        if (m_data.data.size() > LOGUI_LIMIT) {
            m_data.data.pop_front();
        }
        m_data.data.emplace_back(color, fmt::to_string(formatted));
    }

    void flush_() override
    {
        // No-op
    }

private:
    Mutex m_mutex;
    LogData& m_data;
};

using SpdlogUISinkMt = SpdlogUISink<std::mutex>;
using SpdlogUISinkSt = SpdlogUISink<spdlog::details::null_mutex>;


} // namespace

struct LoggerContextData
{
    LoggerContextData()
    {
        try {
            sink = std::make_shared<SpdlogUISinkMt>(log_data);
            logger().sinks().emplace_back(sink);
        } catch (std::exception&) {
            // Sink with same name exists
        }
    }

    ~LoggerContextData()
    {
        auto it = std::find(logger().sinks().begin(), logger().sinks().end(), sink);
        if (it != logger().sinks().end()) {
            logger().sinks().erase(it);
        }
    }

    LogData log_data;
    std::shared_ptr<SpdlogUISinkMt> sink;
};


void draw_logger_panel(Registry& r, Entity entity)
{
    LoggerPanel& data = r.get<LoggerPanel>(entity);

    // Create registry global sink
    auto& ctx = r.ctx_or_set<LoggerContextData>();

    ImGui::BeginChild("##log_scroll");

    std::lock_guard<std::mutex> lock(ctx.log_data.mutex);
    for (auto it : ctx.log_data.data) {
        ImGui::TextColored(ImColor(it.first).Value, "%s", it.second.c_str());
    }

    if (data.last_frame_size == ctx.log_data.data.size()) {
    } else {
        ImGui::SetScrollHereY();
        data.last_frame_size = ctx.log_data.data.size();
    }

    ImGui::EndChild();
}

Entity add_logger_panel(Registry& r, const std::string& name)
{
    auto e = add_panel(r, name, draw_logger_panel);
    r.emplace<LoggerPanel>(e);
    return e;
}


} // namespace ui
} // namespace lagrange
