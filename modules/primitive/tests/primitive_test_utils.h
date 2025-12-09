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

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
    #include <lagrange/ExactPredicatesShewchuk.h>
    #include <lagrange/Mesh.h>
    #include <lagrange/mesh_cleanup/detect_degenerate_triangles.h>
    #include <lagrange/primitive/legacy/generation_utils.h>

    #include <catch2/catch_approx.hpp>
#endif

#include <lagrange/SurfaceMesh.h>
#include <lagrange/compute_components.h>
#include <lagrange/extract_boundary_loops.h>
#include <lagrange/mesh_cleanup/detect_degenerate_facets.h>
#include <lagrange/testing/common.h>
#include <lagrange/topology.h>
#include <lagrange/uv_mesh.h>

namespace lagrange {
namespace primitive_test_utils {

template <typename Scalar, typename Index>
void validate_primitive(SurfaceMesh<Scalar, Index>& mesh, int num_boundaries = 0)
{
    REQUIRE(is_vertex_manifold(mesh));
    REQUIRE(is_edge_manifold(mesh));
    REQUIRE(compute_components(mesh) == 1);

    const auto bd_loops = lagrange::extract_boundary_loops(mesh);
    REQUIRE(safe_cast<int>(bd_loops.size()) == num_boundaries);
}

template <typename Scalar, typename Index>
void check_degeneracy(SurfaceMesh<Scalar, Index>& mesh)
{
    auto degenerate_facets = lagrange::detect_degenerate_facets(mesh);
    REQUIRE(degenerate_facets.empty());

    for (auto i : range(mesh.get_num_vertices())) {
        REQUIRE(mesh.count_num_corners_around_vertex(i) > 0);
    }
}

template <typename Scalar, typename Index>
void check_UV(SurfaceMesh<Scalar, Index>& mesh)
{
    auto uv_mesh = uv_mesh_view(mesh);
    uv_mesh.initialize_edges();
    check_degeneracy(uv_mesh);
}

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
template <typename MeshType>
void validate_primitive(MeshType& mesh, int num_boundaries = 0)
{
    mesh.initialize_topology();
    REQUIRE(mesh.is_vertex_manifold());
    REQUIRE(mesh.is_edge_manifold());

    mesh.initialize_components();
    REQUIRE(mesh.get_num_components() == 1);

    // Fix geometry to remove boundary loops
    const auto bd_loops = lagrange::extract_boundary_loops(mesh);
    REQUIRE(safe_cast<int>(bd_loops.size()) == num_boundaries);
}

template <typename MeshType>
void check_degeneracy(MeshType& mesh)
{
    lagrange::detect_degenerate_triangles(mesh);
    REQUIRE(mesh.has_facet_attribute("is_degenerate"));
    REQUIRE(mesh.get_facet_attribute("is_degenerate").maxCoeff() == Catch::Approx(0.0));

    if (!mesh.is_connectivity_initialized()) {
        mesh.initialize_connectivity();
    }
    for (auto i : range(mesh.get_num_vertices())) {
        const auto adj_facets = mesh.get_facets_adjacent_to_vertex(i);
        REQUIRE(adj_facets.size() > 0);
    }
}

template <typename MeshType>
void check_UV(MeshType& mesh)
{
    REQUIRE(mesh.is_uv_initialized());
    auto uv_mesh = mesh.get_uv_mesh();
    lagrange::detect_degenerate_triangles(*uv_mesh);
    REQUIRE(uv_mesh->has_facet_attribute("is_degenerate"));
    REQUIRE(uv_mesh->get_facet_attribute("is_degenerate").maxCoeff() == Catch::Approx(0.0));

    const auto& uvs = uv_mesh->get_vertices();
    const auto& uv_indices = uv_mesh->get_facets();
    ExactPredicatesShewchuk predicates;
    for (const auto f : uv_indices.rowwise()) {
        Eigen::Matrix<double, 1, 2> p0 = uvs.row(f[0]).template cast<double>();
        Eigen::Matrix<double, 1, 2> p1 = uvs.row(f[1]).template cast<double>();
        Eigen::Matrix<double, 1, 2> p2 = uvs.row(f[2]).template cast<double>();

        REQUIRE(predicates.orient2D(p0.data(), p1.data(), p2.data()) == 1);
    }
}

template <typename MeshType>
void check_semantic_labels(const MeshType& mesh)
{
    REQUIRE(mesh.has_facet_attribute("semantic_label"));

    const auto& labels = mesh.get_facet_attribute("semantic_label");
    const auto num_facets = mesh.get_num_facets();
    for (auto i : range(num_facets)) {
        const auto l = safe_cast_enum<PrimitiveSemanticLabel>(labels(i, 0));
        REQUIRE(l != PrimitiveSemanticLabel::UNKNOWN);
    }
}
#endif

} // namespace primitive_test_utils
} // namespace lagrange
