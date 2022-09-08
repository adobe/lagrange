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
#include <lagrange/ui/components/Selection.h>
#include <lagrange/ui/types/Keybinds.h>
#include <lagrange/ui/utils/input.h>
#include <lagrange/ui/utils/selection.h>
#include <lagrange/ui/utils/treenode.h>

namespace lagrange {
namespace ui {

template <typename Component>
bool deselect_all_impl(Registry& r)
{
    auto v = r.view<Component>();
    if (v.empty()) {
        return false;
    }
    r.clear<Component>();
    return true;
}

template <typename Component>
bool set_selection_component_impl(Registry& r, Entity e, SelectionBehavior behavior)
{
    // Null entity
    if (!r.valid(e)) {
        // Reset selection if SET
        if (behavior == SelectionBehavior::SET) return deselect_all_impl<Component>(r);

        // Otherwise do nothing
        return false;
    }

    bool result = false;

    if (behavior == SelectionBehavior::SET) {
        result |= deselect_all_impl<Component>(r);
    }

    if (r.all_of<Component>(e)) {
        if (behavior == SelectionBehavior::ERASE) {
            r.remove<Component>(e);
            result = true;
        }
        return result;
    }

    if (behavior == SelectionBehavior::ERASE) {
        return result;
    }

    r.emplace<Component>(e);
    result = true;
    return result;
}


bool deselect_all(Registry& r)
{
    return deselect_all_impl<Selected>(r);
}

bool dehover_all(Registry& r)
{
    return deselect_all_impl<Hovered>(r);
}


bool is_child_selected(const Registry& registry, Entity e, bool recursive /*= true*/)
{
    bool any_selected = false;
    auto fn = [&](Entity e) { any_selected |= ui::is_selected(registry, e); };

    if (recursive) {
        foreach_child(registry, e, fn);
    } else {
        foreach_child_recursive(registry, e, fn);
    }

    return any_selected;
}

bool is_child_hovered(const Registry& registry, Entity e, bool recursive /*= true*/)
{
    bool any_selected = false;
    auto fn = [&](Entity e) { any_selected |= ui::is_hovered(registry, e); };

    if (recursive) {
        foreach_child(registry, e, fn);
    } else {
        foreach_child_recursive(registry, e, fn);
    }

    return any_selected;
}

std::vector<Entity> collect_selected(const Registry& r)
{
    std::vector<Entity> result;
    r.view<const Selected>().each([&](Entity e, const Selected& /*s*/) { result.push_back(e); });
    return result;
}

std::vector<Entity> collect_hovered(const Registry& r)
{
    std::vector<Entity> result;
    r.view<const Hovered>().each([&](Entity e, const Hovered& /*s*/) { result.push_back(e); });
    return result;
}


bool set_selected(Registry& r, Entity e, SelectionBehavior behavior /*= SelectionBehavior::SET*/)
{
    return set_selection_component_impl<Selected>(r, e, behavior);
}

bool set_hovered(Registry& r, Entity e, SelectionBehavior behavior /*= SelectionBehavior::SET*/)
{
    return set_selection_component_impl<Hovered>(r, e, behavior);
}

SelectionContext& get_selection_context(Registry& r)
{
    return r.ctx_or_set<SelectionContext>(SelectionContext());
}

const SelectionContext& get_selection_context(const Registry& r)
{
    return r.ctx<SelectionContext>();
}

SelectionBehavior selection_behavior(const Keybinds& keybinds)
{
    SelectionBehavior sel_behavior = SelectionBehavior::SET;
    if (keybinds.is_down("viewport.selection_select_erase") ||
        keybinds.is_released("viewport.selection_select_erase")) {
        sel_behavior = SelectionBehavior::ERASE;
    } else if (
        keybinds.is_down("viewport.selection_select_add") ||
        keybinds.is_released("viewport.selection_select_add")) {
        sel_behavior = SelectionBehavior::ADD;
    }
    return sel_behavior;
}

bool are_selection_keys_down(const Keybinds& keybinds)
{
    return keybinds.is_down("viewport.selection_select_set") ||
           keybinds.is_down("viewport.selection_select_add") ||
           keybinds.is_down("viewport.selection_select_erase");
}

bool are_selection_keys_pressed(const Keybinds& keybinds)
{
    return keybinds.is_pressed("viewport.selection_select_set") ||
           keybinds.is_pressed("viewport.selection_select_add") ||
           keybinds.is_pressed("viewport.selection_select_erase");
}

bool are_selection_keys_released(const Keybinds& keybinds)
{
    return keybinds.is_released("viewport.selection_select_set");
}

SelectionBehavior selection_behavior(const Registry& r)
{
    return selection_behavior(get_keybinds(r));
}
bool are_selection_keys_down(const Registry& r)
{
    return are_selection_keys_down(get_keybinds(r));
}
bool are_selection_keys_pressed(const Registry& r)
{
    return are_selection_keys_pressed(get_keybinds(r));
}
bool are_selection_keys_released(const Registry& r)
{
    return are_selection_keys_released(get_keybinds(r));
}

bool select(Registry& r, Entity e)
{
    return set_selected(r, e, SelectionBehavior::SET);
}

bool deselect(Registry& r, Entity e)
{
    return set_selected(r, e, SelectionBehavior::ERASE);
}

bool hover(Registry& r, Entity e)
{
    return set_hovered(r, e, SelectionBehavior::SET);
}

bool dehover(Registry& r, Entity e)
{
    return set_hovered(r, e, SelectionBehavior::ERASE);
}

} // namespace ui
} // namespace lagrange
