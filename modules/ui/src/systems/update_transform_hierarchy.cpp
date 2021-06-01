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
#include <lagrange/ui/components/Transform.h>
#include <lagrange/ui/systems/update_transform_hierarchy.h>
#include <lagrange/ui/utils/treenode.h>
#include <lagrange/ui/default_events.h>
#include <lagrange/ui/utils/events.h>

namespace lagrange {
namespace ui {

void update_transform_recursive(
    Registry& registry,
    Entity e,
    const Eigen::Affine3f& parent_global_transform,
    bool check_change = false
)
{
    assert(registry.has<TreeNode>(e));

    if (registry.has<Transform>(e)) {
        auto& transform = registry.get<Transform>(e);

        if (check_change) {
            auto new_global = (parent_global_transform * transform.local);
            if(transform.global.matrix() != new_global.matrix()){
                ui::publish<TransformChangedEvent>(registry, e);
            }
            transform.global = new_global;
        }
        else {
            transform.global = (parent_global_transform * transform.local);
        }
        

        foreach_child(registry, e, [&](Entity e) {
            update_transform_recursive(registry, e, transform.global, check_change);
        });
    } else {
        // Passthrough if entity doesn't have Transform component
        foreach_child(registry, e, [&](Entity e) {
            update_transform_recursive(registry, e, parent_global_transform, check_change);
        });
    }
}

void update_transform_hierarchy(Registry& registry)
{
    auto view = registry.view<TreeNode>();

    const bool check_change = !(ui::get_event_emitter(registry).empty<TransformChangedEvent>());

    for (auto e : view) {
        if (view.get<TreeNode>(e).parent == NullEntity) {
            update_transform_recursive(registry, e, Eigen::Affine3f::Identity(), check_change);
        }
    }
}


} // namespace ui
} // namespace lagrange