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

#include <lagrange/ui/UIPanel.h>
#include <spdlog/spdlog.h>
#include <sstream>

namespace lagrange {
namespace ui {

class LogData;

class LogUI : public lagrange::ui::UIPanelBase {

public:
    LogUI(Viewer* viewer);
    virtual ~LogUI();

    const char* get_title() const override { return "Log"; }

    void draw() override;

private:
    std::unique_ptr<LogData> m_log_data;
    std::shared_ptr<spdlog::sinks::sink> m_sink;
    size_t m_last_frame_size;
};

}
}
