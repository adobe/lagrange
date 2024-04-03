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
#pragma once

#include <lagrange/ui/api.h>
#include <lagrange/ui/types/Tools.h>


namespace lagrange {
namespace ui {


LA_UI_API Tools& get_tools(ui::Registry& r);
LA_UI_API const Tools& get_tools(const ui::Registry& r);


template <typename T>
bool is_element_type(entt::id_type elem_type)
{
    return elem_type == entt::resolve<T>().id();
}

/// Initialize Tools context variable
LA_UI_API Tools& initialize_tools(ui::Registry& r);
/// Run currently selected tool
LA_UI_API void run_current_tool(ui::Registry& r);
/// Update previously used tool - must be called for is_tool_(de)activate to work
LA_UI_API void update_previous_tool(ui::Registry& r);

/// Was tool type activated this frame
LA_UI_API bool is_tool_activated(const ui::Registry& r, entt::id_type tool_type);
/// Was tool type and element type activated this frame
LA_UI_API bool is_tool_activated(const ui::Registry& r, entt::id_type tool_type, entt::id_type element_type);


/// Was tool type deactivated this frame
LA_UI_API bool is_tool_deactivated(const ui::Registry& r, entt::id_type tool_type);
/// Was tool type and element type deactivated this frame
LA_UI_API bool is_tool_deactivated(
    const ui::Registry& r,
    entt::id_type tool_type,
    entt::id_type element_type);


template <typename ToolTag>
bool is_tool_active(const ui::Registry& r)
{
    return get_tools(r).get_current_tool_type() == entt::resolve<ToolTag>().id();
}

template <typename ToolTag, typename ElementTag>
bool is_tool_active(const ui::Registry& r)
{
    return is_tool_active<ToolTag>(r) &&
           get_tools(r).get_current_element_type() == entt::resolve<ElementTag>().id();
}

template <typename ToolTag>
bool is_tool_activated(const ui::Registry& r)
{
    return is_tool_activated(r, entt::resolve<ToolTag>().id());
}

template <typename ToolTag, typename ElementTag>
bool is_tool_activated(const ui::Registry& r)
{
    const auto idt = entt::resolve<ToolTag>().id();
    const auto ide = entt::resolve<ElementTag>().id();
    return is_tool_activated(r, idt, ide);
}

template <typename ToolTag>
bool is_tool_deactivated(const ui::Registry& r)
{
    return is_tool_deactivated(r, entt::resolve<ToolTag>().id());
}

template <typename ToolTag, typename ElementTag>
bool is_tool_deactivated(const ui::Registry& r)
{
    const auto idt = entt::resolve<ToolTag>().id();
    const auto ide = entt::resolve<ElementTag>().id();
    return is_tool_deactivated(r, idt, ide);
}

} // namespace ui
} // namespace lagrange
