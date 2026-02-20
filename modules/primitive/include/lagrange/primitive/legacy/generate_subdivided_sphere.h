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
#include <lagrange/Mesh.h>
#include <lagrange/attributes/attribute_utils.h>
#include <lagrange/create_mesh.h>
#include <lagrange/legacy/inline.h>
#include <lagrange/primitive/generation_utils.h>
#include <lagrange/subdivision/mesh_subdivision.h>

namespace lagrange {
namespace primitive {
LAGRANGE_LEGACY_INLINE
namespace legacy {

template <typename MeshType>
std::unique_ptr<MeshType> generate_subdivided_sphere(
    const MeshType& base_shape,
    const typename MeshType::Scalar radius,
    const Eigen::Matrix<typename MeshType::Scalar, 3, 1>& center = {0, 0, 0},
    const typename MeshType::Index num_subdivisions = 0)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");

    using VertexArray = typename MeshType::VertexArray;
    using VertexType = typename MeshType::VertexType;
    using UVArray = typename MeshType::UVArray;
    using Scalar = typename MeshType::Scalar;

    la_runtime_assert(radius >= 0.0, "Invalid radius: " + std::to_string(radius));
    la_runtime_assert(
        num_subdivisions >= 0,
        "Invalid number of subdivisions: " + std::to_string(num_subdivisions));

    auto mesh = create_mesh(base_shape.get_vertices(), base_shape.get_facets());

    if (num_subdivisions == 0) {
        // Copy over UVs if any
        if (base_shape.is_uv_initialized()) {
            mesh->initialize_uv(base_shape.get_uv(), base_shape.get_uv_indices());
        }
        lagrange::set_uniform_semantic_label(*mesh, PrimitiveSemanticLabel::SIDE);
        return mesh;
    }

    subdivision::SubdivisionScheme scheme_type;

    switch (mesh->get_vertex_per_facet()) {
    case 3:
        // Triangle Mesh
        scheme_type = subdivision::SubdivisionScheme::SCHEME_LOOP;
        break;
    case 4:
        // Quad Mesh
        scheme_type = subdivision::SubdivisionScheme::SCHEME_CATMARK;
        break;
    default: scheme_type = subdivision::SubdivisionScheme::SCHEME_CATMARK;
    }

    auto subdivided_mesh =
        subdivision::subdivide_mesh<MeshType, MeshType>(*mesh, scheme_type, num_subdivisions);

    VertexArray subdiv_vertices;
    subdivided_mesh->export_vertices(subdiv_vertices);

    const VertexType base_center =
        0.5 * (subdiv_vertices.colwise().minCoeff() + subdiv_vertices.colwise().maxCoeff());

    constexpr Scalar comparison_tol = 1e-5;
    const VertexArray centered_vertices = subdiv_vertices.rowwise() - base_center;

    if (centered_vertices.norm() > comparison_tol) {
        subdiv_vertices =
            (centered_vertices.rowwise().normalized() * radius).rowwise() + center.transpose();
    } else {
        subdiv_vertices = centered_vertices.rowwise() + center.transpose();
    }
    subdivided_mesh->import_vertices(subdiv_vertices);

    UVArray uvs = compute_spherical_uv_mapping<MeshType>(*subdivided_mesh, center);
    subdivided_mesh->add_corner_attribute("uv");
    subdivided_mesh->set_corner_attribute("uv", uvs);
    map_corner_attribute_to_indexed_attribute(*subdivided_mesh, "uv");
    subdivided_mesh->remove_corner_attribute("uv");
    assert(subdivided_mesh->is_uv_initialized());

    // Set uniform semantic label
    lagrange::set_uniform_semantic_label(*subdivided_mesh, PrimitiveSemanticLabel::SIDE);

    return subdivided_mesh;
}

} // namespace legacy
} // namespace primitive
} // namespace lagrange
