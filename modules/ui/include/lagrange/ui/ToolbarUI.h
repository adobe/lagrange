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

#include <string>
#include <unordered_map>

namespace lagrange {
namespace ui {

class ToolbarUI : public UIPanelBase {
public:

    static constexpr float toolbar_width = 45.0f;

    struct Action {
        std::function<void(Action&)> on_click;
        std::function<void(Action&)> popup = nullptr;
        std::string tooltip = "";
        std::string label = "Unnamed";
        bool enabled = true;
        std::function<bool()> selected;

        std::string keybind_action = "";
    };

    ToolbarUI(Viewer* viewer)
        : UIPanelBase(viewer)
    {}

    const char* get_title() const override { return "##Toolbar"; };

    void add_action(const std::string& group_name, const std::shared_ptr<Action>& action);

    void draw() final;    

private:
    std::unordered_map<std::string, std::vector<std::shared_ptr<Action>>> m_group_actions;
};

} // namespace ui
} // namespace lagrange
