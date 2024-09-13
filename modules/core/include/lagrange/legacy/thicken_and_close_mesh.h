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
#include <lagrange/attributes/map_indexed_attributes.h>
#include <lagrange/compute_vertex_normal.h>
#include <lagrange/create_mesh.h>
#include <lagrange/legacy/inline.h>

#include <unordered_map>

namespace lagrange {
LAGRANGE_LEGACY_INLINE
namespace legacy {

namespace {

///
/// Thicken a mesh by offsetting it in a fixed direction or along the normals.
/// Close the shape into a thick 3D solid. Input mesh vertices are duplicated and projected onto a target plane and can be
/// additionally mirrored wrt to this plane.
///
/// @param[in]     input_mesh                   Input mesh, Must have edge information included.
///                                             edge information.
/// @param[in]     use_direction_and_mirror     If on, uses a fixed direction for the thickening.
/// @param[in]     direction                    Offset direction.
/// @param[in]     offset_amount                Coordinate along the direction vector to project onto.
/// @param[in]     mirror_amount                Mirror amount (between -1 and 1). -1 means fully mirrored, 0 means
///                                             flat region, and 1 means fully translated.
/// @param[in]     num_segments                 Number of segments the stitched are should be split in.
///
/// @tparam        MeshType                     Mesh type.
///
/// @return        A mesh of the offset and closed surface.
///

template <typename MeshType>
std::unique_ptr<MeshType> thicken_and_close_mesh(
    const MeshType& input_mesh,
    bool use_direction_and_mirror,
    Eigen::Matrix<ScalarOf<MeshType>, 3, 1> direction,
    ScalarOf<MeshType> offset_amount,
    ScalarOf<MeshType> mirror_amount,
    typename MeshType::Index num_segments = 1)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");

    using Scalar = typename MeshType::Scalar;
    using Index = typename MeshType::Index;
    using VertexArray = typename MeshType::VertexArray;
    using AttributeArray = typename MeshType::AttributeArray;
    using UVArray = typename MeshType::UVArray;
    using UVIndices = typename MeshType::UVIndices;
    using FacetArray = typename MeshType::FacetArray;
    using Vector3s = Eigen::Matrix<Scalar, 3, 1>;

    la_runtime_assert(input_mesh.get_dim() == 3, "This function only supports 3D meshes.");
    la_runtime_assert(
        input_mesh.get_vertex_per_facet() == 3,
        "This function only supports triangle meshes.");
    la_runtime_assert(
        input_mesh.is_edge_data_initialized(),
        "This function requires the mesh to have edge data pre-initialized.");

    // compute offset vertex
    auto compute_vertex = [](const Vector3s& vertex,
                             const Vector3s& offset_vector,
                             const Vector3s& mirror_vector,
                             const Vector3s& target_direction,
                             bool with_direction_and_mirror,
                             Scalar amount) {
        Vector3s offset_vertex = vertex + amount * offset_vector;
        if (with_direction_and_mirror) {
            offset_vertex -= (offset_vertex.dot(target_direction) * amount) * mirror_vector;
        }
        return offset_vertex;
    };

    // Sanitize parameters
    if (num_segments < 1) num_segments = 1;
    direction.stableNormalize();

    // Prepare ancillary data
    Vector3s offset_vector(0.0, 1.0, 0.0);
    Vector3s mirror_vector(0.0, 0.0, 0.0);
    AttributeArray vertex_normal;
    if (use_direction_and_mirror) {
        offset_vector = offset_amount * direction;
        mirror_vector = (Scalar(1) - mirror_amount) * direction;
    } else {
        // compute vertex normal vector
        std::unique_ptr<MeshType> copied_mesh = wrap_with_mesh<VertexArray, FacetArray>(
            input_mesh.get_vertices(),
            input_mesh.get_facets());
        compute_vertex_normal(*copied_mesh);
        copied_mesh->export_vertex_attribute("normal", vertex_normal);
        la_runtime_assert(vertex_normal.rows() == input_mesh.get_num_vertices());
    }
    const Index num_input_vertices = input_mesh.get_num_vertices();
    const Index num_input_facets = input_mesh.get_num_facets();
    const bool has_uvs = input_mesh.is_uv_initialized();
    const Index num_input_uvs = static_cast<Index>(has_uvs ? input_mesh.get_uv().rows() : 0);

    // Count boundary edges and vertices in the input mesh
    Index num_boundary_edges = 0;
    Index num_boundary_vertices = 0;
    std::unordered_map<Index, Index>
        boundary_vertices; // (key is vertex index, value is a boundary-relative index)
    for (Index e = 0; e < input_mesh.get_num_edges(); ++e) {
        if (input_mesh.is_boundary_edge(e)) {
            ++num_boundary_edges;
            for (auto v : input_mesh.get_edge_vertices(e)) { // for each of two vertices
                boundary_vertices.emplace(v, Index(boundary_vertices.size()));
            }
        }
    }
    num_boundary_vertices = static_cast<Index>(boundary_vertices.size());

    // Vertices
    // output vertices are packed as follows:
    // 1. original vertices (num_input_vertices)
    // 2. offset vertices (num_input_vertices)
    // 3. stitch vertices ((num_segments - 1) * num_boundary_vertices), packed by segment
    VertexArray offset_vertices(
        num_input_vertices * 2 + (num_segments - 1) * num_boundary_vertices,
        3);
    for (Index v = 0; v < num_input_vertices; ++v) {
        if (!use_direction_and_mirror) {
            // compute per vertex normal and use it as offset direction
            // TODO: a smarter version could compensate for tight angles with disjoint
            // indexed normals and amplify this vector to preserve the apparent thickness
            // of the resulting solid.
            offset_vector = -vertex_normal.row(v).template head<3>() * offset_amount;
        }
        Vector3s vertex = input_mesh.get_vertices().row(v).template head<3>();
        // copy original vertices
        offset_vertices.row(v) = vertex;
        // also opposite face
        auto offset_vertex = compute_vertex(
            vertex,
            offset_vector,
            mirror_vector,
            direction,
            use_direction_and_mirror,
            1.0);
        offset_vertices.row(num_input_vertices + v) = offset_vertex;

        // is this a boundary vertex? Add intermediate vertices for the stitch
        Scalar segment_increment = Scalar(1) / static_cast<Scalar>(num_segments);
        if (num_segments > 1) {
            auto vb = boundary_vertices.find(v);
            if (vb != boundary_vertices.end()) {
                for (Index is = 1; is < num_segments; ++is) {
                    Scalar offset_ratio = static_cast<Scalar>(is) * segment_increment;
                    assert(offset_ratio < 1.0);
                    offset_vertices.row(
                        num_input_vertices * 2 + // original + offset vertices
                        (is - 1) * num_boundary_vertices + // segment row
                        vb->second // index in the boundary
                        ) =
                        compute_vertex(
                            vertex,
                            offset_vector,
                            mirror_vector,
                            direction,
                            use_direction_and_mirror,
                            offset_ratio);
                }
            }
        }
    }


    // Facets
    // output facets are packed as follows:
    // 1. original facets interleaved with flipped facets (num_input_facets*2)
    // 2. stitch vertices (num_boundary_edges * num_boundary_vertices), packed by segment
    FacetArray offset_facets((num_input_facets + num_boundary_edges * num_segments) * 2, 3);
    for (Index f = 0; f < num_input_facets; ++f) {
        const auto& facet = input_mesh.get_facets().row(f);
        offset_facets.row(2 * f) = facet;
        offset_facets.row(2 * f + 1) << facet[0] + num_input_vertices,
            facet[2] + num_input_vertices,
            facet[1] + num_input_vertices; // this face is flipped: order of vertices is reversed
    }

    // 2. Stitch
    Index vbstart = num_input_vertices * 2;
    for (Index e = 0, f = 2 * num_input_facets; e < input_mesh.get_num_edges(); ++e) {
        if (input_mesh.is_boundary_edge(e)) {
            auto edge_vertices = input_mesh.get_edge_vertices(e);
            assert(boundary_vertices.find(edge_vertices[0]) != boundary_vertices.end());
            assert(boundary_vertices.find(edge_vertices[1]) != boundary_vertices.end());
            assert(f + 1 < offset_facets.rows());

            for (Index is = 0; is < num_segments; ++is) {
                Index vbstart_on_this_segment = vbstart + (is - 1) * num_boundary_vertices;
                Index vbstart_on_next_segment = vbstart + is * num_boundary_vertices;
                bool is_first_segment = (is == 0);
                bool is_last_segment = (is == num_segments - 1);
                Index v0 = is_first_segment
                               ? edge_vertices[0]
                               : vbstart_on_this_segment + boundary_vertices[edge_vertices[0]];
                Index v1 = is_first_segment
                               ? edge_vertices[1]
                               : vbstart_on_this_segment + boundary_vertices[edge_vertices[1]];
                Index v2 = is_last_segment
                               ? edge_vertices[0] + num_input_vertices
                               : vbstart_on_next_segment + boundary_vertices[edge_vertices[0]];
                Index v3 = is_last_segment
                               ? edge_vertices[1] + num_input_vertices
                               : vbstart_on_next_segment + boundary_vertices[edge_vertices[1]];

                assert(v0 < offset_vertices.rows());
                assert(v1 < offset_vertices.rows());
                assert(v2 < offset_vertices.rows());
                assert(v3 < offset_vertices.rows());

                offset_facets.row(f++) << v0, v2, v1;
                offset_facets.row(f++) << v1, v2, v3;
            }
        }
    }

    auto offset_mesh = lagrange::create_mesh(std::move(offset_vertices), std::move(offset_facets));

    if (has_uvs) {
        const auto& input_uv_values = input_mesh.get_uv();
        const auto& input_uv_indices = input_mesh.get_uv_indices();

        // UV values
        UVArray uv_values(num_input_uvs * 2 + (num_segments - 1) * num_boundary_vertices, 2);
        for (Index u = 0; u < num_input_uvs; ++u) {
            // copy original uv and opposite face
            uv_values.row(u) = uv_values.row(u + num_input_uvs) = input_uv_values.row(u);
        }
        // additional uv values for num_segments
        for (Index f = 0; f < input_mesh.get_facets().rows(); ++f) {
            // find individual uvs indices
            const auto& uv_facet = input_mesh.get_uv_indices().row(f);
            // get face indices for this same facet
            const auto& facet = input_mesh.get_facets().row(f);
            // for each uv on the face:
            for (Index fv = 0; fv < 3; ++fv) {
                Index v = facet[fv];
                auto vb = boundary_vertices.find(v);
                if (vb != boundary_vertices.end()) {
                    // this is a boundary vertex.
                    Index uv_index = uv_facet[fv];
                    auto uv_value = input_mesh.get_uv().row(uv_index);

                    // add overlapping identical values for uvs at the stitch...
                    // at some point we might want to have non overlapping uvs. Not sure if it's in
                    // scope for this.
                    for (Index is = 1; is < num_segments; ++is) {
                        uv_values.row(
                            num_input_vertices * 2 + // original + offset vertices
                            (is - 1) * num_boundary_vertices + // segment row
                            vb->second) = uv_value;
                    }
                }
            }
        }

        // UV facets
        UVIndices uv_facets((input_uv_indices.rows() + num_boundary_edges * num_segments) * 2, 3);
        for (Index u = 0; u < input_uv_indices.rows(); ++u) {
            const auto& uv_facet = input_uv_indices.row(u);
            uv_facets.row(2 * u) = uv_facet;
            uv_facets.row(2 * u + 1) << uv_facet[0] + num_input_uvs, uv_facet[2] + num_input_uvs,
                uv_facet[1] + num_input_uvs;
        }

        // Stitch
        Index uvbstart = static_cast<Index>(2 * input_uv_values.rows());
        for (Index e = 0, f_uv = 2 * static_cast<Index>(input_uv_indices.rows());
             e < input_mesh.get_num_edges();
             ++e) {
            if (input_mesh.is_boundary_edge(e)) {
                // Find first and only face on this edge
                const Index f = input_mesh.get_one_facet_around_edge(e);
                assert(f != lagrange::invalid<Index>());
                const auto& facet = input_mesh.get_facets().row(f);
                assert((facet.array() < num_input_vertices).all());
                const auto& uv_facet = input_uv_indices.row(f);
                assert((uv_facet.array() < num_input_uvs).all());
                // Find vertices on this edge
                auto edge_vertices = input_mesh.get_edge_vertices(e);
                // Now find corresponding uvs on this edge
                Index uv_index_0 = lagrange::invalid<Index>();
                Index uv_index_1 = lagrange::invalid<Index>();
                for (Index i = 0; i < 3; ++i) {
                    Index vtx_index = facet[i];
                    Index uv_index = uv_facet[i];
                    if (vtx_index == edge_vertices[0]) {
                        uv_index_0 = uv_index;
                    } else if (vtx_index == edge_vertices[1]) {
                        uv_index_1 = uv_index;
                    }
                }
                assert(uv_index_0 != lagrange::invalid<Index>());
                assert(uv_index_1 != lagrange::invalid<Index>());

                assert(boundary_vertices.find(edge_vertices[0]) != boundary_vertices.end());
                assert(boundary_vertices.find(edge_vertices[1]) != boundary_vertices.end());
                assert(f_uv + 1 < uv_facets.rows());

                for (Index is = 0; is < num_segments; ++is) {
                    Index uvbstart_on_this_segment = uvbstart + (is - 1) * num_boundary_vertices;
                    Index uvbstart_on_next_segment = uvbstart + is * num_boundary_vertices;
                    bool is_first_segment = (is == 0);
                    bool is_last_segment = (is == num_segments - 1);

                    Index uv0 = is_first_segment ? uv_index_0
                                                 : uvbstart_on_this_segment +
                                                       boundary_vertices[edge_vertices[0]];
                    Index uv1 = is_first_segment ? uv_index_1
                                                 : uvbstart_on_this_segment +
                                                       boundary_vertices[edge_vertices[1]];
                    Index uv2 = is_last_segment ? uv_index_0 + num_input_vertices
                                                : uvbstart_on_next_segment +
                                                      boundary_vertices[edge_vertices[0]];
                    Index uv3 = is_last_segment ? uv_index_1 + num_input_vertices
                                                : uvbstart_on_next_segment +
                                                      boundary_vertices[edge_vertices[1]];

                    assert(uv0 < uv_values.rows());
                    assert(uv1 < uv_values.rows());
                    assert(uv2 < uv_values.rows());
                    assert(uv3 < uv_values.rows());

                    uv_facets.row(f_uv++) << uv0, uv2, uv1;
                    uv_facets.row(f_uv++) << uv1, uv2, uv3;
                }
            }
        }

        // Apply UVs
        assert(uv_values.rows() == num_input_uvs * 2 + (num_segments - 1) * num_boundary_vertices);
        offset_mesh->initialize_uv(std::move(uv_values), std::move(uv_facets));
    }

    // sanity check
    assert(
        offset_mesh->get_num_vertices() ==
        num_input_vertices * 2 + num_boundary_vertices * (num_segments - 1));

    return offset_mesh;
}

} // namespace

///
/// Thicken a mesh by offsetting it in a fixed direction.
/// Close the shape into a thick 3D solid. The mesh is assumed to have a disk
/// topology. Input mesh vertices are duplicated and projected onto a target plane and can be
/// additionally mirrored wrt to this plane.
///
/// @param[in] input_mesh         Input mesh, assumed to have a disk topology. Must have edge
///                               information included.
/// @param[in]     direction      Offset direction.
/// @param[in]     offset_amount  Coordinate along the direction vector to project onto.
/// @param[in]     mirror_amount  Mirror amount (between -1 and 1). -1 means fully mirrored, 0 means
///                               flat region, and 1 means fully translated.
/// @param[in]     num_segments   Number of segments the stitched are should be split in.
///
/// @tparam        MeshType       Mesh type.
///
/// @return        A mesh of the offset and closed surface.
///
template <typename MeshType>
std::unique_ptr<MeshType> thicken_and_close_mesh(
    const MeshType& input_mesh,
    Eigen::Matrix<ScalarOf<MeshType>, 3, 1> direction,
    ScalarOf<MeshType> offset_amount,
    ScalarOf<MeshType> mirror_amount,
    typename MeshType::Index num_segments = 1)
{
    return thicken_and_close_mesh(
        input_mesh,
        true,
        direction,
        offset_amount,
        mirror_amount,
        num_segments);
}

///
/// Thicken a mesh vertices along normals, and close the shape into a thick 3D solid.
/// This function makes no assumption on the shape's topology and will apply its effect nicely to any surface,
/// even those that have no holes (e.g. a solid sphere will become a hollow sphere with a solid shell).
///
/// @param[in]     input_mesh     Input mesh. Must have edge information included.
/// @param[in]     offset_amount  Coordinate along the direction vector to project onto.
/// @param[in]     num_segments   Number of segments the stitched are should be split in.
///
/// @tparam        MeshType       Mesh type.
///
/// @return        A mesh of the offset and closed surface.
///
template <typename MeshType>
std::unique_ptr<MeshType> thicken_and_close_mesh(
    const MeshType& input_mesh,
    ScalarOf<MeshType> offset_amount,
    typename MeshType::Index num_segments = 1)
{
    return thicken_and_close_mesh(
        input_mesh,
        false,
        Eigen::Matrix<ScalarOf<MeshType>, 3, 1>(0.0, 1.0, 0.0),
        offset_amount,
        0.f,
        num_segments);
}

} // namespace legacy
} // namespace lagrange
