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

#include <lagrange/ui/utils/tools.h>

namespace lagrange {
namespace ui {


struct LastTool
{
    entt::id_type prev_tool = 0;
    entt::id_type prev_element = 0;
};

Tools& initialize_tools(ui::Registry& r)
{
    return r.ctx().insert_or_assign<Tools>(Tools{});
}

Tools& get_tools(ui::Registry& r)
{
    return r.ctx().get<Tools>();
}

const Tools& get_tools(const ui::Registry& r)
{
    return r.ctx().get<Tools>();
}

void run_current_tool(ui::Registry& r)
{
    auto& t = get_tools(r);
    t.run_current(r);
}

void update_previous_tool(ui::Registry& r)
{
    auto& lt = r.ctx().emplace<LastTool>(LastTool{});
    auto& t = get_tools(r);
    lt.prev_tool = t.get_current_tool_type();
    lt.prev_element = t.get_current_element_type();
}

bool is_tool_activated(const ui::Registry& r, entt::id_type tool_type)
{
    const auto& lt = r.ctx().get<LastTool>();
    const auto& tools = get_tools(r);
    return tools.get_current_tool_type() == tool_type && lt.prev_tool != tool_type;
}

bool is_tool_activated(const ui::Registry& r, entt::id_type tool_type, entt::id_type element_type)
{
    const auto& lt = r.ctx().get<LastTool>();
    const auto& tools = get_tools(r);
    return (tools.get_current_tool_type() == tool_type && lt.prev_tool != tool_type) &&
           (tools.get_current_element_type() == element_type && lt.prev_element != element_type);
}

bool is_tool_deactivated(const ui::Registry& r, entt::id_type tool_type)
{
    const auto& lt = r.ctx().get<LastTool>();
    const auto& tools = get_tools(r);
    return tools.get_current_tool_type() != tool_type && lt.prev_tool == tool_type;
}

bool is_tool_deactivated(const ui::Registry& r, entt::id_type tool_type, entt::id_type element_type)
{
    const auto& lt = r.ctx().get<LastTool>();
    const auto& tools = get_tools(r);
    return (tools.get_current_tool_type() != tool_type && lt.prev_tool == tool_type) &&
           (tools.get_current_element_type() != element_type && lt.prev_element == element_type);
}

} // namespace ui
} // namespace lagrange
