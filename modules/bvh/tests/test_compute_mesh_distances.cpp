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
#include <lagrange/bvh/compute_mesh_distances.h>

#include <lagrange/primitive/generate_sphere.h>
#include <lagrange/testing/common.h>
#include <lagrange/views.h>

#include <catch2/matchers/catch_matchers_floating_point.hpp>

namespace {

using namespace lagrange;
using Scalar = float;
using Index = uint32_t;

// Two axis-aligned unit squares at z=0 and z=d, each made of 2 triangles.
// Every vertex of sq_a is directly below a vertex of sq_b, so the
// distance from each vertex to the opposite plane is exactly d.
std::pair<SurfaceMesh<Scalar, Index>, SurfaceMesh<Scalar, Index>> make_parallel_squares(Scalar d)
{
    SurfaceMesh<Scalar, Index> sq_a;
    sq_a.add_vertex({0, 0, 0});
    sq_a.add_vertex({1, 0, 0});
    sq_a.add_vertex({1, 1, 0});
    sq_a.add_vertex({0, 1, 0});
    sq_a.add_triangle(0, 1, 2);
    sq_a.add_triangle(0, 2, 3);

    SurfaceMesh<Scalar, Index> sq_b;
    sq_b.add_vertex({0, 0, d});
    sq_b.add_vertex({1, 0, d});
    sq_b.add_vertex({1, 1, d});
    sq_b.add_vertex({0, 1, d});
    sq_b.add_triangle(0, 1, 2);
    sq_b.add_triangle(0, 2, 3);

    return {sq_a, sq_b};
}

// Concentric unit spheres at the origin with the given radii and tessellation density.
std::pair<SurfaceMesh<Scalar, Index>, SurfaceMesh<Scalar, Index>>
make_concentric_spheres(Scalar r1, Scalar r2, size_t num_sections = 64)
{
    primitive::SphereOptions opts;
    opts.triangulate = true;
    opts.num_longitude_sections = num_sections;
    opts.num_latitude_sections = num_sections;

    opts.radius = r1;
    auto sphere_a = primitive::generate_sphere<Scalar, Index>(opts);

    opts.radius = r2;
    auto sphere_b = primitive::generate_sphere<Scalar, Index>(opts);

    return {sphere_a, sphere_b};
}

} // namespace

// ---------------------------------------------------------------------------
// compute_mesh_distances
// ---------------------------------------------------------------------------

TEST_CASE("compute_mesh_distances", "[bvh][mesh_distances]")
{
    using namespace lagrange;

    SECTION("self: all distances are zero")
    {
        // Distance from every vertex of a mesh to the mesh itself must be 0.
        primitive::SphereOptions opts;
        opts.radius = 1.0f;
        opts.triangulate = true;
        opts.num_longitude_sections = 32;
        opts.num_latitude_sections = 32;
        auto mesh = primitive::generate_sphere<Scalar, Index>(opts);

        auto attr_id = bvh::compute_mesh_distances(mesh, mesh);
        auto dist = attribute_matrix_view<Scalar>(mesh, attr_id);

        for (Index vi = 0; vi < mesh.get_num_vertices(); ++vi) {
            REQUIRE_THAT(dist(vi, 0), Catch::Matchers::WithinAbs(0.0f, 1e-5f));
        }
    }

    SECTION("parallel squares: all distances are exactly d")
    {
        // Exact test: every vertex of sq_a is directly below a vertex of sq_b,
        // so the closest-point distance is exactly d = 1.5.
        const Scalar d = 1.5f;
        auto [sq_a, sq_b] = make_parallel_squares(d);

        auto attr_id = bvh::compute_mesh_distances(sq_a, sq_b);
        auto dist = attribute_matrix_view<Scalar>(sq_a, attr_id);

        for (Index vi = 0; vi < sq_a.get_num_vertices(); ++vi) {
            REQUIRE_THAT(dist(vi, 0), Catch::Matchers::WithinAbs(d, 1e-5f));
        }
    }

    SECTION("custom attribute name is used")
    {
        auto [sq_a, sq_b] = make_parallel_squares(1.0f);

        bvh::MeshDistancesOptions opts;
        opts.output_attribute_name = "my_distances";
        bvh::compute_mesh_distances(sq_a, sq_b, opts);

        REQUIRE(sq_a.has_attribute("my_distances"));
        REQUIRE(!sq_a.has_attribute("@distance_to_mesh"));
    }

    SECTION("empty source: attribute created with zero entries")
    {
        // Source has no vertices — the attribute should be created but empty.
        SurfaceMesh<Scalar, Index> empty_source;
        auto [_, sq_b] = make_parallel_squares(1.0f);

        auto attr_id = bvh::compute_mesh_distances(empty_source, sq_b);
        auto dist = attribute_vector_view<Scalar>(empty_source, attr_id);
        REQUIRE(empty_source.get_num_vertices() == 0);
        REQUIRE(dist.size() == 0);
    }

    SECTION("empty target: all source distances are zero")
    {
        // Target has no faces — the BVH tree is empty, so every source vertex
        // gets distance 0 (closest point undefined; implementation returns 0).
        auto [sq_a, _] = make_parallel_squares(1.0f);
        SurfaceMesh<Scalar, Index> empty_target;

        auto attr_id = bvh::compute_mesh_distances(sq_a, empty_target);
        auto dist = attribute_matrix_view<Scalar>(sq_a, attr_id);

        for (Index vi = 0; vi < sq_a.get_num_vertices(); ++vi) {
            REQUIRE_THAT(dist(vi, 0), Catch::Matchers::WithinAbs(0.0f, 1e-6f));
        }
    }

    SECTION("both empty: no crash, attribute created with zero entries")
    {
        SurfaceMesh<Scalar, Index> empty_source;
        SurfaceMesh<Scalar, Index> empty_target;

        auto attr_id = bvh::compute_mesh_distances(empty_source, empty_target);
        auto dist = attribute_vector_view<Scalar>(empty_source, attr_id);
        REQUIRE(empty_source.get_num_vertices() == 0);
        REQUIRE(dist.size() == 0);
    }
}

// ---------------------------------------------------------------------------
// compute_hausdorff
// ---------------------------------------------------------------------------

TEST_CASE("compute_hausdorff", "[bvh][mesh_distances]")
{
    using namespace lagrange;

    SECTION("self: Hausdorff distance is zero")
    {
        primitive::SphereOptions opts;
        opts.radius = 1.0f;
        opts.triangulate = true;
        opts.num_longitude_sections = 32;
        opts.num_latitude_sections = 32;
        auto mesh = primitive::generate_sphere<Scalar, Index>(opts);

        Scalar h = bvh::compute_hausdorff(mesh, mesh);
        REQUIRE_THAT(h, Catch::Matchers::WithinAbs(0.0f, 1e-5f));
    }

    SECTION("parallel squares: Hausdorff = d (exact)")
    {
        // All 4 vertices of sq_a are at distance d from sq_b and vice versa,
        // so the symmetric Hausdorff distance equals d exactly.
        const Scalar d = 2.0f;
        auto [sq_a, sq_b] = make_parallel_squares(d);

        Scalar h = bvh::compute_hausdorff(sq_a, sq_b);
        REQUIRE_THAT(h, Catch::Matchers::WithinAbs(d, 1e-5f));
    }

    SECTION("concentric spheres: Hausdorff ≈ r2 - r1")
    {
        // Analytical value: every point on the inner sphere is at distance
        // r2 - r1 from the outer sphere (and vice versa).  The tessellated
        // mesh approximates this; 64×64 sections give <0.1 % error.
        const Scalar r1 = 1.0f, r2 = 2.0f;
        const Scalar expected = r2 - r1; // = 1.0

        auto [sphere_a, sphere_b] = make_concentric_spheres(r1, r2);

        Scalar h = bvh::compute_hausdorff(sphere_a, sphere_b);
        REQUIRE_THAT(h, Catch::Matchers::WithinRel(expected, 0.02f)); // 2 % tolerance
    }

    SECTION("symmetry: Hausdorff(A,B) == Hausdorff(B,A)")
    {
        auto [sphere_a, sphere_b] = make_concentric_spheres(1.0f, 1.5f);

        Scalar h_ab = bvh::compute_hausdorff(sphere_a, sphere_b);
        Scalar h_ba = bvh::compute_hausdorff(sphere_b, sphere_a);
        REQUIRE_THAT(h_ab, Catch::Matchers::WithinAbs(h_ba, 1e-5f));
    }

    SECTION("empty source: Hausdorff is zero")
    {
        // No forward distances (empty source) and the backward tree is empty
        // (source has no faces), so both directed distances are 0.
        SurfaceMesh<Scalar, Index> empty_source;
        auto [sq_b, _] = make_parallel_squares(1.0f);

        Scalar h = bvh::compute_hausdorff(empty_source, sq_b);
        REQUIRE_THAT(h, Catch::Matchers::WithinAbs(0.0f, 1e-6f));
    }

    SECTION("empty target: Hausdorff is zero")
    {
        // Forward tree is empty → all source distances are 0.
        // No backward distances (empty target).
        auto [sq_a, _] = make_parallel_squares(1.0f);
        SurfaceMesh<Scalar, Index> empty_target;

        Scalar h = bvh::compute_hausdorff(sq_a, empty_target);
        REQUIRE_THAT(h, Catch::Matchers::WithinAbs(0.0f, 1e-6f));
    }

    SECTION("both empty: Hausdorff is zero")
    {
        SurfaceMesh<Scalar, Index> empty_a;
        SurfaceMesh<Scalar, Index> empty_b;

        Scalar h = bvh::compute_hausdorff(empty_a, empty_b);
        REQUIRE_THAT(h, Catch::Matchers::WithinAbs(0.0f, 1e-6f));
    }
}

// ---------------------------------------------------------------------------
// compute_chamfer
// ---------------------------------------------------------------------------

TEST_CASE("compute_chamfer", "[bvh][mesh_distances]")
{
    using namespace lagrange;

    SECTION("self: Chamfer distance is zero")
    {
        primitive::SphereOptions opts;
        opts.radius = 1.0f;
        opts.triangulate = true;
        opts.num_longitude_sections = 32;
        opts.num_latitude_sections = 32;
        auto mesh = primitive::generate_sphere<Scalar, Index>(opts);

        Scalar c = bvh::compute_chamfer(mesh, mesh);
        REQUIRE_THAT(c, Catch::Matchers::WithinAbs(0.0f, 1e-5f));
    }

    SECTION("parallel squares: Chamfer = 2 * d^2 (exact)")
    {
        // Every vertex of sq_a is at distance d from sq_b, and every vertex
        // of sq_b is at distance d from sq_a.
        //   Chamfer = (1/4)*4*d^2  +  (1/4)*4*d^2  =  2*d^2.
        const Scalar d = 1.5f;
        auto [sq_a, sq_b] = make_parallel_squares(d);

        Scalar c = bvh::compute_chamfer(sq_a, sq_b);
        REQUIRE_THAT(c, Catch::Matchers::WithinAbs(2.0f * d * d, 1e-4f));
    }

    SECTION("concentric spheres: Chamfer ≈ 2 * (r2 - r1)^2")
    {
        // Analytical value: every vertex contributes (r2-r1)^2 to the
        // mean squared distance, giving Chamfer = 2*(r2-r1)^2.
        const Scalar r1 = 1.0f, r2 = 2.0f;
        const Scalar expected = 2.0f * (r2 - r1) * (r2 - r1); // = 2.0

        auto [sphere_a, sphere_b] = make_concentric_spheres(r1, r2);

        Scalar c = bvh::compute_chamfer(sphere_a, sphere_b);
        REQUIRE_THAT(c, Catch::Matchers::WithinRel(expected, 0.02f)); // 2 % tolerance
    }

    SECTION("symmetry: Chamfer(A,B) == Chamfer(B,A)")
    {
        auto [sphere_a, sphere_b] = make_concentric_spheres(1.0f, 1.5f);

        Scalar c_ab = bvh::compute_chamfer(sphere_a, sphere_b);
        Scalar c_ba = bvh::compute_chamfer(sphere_b, sphere_a);
        REQUIRE_THAT(c_ab, Catch::Matchers::WithinAbs(c_ba, 1e-4f));
    }

    SECTION("empty source: Chamfer is zero")
    {
        // No forward distances (empty source), backward tree is empty
        // (source has no faces) → both sums are 0.
        SurfaceMesh<Scalar, Index> empty_source;
        auto [sq_b, _] = make_parallel_squares(1.0f);

        Scalar c = bvh::compute_chamfer(empty_source, sq_b);
        REQUIRE_THAT(c, Catch::Matchers::WithinAbs(0.0f, 1e-6f));
    }

    SECTION("empty target: Chamfer is zero")
    {
        // Forward tree is empty → all source distances are 0 → forward sum is 0.
        // No backward distances (empty target).
        auto [sq_a, _] = make_parallel_squares(1.0f);
        SurfaceMesh<Scalar, Index> empty_target;

        Scalar c = bvh::compute_chamfer(sq_a, empty_target);
        REQUIRE_THAT(c, Catch::Matchers::WithinAbs(0.0f, 1e-6f));
    }

    SECTION("both empty: Chamfer is zero")
    {
        SurfaceMesh<Scalar, Index> empty_a;
        SurfaceMesh<Scalar, Index> empty_b;

        Scalar c = bvh::compute_chamfer(empty_a, empty_b);
        REQUIRE_THAT(c, Catch::Matchers::WithinAbs(0.0f, 1e-6f));
    }
}
