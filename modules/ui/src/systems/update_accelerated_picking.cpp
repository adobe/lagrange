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

#include <lagrange/ui/components/AcceleratedPicking.h>
#include <lagrange/ui/systems/update_accelerated_picking.h>
#include <lagrange/ui/utils/mesh_picking.h>

namespace lagrange {
namespace ui {

void update_accelerated_picking(Registry& r)
{
    auto dview = r.view<MeshDataDirty, MeshData, AcceleratedPicking>();

    for (auto e : dview) {
        const auto& mdd = dview.get<MeshDataDirty>(e);
        if (mdd.all || mdd.vertices) {
            // Recompute acceleration data structure
            enable_accelerated_picking(r, e);
        }
    }
}

} // namespace ui
} // namespace lagrange
