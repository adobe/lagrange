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

#include <lagrange/ui/components/MeshGeometry.h>
#include <lagrange/ui/components/Transform.h>
#include <lagrange/ui/systems/update_mesh_bounds.h>
#include <lagrange/ui/utils/bounds.h>
#include <lagrange/ui/utils/mesh.h>


namespace lagrange {
namespace ui {


void update_mesh_bounds_system(Registry& r)
{
    {
        // Dirty view, reset bounds
        auto dview = r.view<MeshDataDirty, MeshData>();
        for (auto e : dview) {
            if (!r.has<Bounds>(e)) continue;
            r.remove<Bounds>(e);
        }
    }

    {
        // Compute bounds
        auto md_view = r.view<MeshData>();
        for (auto e : md_view) {
            if (r.has<Bounds>(e)) continue;

            auto bb = get_mesh_bounds(r.get<MeshData>(e));

            // Set local == global
            r.emplace<Bounds>(e, Bounds{bb, bb, bb});
        }
    }


    auto view = r.view<const MeshGeometry, const Transform>();

    // Update instances of the mesh
    for (auto e : view) {
        // Get the geometry entity
        const auto g = view.get<const MeshGeometry>(e).entity;
        if (!r.valid(g) || !r.has<Bounds>(g)) continue;

        const auto mesh_bb = r.get<Bounds>(g).global;
        Bounds bounds;
        bounds.local = mesh_bb;
        r.emplace_or_replace<Bounds>(e, bounds);
    }
}


} // namespace ui
} // namespace lagrange
