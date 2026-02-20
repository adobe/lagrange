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
#include <lagrange/subdivision/compute_sharpness.h>

#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/cast_attribute.h>
#include <lagrange/compute_normal.h>
#include <lagrange/compute_seam_edges.h>
#include <lagrange/compute_vertex_valence.h>
#include <lagrange/find_matching_attributes.h>
#include <lagrange/subdivision/api.h>
#include <lagrange/utils/utils.h>
#include <lagrange/views.h>

namespace lagrange::subdivision {

template <typename Scalar, typename Index>
SharpnessResults compute_sharpness(
    SurfaceMesh<Scalar, Index>& mesh,
    const SharpnessOptions& options)
{
    // Find an attribute to use as indexed normal if possible (defines sharp edges)
    AttributeMatcher matcher;
    matcher.usages = AttributeUsage::Normal;
    matcher.element_types = AttributeElement::Indexed;
    std::optional<AttributeId> normal_id;
    for (AttributeId id : find_matching_attributes(mesh, matcher)) {
        if (options.normal_attribute_name.empty() ||
            mesh.get_attribute_name(id) == options.normal_attribute_name) {
            normal_id = id;
            break;
        }
    }
    if (normal_id.has_value()) {
        logger().debug(
            "Using indexed normal attribute: {}",
            mesh.get_attribute_name(normal_id.value()));
    } else if (options.feature_angle_threshold.has_value()) {
        logger().debug(
            "Computing autosmooth normals with a threshold of {} degrees",
            to_degrees(options.feature_angle_threshold.value()));
        normal_id = compute_normal<Scalar, Index>(
            mesh,
            static_cast<Scalar>(options.feature_angle_threshold.value()));
    }

    // Finally, compute edge sharpness info based on indexed normal topology
    SharpnessResults results;
    if (normal_id.has_value()) {
        logger().debug("Using mesh normals to set sharpness flag.");
        results.normal_attr = normal_id;

        auto seam_id = compute_seam_edges(mesh, normal_id.value());
        auto edge_sharpness_id = cast_attribute<float>(mesh, seam_id, "edge_sharpness");
        results.edge_sharpness_attr = edge_sharpness_id;

        // Set vertex sharpness to 1 for leaf and junction vertices
        VertexValenceOptions v_opts;
        v_opts.induced_by_attribute = mesh.get_attribute_name(seam_id);
        auto valence_id = compute_vertex_valence(mesh, v_opts);
        auto valence = attribute_vector_view<Index>(mesh, valence_id);
        auto vertex_sharpness_id = mesh.template create_attribute<float>(
            "vertex_sharpness",
            AttributeElement::Vertex,
            AttributeUsage::Scalar);
        auto vertex_sharpness = attribute_vector_ref<float>(mesh, vertex_sharpness_id);
        for (Index v = 0; v < mesh.get_num_vertices(); ++v) {
            vertex_sharpness[v] = (valence[v] == 1 || valence[v] > 2 ? 1.f : 0.f);
        }
        results.vertex_sharpness_attr = vertex_sharpness_id;
    }

    return results;
}

#define LA_X_compute_sharpness(_, Scalar, Index)                    \
    template LA_SUBDIVISION_API SharpnessResults compute_sharpness( \
        SurfaceMesh<Scalar, Index>& mesh,                           \
        const SharpnessOptions& options);
LA_SURFACE_MESH_X(compute_sharpness, 0)

} // namespace lagrange::subdivision
