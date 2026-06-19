/*
 * Copyright 2026 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/xatlas/unwrap_mesh.h>

#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>

#include "AtlasEngine.h"

namespace lagrange::xatlas {

template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> unwrap_mesh(
    const SurfaceMesh<Scalar, Index>& mesh,
    const UnwrapOptions& options,
    std::function<void(const std::string&, float)> notification_func,
    const std::atomic_bool* cancel)
{
    if (mesh.get_num_facets() == 0) {
        logger().warn("lagrange::xatlas::unwrap_mesh: input mesh is empty; returning unchanged");
        return mesh;
    }

    AtlasEngine<Scalar, Index> engine;
    engine.set_notification_func(notification_func);
    engine.set_cancel(cancel);
    engine.add_mesh(mesh, options);
    engine.generate(options);
    return engine.extract_mesh(
        0,
        options.multi_atlas_policy,
        options.output_uv_attribute_name,
        options.output_atlas_attribute_name,
        options.output_chart_attribute_name);
}

#define LA_X_unwrap_mesh(_, S, I)                       \
    template SurfaceMesh<S, I> unwrap_mesh<S, I>(       \
        const SurfaceMesh<S, I>&,                       \
        const UnwrapOptions&,                           \
        std::function<void(const std::string&, float)>, \
        const std::atomic_bool*);
LA_SURFACE_MESH_X(unwrap_mesh, 0)
#undef LA_X_unwrap_mesh

} // namespace lagrange::xatlas
