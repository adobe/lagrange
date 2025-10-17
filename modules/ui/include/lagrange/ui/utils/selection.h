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
#include <lagrange/ui/components/Selection.h>
#include <lagrange/ui/components/SelectionContext.h>

namespace lagrange {
namespace ui {

class Keybinds;


LA_UI_API bool deselect_all(Registry& registry);
LA_UI_API bool dehover_all(Registry& registry);


inline bool is_selected(const Registry& registry, Entity e)
{
    return registry.all_of<Selected>(e);
}

inline bool is_hovered(const Registry& registry, Entity e)
{
    return registry.all_of<Hovered>(e);
}

inline decltype(auto) selected_view(Registry& registry)
{
    return registry.view<Selected>();
}

inline decltype(auto) hovered_view(Registry& registry)
{
    return registry.view<Hovered>();
}

LA_UI_API bool is_child_selected(const Registry& registry, Entity e, bool recursive = true);
LA_UI_API bool is_child_hovered(const Registry& registry, Entity e, bool recursive = true);


LA_UI_API std::vector<Entity> collect_selected(const Registry& registry);
LA_UI_API std::vector<Entity> collect_hovered(const Registry& registry);


LA_UI_API bool
set_selected(Registry& registry, Entity e, SelectionBehavior behavior = SelectionBehavior::SET);

LA_UI_API bool
set_hovered(Registry& registry, Entity e, SelectionBehavior behavior = SelectionBehavior::SET);

LA_UI_API bool select(Registry& registry, Entity e);

LA_UI_API bool deselect(Registry& registry, Entity e);

LA_UI_API bool hover(Registry& registry, Entity e);

LA_UI_API bool dehover(Registry& registry, Entity e);


LA_UI_API SelectionContext& get_selection_context(Registry& r);

LA_UI_API const SelectionContext& get_selection_context(const Registry& r);

/* Utils*/

LA_UI_API SelectionBehavior selection_behavior(const Keybinds& keybinds);
LA_UI_API bool are_selection_keys_down(const Keybinds& keybinds);
LA_UI_API bool are_selection_keys_pressed(const Keybinds& keybinds);
LA_UI_API bool are_selection_keys_released(const Keybinds& keybinds);


LA_UI_API SelectionBehavior selection_behavior(const Registry& r);
LA_UI_API bool are_selection_keys_down(const Registry& r);
LA_UI_API bool are_selection_keys_pressed(const Registry& r);
LA_UI_API bool are_selection_keys_released(const Registry& r);


} // namespace ui
} // namespace lagrange
