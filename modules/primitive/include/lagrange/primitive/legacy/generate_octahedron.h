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

namespace Octahedron {

template <typename Scalar>
constexpr Scalar TOLERANCE = safe_cast<Scalar>(1e-6);

template <typename MeshType>
typename MeshType::VertexArray generate_vertices(const typename MeshType::Scalar radius);

template <typename MeshType>
typename MeshType::FacetArray generate_facets();

} // namespace Octahedron

template <typename MeshType>
std::unique_ptr<MeshType> generate_octahedron(
    const typename MeshType::Scalar radius,
    const Eigen::Matrix<typename MeshType::Scalar, 3, 1>& center = {0, 0, 0})
{
    using namespace Octahedron;

    using VertexArray = typename MeshType::VertexArray;
    using FacetArray = typename MeshType::FacetArray;
    using Scalar = typename MeshType::Scalar;
    using UVArray = typename MeshType::UVArray;

    la_runtime_assert(radius >= 0.0, "Invalid radius: " + std::to_string(radius));

    VertexArray vertices = generate_vertices<MeshType>(radius);
    FacetArray facets = generate_facets<MeshType>();

    // Translate to center
    if (center.squaredNorm() > TOLERANCE<Scalar>) {
        vertices.rowwise() += center.transpose();
    }

    auto mesh = create_mesh(vertices, facets);
    UVArray uvs = compute_spherical_uv_mapping<MeshType>(*mesh, center);
    mesh->add_corner_attribute("uv");
    mesh->set_corner_attribute("uv", uvs);
    map_corner_attribute_to_indexed_attribute(*mesh, "uv");
    mesh->remove_corner_attribute("uv");

    // Set uniform semantic label
    lagrange::set_uniform_semantic_label(*mesh, PrimitiveSemanticLabel::SIDE);
    return mesh;
}

namespace Octahedron {
template <typename MeshType>
typename MeshType::VertexArray generate_vertices(const typename MeshType::Scalar radius)
{
    typename MeshType::VertexArray vertices(6, 3);

    vertices.row(0) << 0.0, -1.0, 0.0;
    vertices.row(1) << 0.0, 0.0, 1.0;
    vertices.row(2) << -1.0, 0.0, 0.0;
    vertices.row(3) << 0.0, 0.0, -1.0;
    vertices.row(4) << 1.0, 0.0, 0.0;
    vertices.row(5) << 0.0, 1.0, 0.0;

    return radius * vertices;
}

template <typename MeshType>
typename MeshType::FacetArray generate_facets()
{
    typename MeshType::FacetArray facets(8, 3);

    facets << 0, 1, 2, 0, 2, 3, 0, 3, 4, 0, 4, 1,

        5, 2, 1, 5, 3, 2, 5, 4, 3, 5, 1, 4;

    return facets;
}
} // namespace Octahedron
} // namespace legacy
} // namespace primitive
} // namespace lagrange
