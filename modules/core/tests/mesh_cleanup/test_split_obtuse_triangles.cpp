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
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <lagrange/Attribute.h>
#include <lagrange/SurfaceMesh.h>
#include <lagrange/compute_area.h>
#include <lagrange/internal/internal_angles.h>
#include <lagrange/mesh_cleanup/split_obtuse_triangles.h>
#include <lagrange/views.h>

#include <cmath>

namespace {

template <typename Scalar, typename Index>
Scalar max_interior_angle(const lagrange::SurfaceMesh<Scalar, Index>& mesh)
{
    Eigen::Matrix<Scalar, Eigen::Dynamic, 3> angles;
    lagrange::internal::internal_angles(
        lagrange::vertex_view(mesh),
        lagrange::facet_view(mesh),
        angles);
    return angles.maxCoeff();
}

} // namespace

TEST_CASE("split_obtuse_triangles", "[surface][cleanup]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;
    const Scalar pi = static_cast<Scalar>(lagrange::internal::pi);
    constexpr Scalar eps = 1e-5;

    SECTION("No obtuse triangles -> no-op")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0.5, std::sqrt(Scalar(3)) / 2, 0});
        mesh.add_triangle(0, 1, 2);

        const auto num_v = mesh.get_num_vertices();
        const auto num_f = mesh.get_num_facets();

        SplitObtuseTrianglesOptions opts;
        size_t n = split_obtuse_triangles(mesh, opts);
        REQUIRE(n == 0);
        REQUIRE(mesh.get_num_vertices() == num_v);
        REQUIRE(mesh.get_num_facets() == num_f);
    }

    SECTION("Single obtuse sliver")
    {
        SurfaceMesh<Scalar, Index> mesh;
        // Very flat triangle: the apex at the top is obtuse (> 90 deg).
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0.5, 0.01, 0});
        mesh.add_triangle(0, 1, 2);

        SplitObtuseTrianglesOptions opts;
        opts.max_iterations = 1;
        size_t n = split_obtuse_triangles(mesh, opts);
        REQUIRE(n == 1);
        REQUIRE(mesh.get_num_vertices() == 4);
        REQUIRE(mesh.get_num_facets() == 2);
        REQUIRE_THAT(compute_mesh_area(mesh), Catch::Matchers::WithinAbs(0.005, 1e-9));
    }

    SECTION("Recursive convergence")
    {
        // T1 = (0,1,2) is initially acute, but splitting the shared edge to remove the highly
        // obtuse T2 introduces a new obtuse triangle inside T1's tessellation that requires
        // additional iterations to clean up.
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({10, 0, 0});
        mesh.add_vertex({2, 5, 0});
        mesh.add_vertex({5, -0.1, 0});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(1, 0, 3);

        const Scalar total_area = compute_mesh_area(mesh);

        SplitObtuseTrianglesOptions opts;
        opts.max_iterations = 0; // iterate to convergence
        opts.max_angle = static_cast<float>(pi / 2);
        size_t n = split_obtuse_triangles(mesh, opts);
        REQUIRE(n > 0);

        const Scalar max_a = max_interior_angle(mesh);
        REQUIRE(max_a <= static_cast<Scalar>(opts.max_angle) + eps);

        REQUIRE_THAT(compute_mesh_area(mesh), Catch::Matchers::WithinAbs(total_area, 1e-9));
    }

    SECTION("Shared obtuse edge across two triangles")
    {
        SurfaceMesh<Scalar, Index> mesh;
        // Both triangles share edge (0,1). The vertices at (0.5, +/- 0.05) make the apex obtuse on
        // both sides, so the shared edge is the obtuse edge for both.
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0.5, 0.05, 0});
        mesh.add_vertex({0.5, -0.05, 0});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(1, 0, 3);

        SplitObtuseTrianglesOptions opts;
        opts.max_iterations = 1;
        size_t n = split_obtuse_triangles(mesh, opts);
        // Both triangles split via a single new vertex on the shared edge.
        REQUIRE(n == 2);
        REQUIRE(mesh.get_num_vertices() == 5);
        REQUIRE(mesh.get_num_facets() == 4);
        REQUIRE_THAT(compute_mesh_area(mesh), Catch::Matchers::WithinAbs(0.05, 1e-9));
    }

    SECTION("Active region mask")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0.5, 0.01, 0});
        mesh.add_vertex({2, 0, 0});
        mesh.add_vertex({1.5, 0.01, 0});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(1, 3, 4);

        // Only facet 0 is active.
        uint8_t active_buffer[2] = {1, 0};
        mesh.create_attribute<uint8_t>(
            "active",
            AttributeElement::Facet,
            AttributeUsage::Scalar,
            1,
            active_buffer);

        SplitObtuseTrianglesOptions opts;
        opts.max_iterations = 1;
        opts.active_region_attribute = "active";
        size_t n = split_obtuse_triangles(mesh, opts);

        // The inactive obtuse facet remains intact; only the active one is split.
        REQUIRE(n == 1);
        REQUIRE(mesh.get_num_facets() == 3);
        REQUIRE(mesh.has_attribute("active"));
    }

    SECTION("Vertex attribute interpolation")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0.5, 0.01, 0});
        mesh.add_triangle(0, 1, 2);

        // Linear vertex attribute aligned with x-coordinate.
        Scalar values[3] = {0.0, 10.0, 5.0};
        mesh.create_attribute<Scalar>(
            "scalar_attr",
            AttributeElement::Vertex,
            AttributeUsage::Scalar,
            1,
            values);

        SplitObtuseTrianglesOptions opts;
        opts.max_iterations = 1;
        size_t n = split_obtuse_triangles(mesh, opts);
        REQUIRE(n == 1);
        REQUIRE(mesh.get_num_vertices() == 4);

        auto attr_view = attribute_vector_view<Scalar>(mesh, "scalar_attr");
        const Scalar new_val = attr_view[3];
        // The new vertex is the projection of v2=(0.5, 0.01, 0) onto edge v0->v1 -> (0.5, 0, 0).
        // Linear interpolation of {0, 10} at t=0.5 -> 5.
        REQUIRE_THAT(new_val, Catch::Matchers::WithinAbs(5.0, 1e-9));
    }

    SECTION("Collinear degenerate stack terminates")
    {
        // Stack of triangles whose three vertices are exactly collinear. The "obtuse" vertex
        // lies on the opposite edge, so naive splitting would never make progress. The
        // algorithm must detect this and terminate even with unbounded iterations.
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0.25, 0, 0});
        mesh.add_vertex({0.5, 0, 0});
        mesh.add_vertex({0.75, 0, 0});
        // Each triangle has 3 collinear vertices on the x-axis.
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(0, 1, 3);
        mesh.add_triangle(0, 1, 4);

        const auto num_v = mesh.get_num_vertices();
        const auto num_f = mesh.get_num_facets();

        SplitObtuseTrianglesOptions opts;
        opts.max_iterations = 0; // iterate to convergence; must not hang
        size_t n = split_obtuse_triangles(mesh, opts);
        REQUIRE(n == 0);
        REQUIRE(mesh.get_num_vertices() == num_v);
        REQUIRE(mesh.get_num_facets() == num_f);
    }

    SECTION("max_iterations early stop")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({10, 0, 0});
        mesh.add_vertex({2, 5, 0});
        mesh.add_vertex({5, -0.1, 0});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(1, 0, 3);

        SplitObtuseTrianglesOptions opts;
        opts.max_iterations = 1;
        opts.max_angle = static_cast<float>(pi / 2);
        size_t n = split_obtuse_triangles(mesh, opts);
        REQUIRE(n > 0);
        // One iteration leaves residual obtuse triangles.
        REQUIRE(max_interior_angle(mesh) > pi / 2);
    }
}
