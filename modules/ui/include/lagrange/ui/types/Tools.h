/*
 * Copyright 2021 Adobe. All rights reserved.
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

#include <lagrange/ui/Entity.h>
#include <lagrange/ui/api.h>
#include <lagrange/ui/utils/pair_hash.h>

namespace lagrange {
namespace ui {

template <typename T>
entt::id_type register_tool_type(
    const std::string& display_name,
    const std::string& icon,
    const std::string& keybind)
{
    using namespace entt::literals;

    entt::meta_factory<T>().template custom<TypeData>(
        TypeData().set_display_name(display_name).set_icon(icon).set_keybind(keybind));

    return entt::type_id<T>().hash();
}


/*
    Container for Tool systems
*/
class LA_UI_API Tools
{
public:
    template <typename ToolType, typename ElementType>
    void register_tool(const System& tool_system)
    {
        const auto k = key<ToolType, ElementType>();
        m_tool_systems[k] = tool_system;

        if (std::find(m_tool_types.begin(), m_tool_types.end(), k.first) == m_tool_types.end()) {
            m_tool_types.push_back(k.first);
        }

        if (std::find(m_element_types.begin(), m_element_types.end(), k.second) ==
            m_element_types.end()) {
            m_element_types.push_back(k.second);
        }
    }

    template <typename ToolType, typename ElementType>
    void run(Registry& registry)
    {
        m_tool_systems[key<ToolType, ElementType>()](registry);
    }

    void run(entt::id_type tool_type, entt::id_type element_type, Registry& registry);


    bool run_current(Registry& registry);


    const std::vector<entt::id_type>& get_element_types() const;
    const std::vector<entt::id_type>& get_tool_types() const;

    std::vector<entt::id_type>& get_element_types();
    std::vector<entt::id_type>& get_tool_types();

    entt::id_type get_current_tool_type() const;
    entt::id_type get_current_element_type() const;

    bool has_backface_selection_tool() const;
    void enable_backface_selection_tool(bool has_backfaces_tool);

    void set_current_element_type(entt::id_type element_type);

    void set_current_tool_type(entt::id_type tool_type);

    template <typename T>
    void set_current_element_type()
    {
        set_current_element_type(entt::type_id<T>().hash());
    }

    template <typename T>
    void set_current_tool_type()
    {
        set_current_tool_type(entt::type_id<T>().hash());
    }

private:
    using KeyType = std::pair<entt::id_type, entt::id_type>;

    template <typename ToolType, typename ElementType>
    static constexpr KeyType key()
    {
        return {entt::type_id<ToolType>().hash(), entt::type_id<ElementType>().hash()};
    }
    std::unordered_map<KeyType, System, utils::pair_hash> m_tool_systems;
    std::vector<entt::id_type> m_tool_types;
    std::vector<entt::id_type> m_element_types;

    KeyType m_current_key = {0, 0};

    bool m_has_backface_selection_tool = true;
};


} // namespace ui
} // namespace lagrange
