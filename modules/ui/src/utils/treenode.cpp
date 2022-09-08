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
#include <lagrange/ui/components/Common.h>
#include <lagrange/ui/components/Transform.h>
#include <lagrange/ui/components/TreeNode.h>
#include <lagrange/ui/utils/treenode.h>

#include <assert.h>
#include <lagrange/Logger.h>
#include <lagrange/utils/assert.h>

namespace lagrange {
namespace ui {


Entity create_scene_node(
    Registry& r,
    const std::string& name /*= "Unnamed Scene Node Entity"*/,
    Entity parent /*= NullEntity*/)
{
    auto e = r.create();

    TreeNode t;
    t.parent = entt::null;

    r.emplace<Transform>(e, Transform());
    r.emplace<TreeNode>(e, t);
    r.emplace<Name>(e, name);

    if (parent != NullEntity) {
        ui::set_parent(r, e, parent);
    }

    return e;
}


void remove(Registry& r, Entity e, bool recursive /*= false*/)
{
    assert(r.valid(e));

    if (recursive) {
        auto children = get_children(r, e);
        for (auto child : children) {
            remove(r, child, true);
        }
    }

    r.destroy(e);
}

void foreach_child(
    const Registry& registry,
    const Entity parent,
    const std::function<void(Entity)>& fn)
{
    if (!registry.all_of<TreeNode>(parent)) return;
    const auto& parent_tree = registry.get<TreeNode>(parent);
    if (parent_tree.num_children == 0) return;

    auto e = parent_tree.first_child;
    if (!registry.valid(e)) return;
    if (!registry.all_of<TreeNode>(e)) return;
    auto* t = &registry.get<TreeNode>(e);

    fn(e);

    while (registry.valid(t->next_sibling)) {
        e = t->next_sibling;
        t = &registry.get<TreeNode>(e);

        fn(e);
    }
}

void foreach_child_recursive(
    const Registry& registry,
    const Entity parent,
    const std::function<void(Entity)>& fn)
{
    std::function<void(Entity)> rec_fn;
    rec_fn = [&rec_fn, &fn, &registry](Entity e) {
        fn(e);
        foreach_child(registry, e, rec_fn);
    };
    foreach_child(registry, parent, rec_fn);
}

Entity get_last_child(const Registry& registry, const TreeNode& parent_tree)
{
    assert(parent_tree.num_children > 0);

    auto e = parent_tree.first_child;
    auto* t = &registry.get<TreeNode>(e);

    while (t->next_sibling != NullEntity) {
        e = t->next_sibling;
        t = &registry.get<TreeNode>(e);
    }

    assert(e != NullEntity);
    return e;
}

bool is_orphan(const Registry& registry, Entity child)
{
    return registry.get<TreeNode>(child).parent == NullEntity;
}

void orphan(Registry& registry, Entity child)
{
    auto& t_child = registry.get<TreeNode>(child);

    // Already an orphan
    if (t_child.parent == NullEntity) return;

    auto& t_parent = registry.get<TreeNode>(t_child.parent);

    auto* t_prev = (t_child.prev_sibling == NullEntity)
                       ? nullptr
                       : &registry.get<TreeNode>(t_child.prev_sibling);

    auto* t_next = (t_child.next_sibling == NullEntity)
                       ? nullptr
                       : &registry.get<TreeNode>(t_child.next_sibling);


    // If not first child, adjust prev
    if (t_prev) {
        t_prev->next_sibling = t_child.next_sibling;
    }
    // If first child, adjust parent
    else if (t_parent.first_child == child) {
        t_parent.first_child = t_child.next_sibling;
    }

    // If not last child, adjust next
    if (t_next) {
        t_next->prev_sibling = t_child.prev_sibling;
    }

    t_child.parent = NullEntity;
    t_parent.num_children--;
}

void set_parent(Registry& registry, Entity child, Entity new_parent)
{
    la_runtime_assert(registry.all_of<TreeNode>(child), "Child must have Tree component");
    auto& t_child = registry.get<TreeNode>(child);
    auto* t_new_parent = (new_parent == NullEntity) ? nullptr : &registry.get<TreeNode>(new_parent);

    // Already parented correctly
    if (t_child.parent == new_parent) return;

    // Make sure its orphaned
    if (!is_orphan(registry, child)) orphan(registry, child);

    // Top level special case
    if (!t_new_parent) {
        // already orphaned
    }
    // New parent already has children
    else if (t_new_parent->num_children > 0) {
        auto last_child = get_last_child(registry, *t_new_parent);
        auto& t_last_child = registry.get<TreeNode>(last_child);

        t_last_child.next_sibling = child;
        t_new_parent->num_children++;

        t_child.parent = new_parent;
        t_child.prev_sibling = last_child;
    } else {
        t_new_parent->first_child = child;
        t_new_parent->num_children++;
        t_child.parent = new_parent;
    }
}


void iterate_inorder_recursive(
    Registry& registry,
    Entity e,
    const std::function<bool(Entity)>& on_enter,
    const std::function<void(Entity, bool)>& on_exit)
{
    if (on_enter(e)) {
        auto child = registry.get<TreeNode>(e).first_child;

        while (child != NullEntity) {
            iterate_inorder_recursive(registry, child, on_enter, on_exit);
            child = registry.get<TreeNode>(child).next_sibling;
        }

        on_exit(e, true);
    } else {
        on_exit(e, false);
    }
}

void iterate_inorder(
    Registry& registry,
    const std::function<bool(Entity)>& on_enter,
    const std::function<void(Entity, bool)>& on_exit)
{
    auto view = registry.view<TreeNode>();

    // For each root
    for (auto e : view) {
        if (view.get<TreeNode>(e).parent == NullEntity) {
            iterate_inorder_recursive(registry, e, on_enter, on_exit);
        }
    }
}

Entity group(Registry& registry, const std::vector<Entity>& entities, const std::string& name)
{
    return group_under(registry, entities, create_scene_node(registry, name));
}

Entity group_under(Registry& registry, const std::vector<Entity>& entities, Entity parent)
{
    for (auto e : entities) {
        set_parent(registry, e, parent);
    }
    return parent;
}

Entity get_parent(const Registry& registry, Entity e)
{
    if (!registry.valid(e)) return NullEntity;
    return registry.get<TreeNode>(e).parent;
}

std::vector<Entity> get_children(const Registry& registry, Entity e)
{
    std::vector<Entity> res;
    foreach_child(registry, e, [&](Entity e) { res.push_back(e); });
    return res;
}


Entity ungroup(Registry& registry, Entity parent, bool remove_parent /*= false*/)
{
    auto new_parent = get_parent(registry, parent);

    auto children = get_children(registry, parent);
    for (auto e : children) {
        set_parent(registry, e, new_parent);
    }

    if (remove_parent) {
        registry.destroy(parent);
    }

    return new_parent;
}

void orphan_without_subtree(Registry& registry, Entity e)
{
    assert(registry.all_of<TreeNode>(e));

    // Reparent any children of e to e's parent (i.e., move up one level)
    auto p = get_parent(registry, e);

    assert(p == NullEntity || registry.valid(p));

    foreach_child(registry, e, [&registry, new_parent = p](Entity child) {
        set_parent(registry, child, new_parent);
    });

    // Orphan
    orphan(registry, e);
}

void remove_from_tree(Registry& registry, Entity e)
{
    orphan_without_subtree(registry, e);
    registry.remove<TreeNode>(e);
}

} // namespace ui
} // namespace lagrange
