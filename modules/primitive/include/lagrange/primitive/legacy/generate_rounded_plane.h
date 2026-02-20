/*
 * Copyright 2019 Adobe. All rights reserved.
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

#include <algorithm>
#include <vector>

#include <lagrange/Edge.h>
#include <lagrange/Mesh.h>
#include <lagrange/common.h>
#include <lagrange/compute_vertex_normal.h>
#include <lagrange/create_mesh.h>
#include <lagrange/legacy/inline.h>
#include <lagrange/mesh_cleanup/remove_degenerate_triangles.h>
#include <lagrange/mesh_cleanup/remove_duplicate_vertices.h>
#include <lagrange/primitive/generation_utils.h>
#include <lagrange/utils/safe_cast.h>


namespace lagrange {
namespace primitive {
LAGRANGE_LEGACY_INLINE
namespace legacy {

namespace Plane {

template <typename MeshType>
typename MeshType::VertexArray generate_vertices(
    typename MeshType::Scalar width,
    typename MeshType::Scalar height,
    typename MeshType::Scalar radius,
    const Eigen::Matrix<ScalarOf<MeshType>, 3, 1>& center);

template <typename VertexArray, typename FacetArray>
SubdividedMeshData<VertexArray, FacetArray> subdivide_corners(
    const VertexArray& vertices,
    const FacetArray& corner_tris,
    typename FacetArray::Scalar segments);

template <typename MeshType>
typename MeshType::FacetArray generate_facets();

template <typename MeshType>
typename MeshType::FacetArray generate_corner_triangles();

template <typename MeshType>
typename MeshType::FacetArray generate_quads();

template <typename MeshType>
typename MeshType::AttributeArray generate_uvs(
    typename MeshType::Scalar width,
    typename MeshType::Scalar height,
    typename MeshType::Scalar radius);

template <typename MeshType>
void generate_normals(MeshType& mesh);
} // namespace Plane

struct RoundedPlaneConfig
{
    using Scalar = float;
    using Index = uint32_t;

    ///
    /// @name Shape parameters.
    /// @{
    Scalar width = 1;
    Scalar height = 1;
    Scalar radius = 0;
    Index num_segments = 1;
    Eigen::Matrix<Scalar, 3, 1> center{0, 0, 0};

    /// @}
    /// @name Output parameters.
    /// @}
    bool output_normals = true;

    /// @}
    /// @name Tolerances.
    /// @{
    /**
     * Two vertices are considered coinciding iff the distance between them is
     * smaller than `dist_threshold`.
     */
    Scalar dist_threshold = static_cast<Scalar>(1e-6);
    /// @}

    /**
     * Project config setting into valid range.
     *
     * This method ensure all lengths parameters are non-negative, and clip the
     * radius parameter to its valid range.
     */
    void project_to_valid_range()
    {
        width = std::max(width, static_cast<Scalar>(0.0f));
        height = std::max(height, static_cast<Scalar>(0.0f));
        radius = std::min(
            std::max(radius, static_cast<Scalar>(0.0f)),
            (std::min(width, height)) / static_cast<Scalar>(2.0f));
        num_segments = std::max(num_segments, static_cast<Index>(0));
    }
};

template <typename MeshType>
std::unique_ptr<MeshType> generate_rounded_plane(RoundedPlaneConfig config)
{
    using VertexArray = typename MeshType::VertexArray;
    using FacetArray = typename MeshType::FacetArray;
    using Scalar = typename MeshType::Scalar;
    using Index = typename MeshType::Index;
    using VertexType = typename MeshType::VertexType;

    config.project_to_valid_range();

    /*
     * Handle Empty Mesh
     */
    if (config.width < config.dist_threshold || config.height < config.dist_threshold) {
        auto mesh = create_empty_mesh<VertexArray, FacetArray>();
        lagrange::set_uniform_semantic_label(*mesh, PrimitiveSemanticLabel::TOP);
        return mesh;
    }

    auto vertices = Plane::generate_vertices<MeshType>(
        config.width,
        config.height,
        2 * config.radius,
        config.center.template cast<Scalar>());
    auto facets = Plane::generate_facets<MeshType>();
    auto corner_tris = Plane::generate_corner_triangles<MeshType>();
    auto quads = Plane::generate_quads<MeshType>();
    auto uvs = Plane::generate_uvs<MeshType>(config.width, config.height, config.radius);
    FacetArray concat_tris;

    if (config.radius < config.dist_threshold) {
        auto mesh = lagrange::create_mesh(vertices, facets);

        // Set default uvs
        normalize_to_unit_box(uvs);
        mesh->initialize_uv(uvs, facets);
        mesh = remove_duplicate_vertices(*mesh, "", false);

        // Set normals
        if (config.output_normals) {
            Plane::generate_normals(*mesh);
        }

        // Set uniform semantic label
        lagrange::set_uniform_semantic_label(*mesh, PrimitiveSemanticLabel::TOP);

        return mesh;

    } else if (config.num_segments <= 1) {
        concat_tris.resize(facets.rows() + corner_tris.rows() + quads.rows(), 3);
        concat_tris << facets, corner_tris, quads;

        auto mesh = lagrange::create_mesh(vertices, concat_tris);

        // Set default uvs
        normalize_to_unit_box(uvs);
        mesh->initialize_uv(uvs, concat_tris);

        if (config.width > config.dist_threshold && config.height > config.dist_threshold) {
            mesh = remove_degenerate_triangles(*mesh);
        }

        // Set normals
        if (config.output_normals) {
            Plane::generate_normals(*mesh);
        }

        // Set uniform semantic label
        lagrange::set_uniform_semantic_label(*mesh, PrimitiveSemanticLabel::TOP);

        return mesh;
    }

    auto vertex_data =
        Plane::subdivide_corners(vertices, corner_tris, safe_cast<Index>(config.num_segments));

    auto uv_data =
        Plane::subdivide_corners(uvs, corner_tris, safe_cast<Index>(config.num_segments));

    auto output_vertices = vertex_data.vertices;
    auto output_uvs = uv_data.vertices;

    // Project 4 Corner Points and UVs to Circle
    Eigen::Matrix<Index, Eigen::Dynamic, 1> centers = corner_tris.col(0);
    assert(centers.rows() == 4);
    for (auto i : range(centers.rows())) {
        VertexType center = vertices.row(centers[i]);
        Eigen::Matrix<Scalar, Eigen::Dynamic, 2> uv_center = uvs.row(centers[i]);
        auto vertex_indices = vertex_data.segment_indices[i];
        auto uv_indices = uv_data.segment_indices[i];
        Index size = safe_cast<Index>(vertex_indices.size());
        for (Index col : range(size)) {
            // Project Vertices
            VertexType point = output_vertices.row(vertex_indices[col]);
            output_vertices.row(vertex_indices[col]) =
                project_to_sphere(center, point, config.radius);

            // Project UVs
            Eigen::Matrix<Scalar, Eigen::Dynamic, 2> uv_point = output_uvs.row(uv_indices[col]);
            output_uvs.row(uv_indices[col]) = project_to_sphere(uv_center, uv_point, config.radius);
        }
    }

    concat_tris.resize(facets.rows() + quads.rows() + vertex_data.triangles.rows(), 3);
    concat_tris << facets, quads, vertex_data.triangles;
    auto mesh = lagrange::create_mesh(output_vertices, concat_tris);

    // Set uvs
    normalize_to_unit_box(output_uvs);
    mesh->initialize_uv(output_uvs, concat_tris);

    // Clean mesh
    if (config.width > config.dist_threshold && config.height > config.dist_threshold) {
        mesh = remove_degenerate_triangles(*mesh);
    }

    // Set normals
    if (config.output_normals) {
        Plane::generate_normals(*mesh);
    }

    // Set uniform semantic label
    lagrange::set_uniform_semantic_label(*mesh, PrimitiveSemanticLabel::TOP);
    return mesh;
}

template <typename MeshType>
std::unique_ptr<MeshType> generate_rounded_plane(
    typename MeshType::Scalar width,
    typename MeshType::Scalar height,
    typename MeshType::Scalar radius,
    typename MeshType::Index num_segments)
{
    using Scalar = typename RoundedPlaneConfig::Scalar;
    using Index = typename RoundedPlaneConfig::Index;

    RoundedPlaneConfig config;
    config.width = safe_cast<Scalar>(width);
    config.height = safe_cast<Scalar>(height);
    config.radius = safe_cast<Scalar>(radius);
    config.num_segments = safe_cast<Index>(num_segments);

    return generate_rounded_plane<MeshType>(std::move(config));
}

namespace Plane {

template <typename MeshType>
typename MeshType::VertexArray generate_vertices(
    typename MeshType::Scalar width,
    typename MeshType::Scalar height,
    typename MeshType::Scalar radius,
    const Eigen::Matrix<ScalarOf<MeshType>, 3, 1>& center)
{
    typename MeshType::VertexArray vertices(12, 3);
    // clang-format off
    vertices <<
        // Plane Vertices
        // Top
        -width + radius, 0, height - radius,
        width - radius, 0, height - radius,

        // Bottom
        width - radius, 0, -height + radius,
        -width + radius, 0, -height + radius,

        // Top Bevelled Vertices
        -width + radius, 0, height,
        width - radius, 0, height,
        width, 0, height - radius,
        -width, 0, height - radius,

        // Bottom Bevelled Vertices
        -width, 0, -height + radius,
        width, 0, -height + radius,
        width - radius, 0, -height,
        -width + radius, 0, -height;
    // clang-format on

    return (vertices / 2).rowwise() + center.transpose();
}

template <typename MeshType>
typename MeshType::FacetArray generate_facets()
{
    typename MeshType::FacetArray facets(2, 3);
    facets << 0, 2, 3, 0, 1, 2;
    return facets;
}

template <typename MeshType>
typename MeshType::FacetArray generate_corner_triangles()
{
    typename MeshType::FacetArray triangles(4, 3);
    triangles <<
        // Top Triangles
        0,
        7, 4, 1, 5, 6,

        // Bottom Triangles
        2, 9, 10, 3, 11, 8;

    return triangles;
}

template <typename MeshType>
typename MeshType::FacetArray generate_quads()
{
    typename MeshType::FacetArray quads(8, 3);
    quads << 0, 4, 5, 0, 5, 1, 0, 3, 8, 0, 8, 7, 1, 6, 9, 1, 9, 2, 3, 2, 10, 3, 10, 11;
    return quads;
}

template <typename MeshType>
typename MeshType::AttributeArray generate_uvs(
    const typename MeshType::Scalar width,
    const typename MeshType::Scalar height,
    const typename MeshType::Scalar radius)
{
    typename MeshType::AttributeArray uvs(12, 2);
    const auto h = height - 2 * radius;
    const auto w = width - 2 * radius;
    const auto r = radius;
    // UVs per vertex

    // Top
    uvs.row(0) << r, r + h;
    uvs.row(1) << r + w, r + h;

    // Bottom
    uvs.row(2) << r + w, r;
    uvs.row(3) << r, r;

    // Top Bevelled
    uvs.row(4) << r, h + 2 * r;
    uvs.row(5) << r + w, h + 2 * r;
    uvs.row(6) << 2 * r + w, r + h;
    uvs.row(7) << 0, r + h;

    // Bottom Bevelled
    uvs.row(8) << 0, r;
    uvs.row(9) << 2 * r + w, r;
    uvs.row(10) << r + w, 0;
    uvs.row(11) << r, 0;

    uvs.col(1) *= -1;
    return uvs;
}

template <typename VertexArray, typename FacetArray>
SubdividedMeshData<VertexArray, FacetArray> subdivide_corners(
    const VertexArray& vertices,
    const FacetArray& corner_tris,
    typename FacetArray::Scalar num_segments)
{
    using Index = typename FacetArray::Scalar;
    using IndexList = std::vector<Index>;

    assert(corner_tris.rows() == 4);
    SubdividedMeshData<VertexArray, FacetArray> subdiv_data;
    std::vector<IndexList> index_list;
    FacetArray subdivided_tris(num_segments * 4, 3);
    VertexArray output_vertices = vertices;
    IndexList seg_indices;

    Index sub_idx = 0;
    for (Index i = 0; i < 4; i++) {
        auto triangle = corner_tris.row(i);
        std::tie(output_vertices, seg_indices) =
            divide_line_into_segments(output_vertices, triangle[1], triangle[2], num_segments);

        for (auto j : range(num_segments)) {
            subdivided_tris.row(sub_idx++) << triangle[0], seg_indices[j], seg_indices[j + 1];
        }
        index_list.push_back(seg_indices);
    }

    subdiv_data.vertices = output_vertices;
    subdiv_data.triangles = subdivided_tris;
    subdiv_data.segment_indices = index_list;
    return subdiv_data;
}

template <typename MeshType>
void generate_normals(MeshType& mesh)
{
    typename MeshType::AttributeArray normals(1, 3);
    normals << 0, 1, 0;
    typename MeshType::IndexArray indices(mesh.get_num_facets(), 3);
    indices.setZero();
    mesh.add_indexed_attribute("normal");
    mesh.set_indexed_attribute("normal", std::move(normals), std::move(indices));
}

} // namespace Plane

} // namespace legacy
} // namespace primitive
} // namespace lagrange
