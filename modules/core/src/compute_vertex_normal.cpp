/*
 * Copyright 2022 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */

#include <lagrange/AttributeFwd.h>
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/compute_weighted_corner_normal.h>
#include <lagrange/compute_vertex_normal.h>
#include <lagrange/utils/Error.h>
#include <lagrange/views.h>
#include "internal/compute_weighted_corner_normal.h"

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <tbb/parallel_for.h>
#include <lagrange/utils/warnon.h>
// clang-format on

namespace lagrange {

template <typename Scalar, typename Index>
AttributeId compute_vertex_normal(SurfaceMesh<Scalar, Index>& mesh, VertexNormalOptions options)
{
    la_runtime_assert(mesh.get_dimension() == 3, "Only 3D meshes are supported.");

    const Index num_vertices = mesh.get_num_vertices();
    AttributeId id;
    if (mesh.has_attribute(options.output_attribute_name)) {
        logger().info(
            "Attribute {} already exists, overwriting it.",
            options.output_attribute_name);
        id = mesh.get_attribute_id(options.output_attribute_name);
        la_runtime_assert(mesh.template is_attribute_type<Scalar>(id));
    } else {
        id = mesh.template create_attribute<Scalar>(
            options.output_attribute_name,
            Vertex,
            3,
            AttributeUsage::Normal);
    }
    auto normals = matrix_ref(mesh.template ref_attribute<Scalar>(id));
    la_debug_assert(static_cast<Index>(normals.rows()) == num_vertices);
    normals.setZero();

    if (mesh.has_edges()) {
        tbb::parallel_for(Index(0), num_vertices, [&](Index vi) {
            mesh.foreach_corner_around_vertex(vi, [&](Index ci) {
                normals.row(vi) +=
                    internal::compute_weighted_corner_normal(mesh, ci, options.weight_type);
            });
            normals.row(vi).stableNormalize();
        });
    } else {
        AttributeId corner_normal_id;
        constexpr char attr_name[] {"@corner_normal"};
        bool cleanup = false;
        if (mesh.has_attribute(attr_name)) {
            corner_normal_id = mesh.get_attribute_id(attr_name);
        } else {
            CornerNormalOptions c_options;
            c_options.output_attribute_name = attr_name;
            c_options.weight_type = options.weight_type;
            corner_normal_id = compute_weighted_corner_normal(mesh, std::move(c_options));
            cleanup = true;
        }
        auto corner_normals = matrix_view(mesh.template get_attribute<Scalar>(corner_normal_id));

        const Index num_corners = mesh.get_num_corners();
        for (Index ci = 0; ci < num_corners; ci++) {
            const Index vi = mesh.get_corner_vertex(ci);
            normals.row(vi) += corner_normals.row(ci);
        }

        // Note: For some reason stableNormalize() is not yet available as a vectorwise operation,
        // so we cannot simply call `vertex_normals.rowwise().stableNormalize()` and call it a day.
        tbb::parallel_for(Index(0), num_vertices, [&](Index v) {
            normals.row(v).stableNormalize();
        });

        if (cleanup) mesh.delete_attribute(attr_name);
    }

    return id;
}

#define LA_X_compute_vertex_normal(_, Scalar, Index)           \
    template AttributeId compute_vertex_normal<Scalar, Index>( \
        SurfaceMesh<Scalar, Index>&,                           \
        VertexNormalOptions);
LA_SURFACE_MESH_X(compute_vertex_normal, 0)
} // namespace lagrange
