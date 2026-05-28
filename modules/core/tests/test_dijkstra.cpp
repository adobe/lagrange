/*
 * Copyright 2022 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */

#include <lagrange/testing/common.h>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_approx.hpp>

#include <lagrange/Logger.h>
#include <lagrange/fs/filesystem.h>
#include <lagrange/internal/dijkstra.h>
#include <lagrange/io/save_mesh_obj.h>
#include <lagrange/mesh_convert.h>
#include <lagrange/views.h>

#include <array>
#include <string>
#include <vector>

namespace {

template <typename Scalar, typename Index>
struct GridInfo
{
    lagrange::SurfaceMesh<Scalar, Index> mesh;
    Eigen::Vector3<Scalar> seed_pt;
    std::array<Index, 3> seeds{};
    std::array<Scalar, 3> seed_dist{};
};

/// Run dijkstra with Euclidean edge-length distances; returns per-vertex distances (infinity
/// for unreached).
template <typename Scalar, typename Index>
std::vector<Scalar> run_dijkstra(
    lagrange::SurfaceMesh<Scalar, Index>& mesh,
    lagrange::span<const Index> seeds,
    lagrange::span<const Scalar> seed_dists,
    const lagrange::internal::DijkstraOptions<Scalar>& opts)
{
    auto vertices = lagrange::vertex_view(mesh);
    auto dist_fn = [&](Index vi, Index vj) { return (vertices.row(vi) - vertices.row(vj)).norm(); };
    std::vector<Scalar> got(mesh.get_num_vertices(), std::numeric_limits<Scalar>::infinity());
    lagrange::internal::dijkstra<Scalar, Index>(
        mesh,
        seeds,
        seed_dists,
        opts,
        dist_fn,
        [&](Index vi, Scalar d) { got[vi] = d; });
    return got;
}

/// Build an `n x n` triangle grid with cell size `(hx, hy)`, seed at the middle facet's
/// centroid, and export the mesh for inspection.
template <typename Scalar, typename Index>
GridInfo<Scalar, Index> make_grid(Index n, Scalar hx, Scalar hy, const std::string& name)
{
    using namespace lagrange;
    GridInfo<Scalar, Index> info;
    auto& mesh = info.mesh;
    mesh = SurfaceMesh<Scalar, Index>(3);
    mesh.add_vertices(n * n);
    auto vertices = vertex_ref(mesh);
    for (Index j = 0; j < n; ++j) {
        for (Index i = 0; i < n; ++i) {
            vertices.row(j * n + i) << hx * static_cast<Scalar>(i), hy * static_cast<Scalar>(j),
                Scalar(0);
        }
    }
    mesh.add_triangles((n - 1) * (n - 1) * 2);
    auto facets = facet_ref(mesh);
    for (Index j = 0; j < n - 1; ++j) {
        for (Index i = 0; i < n - 1; ++i) {
            Index v00 = j * n + i;
            Index v10 = j * n + (i + 1);
            Index v01 = (j + 1) * n + i;
            Index v11 = (j + 1) * n + (i + 1);
            facets.row((j * (n - 1) + i) * 2 + 0) << v00, v10, v01;
            facets.row((j * (n - 1) + i) * 2 + 1) << v10, v11, v01;
        }
    }

    Index seed_facet = ((n - 1) / 2 * (n - 1) + (n - 1) / 2) * 2;
    auto fv = mesh.get_facet_vertices(seed_facet);
    info.seed_pt = Eigen::Vector3<Scalar>::Zero();
    for (int k = 0; k < 3; ++k) {
        info.seeds[k] = fv[k];
        info.seed_pt += Eigen::Vector3<Scalar>(vertices.row(fv[k]));
    }
    info.seed_pt /= Scalar(3);
    for (int k = 0; k < 3; ++k) {
        info.seed_dist[k] =
            (Eigen::Vector3<Scalar>(vertices.row(info.seeds[k])) - info.seed_pt).norm();
    }

    fs::path output_dir = lagrange::testing::get_test_output_dir() / "dijkstra";
    fs::create_directories(output_dir);
    fs::path output_path = output_dir / (name + ".obj");
    fs::ofstream output_stream(output_path);
    lagrange::io::save_mesh_obj(output_stream, mesh);
    lagrange::logger().info("Exported grid '{}' to {}", name, output_path.string());

    return info;
}

} // namespace

TEST_CASE("dijkstra geodesic radius", "[surface][internal][utility]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    auto check = [](SurfaceMesh<Scalar, Index>& mesh,
                    span<const Scalar> expected_dist,
                    Scalar radius,
                    size_t expected_reached) {
        Index seed = 0;
        Scalar seed_dist = 0;
        lagrange::internal::DijkstraOptions<Scalar> opts;
        opts.geodesic_radius = radius;
        auto got = run_dijkstra<Scalar, Index>(
            mesh,
            span<const Index>(&seed, 1),
            span<const Scalar>(&seed_dist, 1),
            opts);
        size_t num_reached = 0;
        for (Index vi = 0; vi < mesh.get_num_vertices(); ++vi) {
            if (!std::isfinite(got[vi])) continue;
            REQUIRE(got[vi] <= radius);
            REQUIRE(got[vi] == Catch::Approx(expected_dist[vi]));
            ++num_reached;
        }
        REQUIRE(num_reached == expected_reached);
    };

    SECTION("triangulated quad")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({1, 1, 0});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(2, 1, 3);
        std::array<Scalar, 4> expected{0, 1, 1, 2};
        check(mesh, span<const Scalar>(expected.data(), expected.size()), 0.1, 1);
        check(mesh, span<const Scalar>(expected.data(), expected.size()), 1.1, 3);
        check(mesh, span<const Scalar>(expected.data(), expected.size()), 2.1, 4);
    }

    SECTION("mixed quad and triangles")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({1, 1, 0});
        mesh.add_vertex({1, 0, 1});
        mesh.add_vertex({1, 1, 1});
        mesh.add_quad(0, 2, 3, 1);
        mesh.add_triangle(1, 3, 4);
        mesh.add_triangle(4, 3, 5);
        std::array<Scalar, 6> expected{0, 1, 1, 2, 2, 3};
        check(mesh, span<const Scalar>(expected.data(), expected.size()), 0.1, 1);
        check(mesh, span<const Scalar>(expected.data(), expected.size()), 1.1, 3);
        check(mesh, span<const Scalar>(expected.data(), expected.size()), 2.1, 5);
        check(mesh, span<const Scalar>(expected.data(), expected.size()), 3.1, 6);
    }
}

TEST_CASE("dijkstra euclidean radius on grid", "[surface][internal][utility]")
{
    // Sweep (geo_radius, euclidean_radius) on an (n x n) triangle grid (optionally Y-scaled)
    // and check the reached set against a brute-force ground truth.
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    auto run_grid = [](Index n, Scalar y_scale, const std::string& name) {
        INFO("grid n=" << n << " y_scale=" << y_scale);

        Scalar h = Scalar(1) / static_cast<Scalar>(n - 1);
        auto info = make_grid<Scalar, Index>(n, h, h * y_scale, name);
        auto& mesh = info.mesh;
        auto vertices = vertex_view(mesh);
        auto seeds = span<const Index>(info.seeds.data(), 3);
        auto seed_dists = span<const Scalar>(info.seed_dist.data(), 3);

        // Brute-force ground-truth graph distances from the seed point (no radius).
        auto truth_dist = run_dijkstra<Scalar, Index>(mesh, seeds, seed_dists, {});
        for (Index vi = 0; vi < n * n; ++vi) {
            REQUIRE(std::isfinite(truth_dist[vi]));
        }

        // `min_fallback_count` asserts that at least N vertices are reached only via the
        // Euclidean fallback (inside the Euclidean ball but outside the geodesic ball).
        auto check = [&](Scalar geo_radius, Scalar euclidean_radius, Index min_fallback_count = 0) {
            INFO("geo_radius=" << geo_radius << " euclidean_radius=" << euclidean_radius);
            lagrange::internal::DijkstraOptions<Scalar> opts;
            opts.geodesic_radius = geo_radius;
            opts.euclidean_radius = euclidean_radius;
            opts.seed_position = info.seed_pt;
            auto got_dist = run_dijkstra<Scalar, Index>(mesh, seeds, seed_dists, opts);

            Scalar effective_geo = geo_radius > 0 ? geo_radius : std::numeric_limits<Scalar>::max();
            bool has_eucl = euclidean_radius > 0;
            Scalar eucl_sq = euclidean_radius * euclidean_radius;
            auto is_seed = [&](Index vi) {
                return vi == info.seeds[0] || vi == info.seeds[1] || vi == info.seeds[2];
            };
            // Vertices in either ball are reached with the exact graph distance. Chord-bridge
            // extras outside both balls may also be reached, but only with an upper-bound
            // distance (the true shortest path may transit unbridged vertices).
            Index fallback_count = 0;
            for (Index vi = 0; vi < n * n; ++vi) {
                bool within_geo = truth_dist[vi] <= effective_geo;
                bool within_eucl =
                    has_eucl &&
                    (Eigen::Vector3<Scalar>(vertices.row(vi)) - info.seed_pt).squaredNorm() <=
                        eucl_sq;
                bool expected = is_seed(vi) || within_geo || within_eucl;
                bool reached = std::isfinite(got_dist[vi]);
                INFO(
                    "vertex " << vi << " expected=" << expected << " reached=" << reached
                              << " truth_dist=" << truth_dist[vi] << " got_dist=" << got_dist[vi]);
                if (expected) {
                    REQUIRE(reached);
                    REQUIRE(got_dist[vi] == Catch::Approx(truth_dist[vi]));
                } else if (reached) {
                    REQUIRE(got_dist[vi] >= truth_dist[vi] - Scalar(1e-9));
                }
                if (within_eucl && !within_geo && !is_seed(vi)) {
                    ++fallback_count;
                }
            }
            INFO(
                "fallback_count=" << fallback_count
                                  << " min_fallback_count=" << min_fallback_count);
            REQUIRE(fallback_count >= min_fallback_count);
        };

        check(Scalar(0.25), Scalar(0)); // geodesic-only, small radius
        check(Scalar(2.0), Scalar(0)); // geodesic-only, full mesh
        check(Scalar(0.001), Scalar(0.5)); // euclidean-only
        check(Scalar(0.2), Scalar(0.5)); // eucl extends geo
        check(Scalar(0.5), Scalar(0.1)); // geo dominates
    };

    run_grid(/*n=*/9, /*y_scale=*/Scalar(1), "grid_9_iso");
    run_grid(/*n=*/9, /*y_scale=*/Scalar(0.25), "grid_9_y025");
    run_grid(/*n=*/11, /*y_scale=*/Scalar(0.5), "grid_11_y05");
}

TEST_CASE("dijkstra euclidean chord bridge", "[surface][internal][utility]")
{
    // Two-triangle mesh where the only graph path from `v0` to `v3` transits `v2`/`v4` (both
    // outside both balls). Geodesic-only drops `v3`; the Euclidean variant recovers it via the
    // chord (v4, v3), whose closest point to the seed is `v3` itself.
    //
    //     v1 --- v2 ----- v3
    //      \    / \      /
    //       \  /   \    /
    //        v0     \  /
    //                v4
    //
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertex({Scalar(0.0), Scalar(0.0), Scalar(0.0)}); // v0 (seed)
    mesh.add_vertex({Scalar(0.5), Scalar(0.0), Scalar(0.0)}); // v1 (in scope)
    mesh.add_vertex({Scalar(3.0), Scalar(1.0), Scalar(0.0)}); // v2 (out of scope)
    mesh.add_vertex({Scalar(2.0), Scalar(0.0), Scalar(0.0)}); // v3 (target, in eucl ball)
    mesh.add_vertex({Scalar(3.0), Scalar(-1.0), Scalar(0.0)}); // v4 (out of scope)
    mesh.add_triangle(0, 1, 2);
    mesh.add_triangle(2, 4, 3);

    Index seed = 0;
    Scalar seed_dist = 0;
    auto seeds = span<const Index>(&seed, 1);
    auto seed_dists = span<const Scalar>(&seed_dist, 1);

    SECTION("geodesic-only leaves v3 unreachable")
    {
        lagrange::internal::DijkstraOptions<Scalar> opts;
        opts.geodesic_radius = Scalar(2.0);
        auto got = run_dijkstra<Scalar, Index>(mesh, seeds, seed_dists, opts);

        REQUIRE(std::isfinite(got[0]));
        REQUIRE(std::isfinite(got[1]));
        REQUIRE_FALSE(std::isfinite(got[2]));
        REQUIRE_FALSE(std::isfinite(got[3]));
        REQUIRE_FALSE(std::isfinite(got[4]));
    }

    SECTION("euclidean fallback recovers v3 via chord bridge")
    {
        lagrange::internal::DijkstraOptions<Scalar> opts;
        opts.geodesic_radius = Scalar(2.0);
        opts.euclidean_radius = Scalar(2.0);
        auto got = run_dijkstra<Scalar, Index>(mesh, seeds, seed_dists, opts);

        // All five vertices are reached: v0/v1 in scope, v2/v3/v4 as chord-bridge endpoints.
        for (Index vi = 0; vi < 5; ++vi) {
            INFO("vertex " << vi);
            REQUIRE(std::isfinite(got[vi]));
        }
    }

    SECTION("euclidean fallback with too-tight radius cannot reach v3")
    {
        // Shrink the Euclidean ball below the chord (v4, v3) so the bridge can no longer fire.
        lagrange::internal::DijkstraOptions<Scalar> opts;
        opts.geodesic_radius = Scalar(2.0);
        opts.euclidean_radius = Scalar(1.5);
        auto got = run_dijkstra<Scalar, Index>(mesh, seeds, seed_dists, opts);

        REQUIRE(std::isfinite(got[0]));
        REQUIRE(std::isfinite(got[1]));
        REQUIRE_FALSE(std::isfinite(got[3]));
    }
}

TEST_CASE("dijkstra chord bridge across visited edges", "[surface][internal][utility]")
{
    // Regression for an edge-visitation bug in the chord-bridge handler. Three triangles fan
    // around the seed `v0`; only the middle triangle's chord (vL, vR) bridges through the seed,
    // but its two edges incident to `v0` are shared with the flanking triangles. The facet
    // add-order is chosen so the bridging triangle is walked last around `v0`, so both shared
    // edges are already marked visited when the bridge fires. The handler must still enqueue
    // `vL` and `vR` despite the visited flag (the `!chord_bridge` guard in `try_enqueue`).
    //
    //   vAR(-3,3)              vAL(3,3)
    //         \                 /
    //          \   f_c   f_a  /
    //           \   /     \   /
    //         vR(-3,0)--f_b--vL(3,0)
    //               \        /
    //                \      /
    //                v0(0,0)
    //
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertex({Scalar(0), Scalar(0), Scalar(0)}); // v0 (seed)
    mesh.add_vertex({Scalar(3), Scalar(0), Scalar(0)}); // vL
    mesh.add_vertex({Scalar(-3), Scalar(0), Scalar(0)}); // vR
    mesh.add_vertex({Scalar(3), Scalar(3), Scalar(0)}); // vAL
    mesh.add_vertex({Scalar(-3), Scalar(3), Scalar(0)}); // vAR

    // Corners around v0 are linked LIFO during edge initialization, so the corner walk visits
    // facets in reverse add-order. Adding f_b first ensures it is walked last around v0.
    mesh.add_triangle(0, 1, 2); // f_b: bridging, chord (vL, vR) passes through origin
    mesh.add_triangle(0, 3, 1); // f_a: shares edge (v0, vL); chord (vAL, vL) does not bridge
    mesh.add_triangle(0, 2, 4); // f_c: shares edge (v0, vR); chord (vR, vAR) does not bridge

    Index seed = 0;
    Scalar seed_dist = 0;
    auto seeds = span<const Index>(&seed, 1);
    auto seed_dists = span<const Scalar>(&seed_dist, 1);

    // Geodesic ball is tiny so no neighbor is geodesically in scope; Euclidean ball excludes
    // every non-seed vertex (||vL|| = ||vR|| = 3, ||vAL|| = ||vAR|| = sqrt(18)). Only the
    // (vL, vR) chord bridges.
    lagrange::internal::DijkstraOptions<Scalar> opts;
    opts.geodesic_radius = Scalar(0.1);
    opts.euclidean_radius = Scalar(2);
    auto got = run_dijkstra<Scalar, Index>(mesh, seeds, seed_dists, opts);

    // With the fix, vL and vR are enqueued by the bridging facet despite the shared edges
    // being already marked visited; vAL and vAR are then reached as further chord-bridge
    // endpoints from vL and vR.
    for (Index vi = 0; vi < 5; ++vi) {
        INFO("vertex " << vi);
        REQUIRE(std::isfinite(got[vi]));
    }
}

TEST_CASE("dijkstra benchmark", "[surface][utility][internal][!benchmark]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/dragon.obj");
    auto vertices = vertex_view(mesh);
    auto dist = [&](Index vi, Index vj) { return (vertices.row(vi) - vertices.row(vj)).norm(); };

    BENCHMARK_ADVANCED("dijkstra")(Catch::Benchmark::Chronometer meter)
    {
        mesh.initialize_edges();
        size_t count = 0;
        Index sources[]{0};
        Scalar source_dist[]{0};
        auto process = [&](Index, Scalar) { count++; };

        meter.measure([&]() {
            lagrange::internal::dijkstra<Scalar, Index>(
                mesh,
                sources,
                source_dist,
                lagrange::internal::DijkstraOptions<Scalar>{},
                dist,
                process);
            return count;
        });
    };

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
    using MeshType = TriangleMesh3D;
    auto legacy_mesh = to_legacy_mesh<MeshType>(mesh);

    BENCHMARK_ADVANCED("dijkstra legacy")(Catch::Benchmark::Chronometer meter)
    {
        legacy_mesh->initialize_connectivity();
        size_t count = 0;
        auto process = [&](Index, Scalar) {
            count++;
            return false;
        };

        meter.measure([&]() {
            lagrange::internal::dijkstra<MeshType>(*legacy_mesh, {0}, {0}, 0, dist, process);
            return count;
        });
    };
#endif
}
