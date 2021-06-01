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

#include <lagrange/ui/Entity.h>
#include <lagrange/ui/components/TreeNode.h>

#include <functional>

namespace lagrange {
namespace ui {


Entity create_scene_node(
    Registry& r,
    const std::string& name = "Unnamed Scene Node Entity",
    Entity parent = NullEntity);


/// @brief Removes entity
/// @param r
/// @param e
/// @param recursive removes all children recursively
void remove(Registry& r, Entity e, bool recursive = false);

/// @brief Sets new_parent as as child's new parent. Both must have <Tree> component.
/// @param registry
/// @param Entity child
/// @param Entity new_parent
void set_parent(Registry& registry, Entity child, Entity new_parent);

/// @brief  Returns parent of e. Must have <Tree> component.
///         Returns NullEntity if e is top-level.
/// @param registry
/// @param e
/// @return
Entity get_parent(const Registry& registry, Entity e);

/// @brief  Returns all children of e. Must have <Tree> component.
///         Note: causes dynamic allocation. Use lagrange::ui::foreach_child to iterate children efficiently.
/// @param registry
/// @param e
/// @return std::vector<Entity>
std::vector<Entity> get_children(const Registry& registry, Entity e);

/// @brief Returns true if child has no parents (is at top-level in the hierarachy)
/// @param registry
/// @param child
bool is_orphan(const Registry& registry, Entity child);

/// @brief Reparents child to be top-level (i.e., to have to parents).
/// @param registry
/// @param child
void orphan(Registry& registry, Entity child);

/// @brief Calls fn(Entity) on each direct child of parent entity
/// @param registry
/// @param parent parent entity with Tree component
/// @param fn function to call, takes Entity parameter
void foreach_child(
    const Registry& registry,
    const Entity parent,
    const std::function<void(Entity)>& fn);

/// @brief Calls fn(Entity) on each child of parent entity recursively
/// @param registry
/// @param parent parent entity with Tree component
/// @param fn function to call, takes Entity parameter
void foreach_child_recursive(
    const Registry& registry,
    const Entity parent,
    const std::function<void(Entity)>& fn);

/// @brief Alternative to foreach_child_recursive
void iterate_inorder(
    Registry& registry,
    const std::function<bool(Entity)>& on_enter,
    const std::function<void(Entity, bool)>& on_exit);

Entity group(
    Registry& registry,
    const std::vector<Entity>& entities,
    const std::string& name = "NewGroup");

Entity group_under(Registry& registry, const std::vector<Entity>& entities, Entity parent);


/// Returns new parent
Entity ungroup(Registry& registry, Entity parent, bool remove_parent = false);

/// Removes the node from tree and puts it as top-level node
void orphan_without_subtree(Registry& registry, Entity e);


} // namespace ui
} // namespace lagrange
