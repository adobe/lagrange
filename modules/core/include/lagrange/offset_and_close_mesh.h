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
#pragma once

#include <lagrange/Mesh.h>
#include <lagrange/MeshTrait.h>

namespace lagrange {

///
/// Offset a mesh, and close the shape into a thick 3D solid. The mesh is assumed to have a disk
/// topology. Input mesh vertices are duplicated and projected onto a target plane and can be
/// additionally mirrored wrt to this plane.
///
/// @param[in,out] input_mesh     Input mesh, assumed to have a disk topology. Modified to compute
///                               edge information.
/// @param[in]     direction      Offset direction.
/// @param[in]     offset_amount  Coordinate along the direction vector to project onto.
/// @param[in]     mirror_amount  Mirror amount (between -1 and 1). -1 means fully mirrored, 0 means
///                               flat region, and 1 means fully translated.
///
/// @tparam        MeshType       Mesh type.
///
/// @return        A mesh of the offset and closed surface.
///
template <typename MeshType>
std::unique_ptr<MeshType> offset_and_close_mesh(
    MeshType& input_mesh,
    Eigen::Matrix<ScalarOf<MeshType>, 3, 1> direction,
    ScalarOf<MeshType> offset_amount,
    ScalarOf<MeshType> mirror_amount)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");

    using Scalar = typename MeshType::Scalar;
    using Index = typename MeshType::Index;
    using VertexArray = typename MeshType::VertexArray;
    using UVArray = typename MeshType::UVArray;
    using UVIndices = typename MeshType::UVIndices;
    using FacetArray = typename MeshType::FacetArray;
    using Vector3s = Eigen::Matrix<Scalar, 3, 1>;

    LA_ASSERT(input_mesh.get_dim() == 3, "This function only supports 3D meshes.");
    LA_ASSERT(
        input_mesh.get_vertex_per_facet() == 3,
        "This function only supports triangle meshes.");

    direction.stableNormalize();
    const Vector3s mirror_vector = (Scalar(1) - mirror_amount) * direction;
    const Vector3s offset_vector = offset_amount * direction;
    const Index num_input_vertices = input_mesh.get_num_vertices();
    const Index num_input_facets = input_mesh.get_num_facets();
    const bool has_uvs = input_mesh.is_uv_initialized();
    const Index num_input_uvs = (has_uvs ? input_mesh.get_uv().rows() : 0);

    // Vertices
    VertexArray offset_vertices(num_input_vertices * 2, 3);
    for (Index v = 0; v < num_input_vertices; ++v) {
        Vector3s vertex = input_mesh.get_vertices().row(v).template head<3>();
        // copy original vertices
        offset_vertices.row(v) = vertex;
        // also opposite face
        offset_vertices.row(v + num_input_vertices) =
            vertex - vertex.dot(direction) * mirror_vector + offset_vector;
    }

    // Facets
    // 0. Count boundary edges in the input mesh
    input_mesh.initialize_edge_data_new();
    Index num_input_boundary_edges = 0;
    for (Index e = 0; e < input_mesh.get_num_edges_new(); ++e) {
        if (input_mesh.is_boundary_edge_new(e)) {
            ++num_input_boundary_edges;
        }
    }

    // 1. Build facets for front and back
    FacetArray offset_facets((num_input_facets + num_input_boundary_edges) * 2, 3);
    for (Index f = 0; f < num_input_facets; ++f) {
        const auto& facet = input_mesh.get_facets().row(f);
        offset_facets.row(2 * f) = facet;
        offset_facets.row(2 * f + 1) << facet[0] + num_input_vertices,
            facet[2] + num_input_vertices, facet[1] + num_input_vertices;
    }
    // 2. Stitch
    for (Index e = 0, f = 2 * num_input_facets; e < input_mesh.get_num_edges_new(); ++e) {
        if (input_mesh.is_boundary_edge_new(e)) {
            auto v = input_mesh.get_edge_vertices_new(e);
            assert(f + 1 < offset_facets.rows());
            offset_facets.row(f++) << v[0], v[0] + num_input_vertices, v[1];
            offset_facets.row(f++) << v[1], v[0] + num_input_vertices, v[1] + num_input_vertices;
        }
    }
    auto offset_mesh = lagrange::create_mesh(std::move(offset_vertices), std::move(offset_facets));

    if (has_uvs) {
        const auto& input_uv_values = input_mesh.get_uv();
        const auto& input_uv_indices = input_mesh.get_uv_indices();

        // UV values
        UVArray uv_values(num_input_uvs * 2, 2);
        for (Index u = 0; u < num_input_uvs; ++u) {
            // copy original uv and opposite face
            uv_values.row(u) = uv_values.row(u + num_input_uvs) = input_uv_values.row(u);
        }

        // UV facets
        UVIndices uv_facets((input_uv_indices.rows() + num_input_boundary_edges) * 2, 3);
        for (Index u = 0; u < input_uv_indices.rows(); ++u) {
            const auto& uv_facet = input_uv_indices.row(u);
            uv_facets.row(2 * u) = uv_facet;
            uv_facets.row(2 * u + 1) << uv_facet[0] + num_input_uvs, uv_facet[2] + num_input_uvs,
                uv_facet[1] + num_input_uvs;
        }
        // Stitch
        for (Index e = 0, f_uv = 2 * input_uv_indices.rows(); e < input_mesh.get_num_edges_new();
             ++e) {
            if (input_mesh.is_boundary_edge_new(e)) {
                // Find first and only face on this edge
                const Index f = input_mesh.get_one_facet_around_edge_new(e);
                assert(f != lagrange::INVALID<Index>());
                const auto& facet = input_mesh.get_facets().row(f);
                assert((facet.array() < num_input_vertices).all());
                const auto& uv_facet = input_uv_indices.row(f);
                assert((uv_facet.array() < num_input_uvs).all());
                // Find vertices on this edge
                auto edge_vertices = input_mesh.get_edge_vertices_new(e);
                // Now find corresponding uvs on this edge
                Index uv_index_0 = lagrange::INVALID<Index>();
                Index uv_index_1 = lagrange::INVALID<Index>();
                for (Index i = 0; i < 3; ++i) {
                    Index vtx_index = facet[i];
                    Index uv_index = uv_facet[i];
                    if (vtx_index == edge_vertices[0]) {
                        uv_index_0 = uv_index;
                    } else if (vtx_index == edge_vertices[1]) {
                        uv_index_1 = uv_index;
                    }
                }
                assert(uv_index_0 != lagrange::INVALID<Index>());
                assert(uv_index_1 != lagrange::INVALID<Index>());

                // finally, push on uv face buffer
                assert(f_uv + 1 < uv_facets.rows());
                uv_facets.row(f_uv++) << uv_index_0, uv_index_0 + num_input_uvs, uv_index_1;
                uv_facets.row(f_uv++) << uv_index_1, uv_index_0 + num_input_uvs,
                    uv_index_1 + num_input_uvs;
            }
        }

        // Apply UVs
        offset_mesh->initialize_uv(std::move(uv_values), std::move(uv_facets));
    }

    return offset_mesh;
}

} // namespace lagrange
