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

#include <lagrange/ui/components/Transform.h>
#include <lagrange/ui/systems/update_scene_bounds.h>
#include <lagrange/ui/utils/bounds.h>
#include <lagrange/ui/utils/treenode.h>

namespace lagrange {
namespace ui {

void update_scene_bounds_system(Registry& registry)
{
    // Transform local bounds to global, using the transform
    {
        auto view = registry.view<Transform, Bounds>();
        for (auto e : view) {
            auto& b = view.get<Bounds>(e);
            if (!b.local.isEmpty()) {
                b.global = b.local.transformed(view.get<Transform>(e).global);
            }
        }
    }

    struct AABBDirty
    {
        bool _dummy;
    };

    // Mark all leafs with bounds as dirty
    auto view = registry.view<TreeNode>();
    for (auto e : view) {
        // Skip inner
        if (view.get<TreeNode>(e).num_children != 0) {
            // Clear previous bvh values
            if (registry.all_of<Bounds>(e)) {
                registry.get<Bounds>(e).bvh_node = AABB{};
            }
            continue;
        }

        // Skip non bounded entities
        if (!registry.all_of<Bounds>(e)) continue;

        registry.emplace_or_replace<AABBDirty>(e, AABBDirty{});

        // Assign bvh node to leaf
        auto& b = registry.get<Bounds>(e);
        b.bvh_node = b.global;
    }

    auto& scene_bounds = get_scene_bounds(registry);

    // Reset scene bounds
    scene_bounds.local = AABB();

    // Iterate until there's no dirty bounds
    bool dirty = false;
    do {
        dirty = false;
        auto tmp_view = registry.view<AABBDirty>();
        for (auto e : tmp_view) {
            registry.remove<AABBDirty>(e);

            auto parent = get_parent(registry, e);


            AABB bvh_node;


            if (registry.all_of<Bounds>(e)) {
                bvh_node = registry.get<Bounds>(e).bvh_node;
            } else if (registry.all_of<Transform>(e)) {
                const auto pos =
                    registry.get<Transform>(e).global.matrix().block<3, 1>(0, 3).eval();
                bvh_node = AABB(pos, pos);
            } else {
                // Not a node with any position data
                continue;
            }

            // Root level nodes -> apply to scene bounds
            if (parent == NullEntity) {
                scene_bounds.local.extend(bvh_node);
                continue;
            }


            // If parent does not have bounds component yet
            if (!registry.all_of<Bounds>(parent)) {
                auto& parent_bounds = registry.emplace<Bounds>(parent);
                // Empty bounds
                parent_bounds.local = AABB();
                parent_bounds.global = AABB();
                // Init with child's bvh
                parent_bounds.bvh_node = bvh_node;
            }

            // Update parent and mark as dirty
            auto& parent_bounds = registry.get<Bounds>(parent);
            parent_bounds.bvh_node.extend(bvh_node);
            registry.emplace_or_replace<AABBDirty>(parent, AABBDirty{});

            dirty = true;
        }

    } while (dirty);


    // Update scene bounds
    scene_bounds.bvh_node = scene_bounds.local;
    scene_bounds.global = scene_bounds.local;
}


} // namespace ui
} // namespace lagrange
