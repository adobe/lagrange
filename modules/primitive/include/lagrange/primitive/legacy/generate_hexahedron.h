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
#include <lagrange/create_mesh.h>
#include <lagrange/legacy/inline.h>
#include <lagrange/primitive/generation_utils.h>
#include <lagrange/subdivision/mesh_subdivision.h>


namespace lagrange {
namespace primitive {
LAGRANGE_LEGACY_INLINE
namespace legacy {

namespace Hexahedron {

template <typename Scalar>
constexpr Scalar TOLERANCE = safe_cast<Scalar>(1e-6);

template <typename MeshType>
typename MeshType::VertexArray generate_vertices(
    const typename MeshType::Scalar width,
    const typename MeshType::Scalar height,
    const typename MeshType::Scalar depth);

template <typename MeshType>
typename MeshType::FacetArray generate_facets();

} // namespace Hexahedron

template <typename MeshType>
std::unique_ptr<MeshType> generate_hexahedron(
    typename MeshType::Scalar radius,
    const Eigen::Matrix<typename MeshType::Scalar, 3, 1>& center = {0, 0, 0})
{
    using namespace Hexahedron;

    using VertexArray = typename MeshType::VertexArray;
    using FacetArray = typename MeshType::FacetArray;
    using Scalar = typename MeshType::Scalar;
    using UVArray = typename MeshType::UVArray;
    using UVIndices = typename MeshType::UVIndices;

    la_runtime_assert(radius >= 0.0, "Invalid radius: " + std::to_string(radius));

    Scalar length = 2.0 / sqrt(3) * radius;
    VertexArray vertices = generate_vertices<MeshType>(length, length, length);
    FacetArray facets = generate_facets<MeshType>();

    // Translate to center
    if (center.squaredNorm() > TOLERANCE<Scalar>) {
        vertices.rowwise() += center.transpose();
    }

    auto mesh = create_mesh(vertices, facets);
    UVArray uvs(14, 2);
    uvs << 0.0, 0.25, 0.25, 0.25, 0.5, 0.25, 0.75, 0.25, 1.0, 0.25, 0.0, 0.5, 0.25, 0.5, 0.5, 0.5,
        0.75, 0.5, 1.0, 0.5, 0.25, 0.0, 0.5, 0.0, 0.25, 0.75, 0.5, 0.75;

    UVIndices uv_indices(6, 4);
    uv_indices << 10, 11, 2, 1, 12, 6, 7, 13, 1, 2, 7, 6, 3, 4, 9, 8, 0, 1, 6, 5, 2, 3, 8, 7;
    mesh->initialize_uv(uvs, uv_indices);

    // Set uniform semantic label
    lagrange::set_uniform_semantic_label(*mesh, PrimitiveSemanticLabel::SIDE);

    return mesh;
}

namespace Hexahedron {

template <typename MeshType>
typename MeshType::VertexArray generate_vertices(
    const typename MeshType::Scalar width,
    const typename MeshType::Scalar height,
    const typename MeshType::Scalar depth)
{
    typename MeshType::VertexArray vertices(8, 3);

    // Bottom
    vertices.row(0) << -width, -height, depth;
    vertices.row(1) << -width, -height, -depth;
    vertices.row(2) << width, -height, -depth;
    vertices.row(3) << width, -height, depth;

    // Top
    vertices.row(4) << -width, height, depth;
    vertices.row(5) << width, height, depth;
    vertices.row(6) << width, height, -depth;
    vertices.row(7) << -width, height, -depth;

    return vertices / 2;
}

template <typename MeshType>
typename MeshType::FacetArray generate_facets()
{
    typename MeshType::FacetArray facets(6, 4);

    facets <<
        // bottom
        0,
        1, 2, 3,
        // top
        4, 5, 6, 7,
        // front
        3, 2, 6, 5,
        // back
        1, 0, 4, 7,
        // left
        0, 3, 5, 4,
        // right
        2, 1, 7, 6;

    return facets;
}
} // namespace Hexahedron
} // namespace legacy
} // namespace primitive
} // namespace lagrange
