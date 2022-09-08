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
#include <lagrange/ui/types/Tools.h>

namespace lagrange {
namespace ui {


void Tools::run(entt::id_type tool_type, entt::id_type element_type, Registry& registry)
{
    m_tool_systems[KeyType{tool_type, element_type}](registry);
}

bool Tools::run_current(Registry& registry)
{
    auto it = m_tool_systems.find(m_current_key);
    if (m_tool_systems.find(m_current_key) == m_tool_systems.end()) return false;
    it->second(registry);

    return true;
}

const std::vector<entt::id_type>& Tools::get_element_types() const
{
    return m_element_types;
}

const std::vector<entt::id_type>& Tools::get_tool_types() const
{
    return m_tool_types;
}

entt::id_type Tools::get_current_tool_type() const
{
    return m_current_key.first;
}

entt::id_type Tools::get_current_element_type() const
{
    return m_current_key.second;
}

void Tools::set_current_element_type(entt::id_type element_type)
{
    m_current_key = KeyType{m_current_key.first, element_type};
}

void Tools::set_current_tool_type(entt::id_type tool_type)
{
    m_current_key = KeyType{tool_type, m_current_key.second};
}

} // namespace ui
} // namespace lagrange