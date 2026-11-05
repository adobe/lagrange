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
#include <lagrange/Attribute.h>
#include <lagrange/sampling/sample_epsilon.h>
#include <lagrange/testing/common.h>
#include <lagrange/views.h>

#include <catch2/catch_approx.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace {

/// Build a unit-square grid mesh in the z = 0 plane with n subdivisions per side.
template <typename Scalar, typename Index>
lagrange::SurfaceMesh<Scalar, Index> make_grid(Index n)
{
    lagrange::SurfaceMesh<Scalar, Index> mesh;
    for (Index i = 0; i <= n; ++i) {
        for (Index j = 0; j <= n; ++j) {
            mesh.add_vertex({Scalar(j) / Scalar(n), Scalar(i) / Scalar(n), Scalar(0)});
        }
    }
    auto vid = [&](Index i, Index j) { return i * (n + 1) + j; };
    for (Index i = 0; i < n; ++i) {
        for (Index j = 0; j < n; ++j) {
            mesh.add_triangle(vid(i, j), vid(i, j + 1), vid(i + 1, j + 1));
            mesh.add_triangle(vid(i, j), vid(i + 1, j + 1), vid(i + 1, j));
        }
    }
    return mesh;
}

/// Set a constant per-vertex "lfs" scalar attribute.
template <typename Scalar, typename Index>
void set_constant_lfs(lagrange::SurfaceMesh<Scalar, Index>& mesh, Scalar value)
{
    auto id = mesh.template create_attribute<Scalar>("lfs", lagrange::AttributeElement::Vertex);
    auto data = mesh.template ref_attribute<Scalar>(id).ref_all();
    for (auto& x : data) x = value;
}

/// Squared distance from a query point to the nearest sample (brute force, dim == 3).
template <typename Scalar>
Scalar nearest_squared(const std::vector<Scalar>& samples, size_t skip, const Scalar* q)
{
    Scalar best = std::numeric_limits<Scalar>::max();
    const size_t n = samples.size() / 3;
    for (size_t i = 0; i < n; ++i) {
        if (i == skip) continue;
        const Scalar dx = samples[i * 3 + 0] - q[0];
        const Scalar dy = samples[i * 3 + 1] - q[1];
        const Scalar dz = samples[i * 3 + 2] - q[2];
        best = std::min(best, dx * dx + dy * dy + dz * dz);
    }
    return best;
}

} // namespace

TEST_CASE("sample_epsilon", "[sampling][epsilon]")
{
    using Scalar = double;
    using Index = uint32_t;
    namespace ls = lagrange::sampling;

    const Index n = 20;
    auto mesh = make_grid<Scalar, Index>(n);
    const Scalar lfs_value = 0.1;
    set_constant_lfs(mesh, lfs_value);
    const Scalar epsilon = 1.0;
    const Scalar radius = epsilon * lfs_value;

    SECTION("coverage")
    {
        ls::EpsilonSamplingOptions options;
        options.seed = 7;
        options.ensure_vertex_coverage = true;
        auto result = ls::sample_epsilon(mesh, "lfs", epsilon, options);

        REQUIRE(!result.facet_indices.empty());
        REQUIRE(result.samples.size() == result.facet_indices.size() * 3);

        // Every mesh vertex must have a sample within epsilon * lfs.
        auto vertices = vertex_view(mesh);
        const Scalar tol = radius * 1e-6;
        for (Index v = 0; v < mesh.get_num_vertices(); ++v) {
            const Scalar q[3] = {vertices(v, 0), vertices(v, 1), vertices(v, 2)};
            const Scalar d = std::sqrt(nearest_squared(result.samples, size_t(-1), q));
            REQUIRE(d <= radius + tol);
        }

        // Every reported facet id is valid.
        for (auto f : result.facet_indices) {
            REQUIRE(f < mesh.get_num_facets());
        }
    }

    SECTION("separation")
    {
        ls::EpsilonSamplingOptions options;
        options.seed = 3;
        auto result = ls::sample_epsilon(mesh, "lfs", epsilon, options);
        // Dart throwing rejects darts within `radius` of an existing sample, so accepted samples
        // are mutually separated by at least `radius`.
        const size_t num = result.facet_indices.size();
        for (size_t i = 0; i < num; ++i) {
            const Scalar q[3] = {
                result.samples[i * 3 + 0],
                result.samples[i * 3 + 1],
                result.samples[i * 3 + 2]};
            const Scalar d = std::sqrt(nearest_squared(result.samples, i, q));
            REQUIRE(d >= radius * (1 - 1e-6));
        }
    }

    SECTION("barycentric reconstruction")
    {
        ls::EpsilonSamplingOptions options;
        options.seed = 11;
        options.compute_barycentric = true;
        auto result = ls::sample_epsilon(mesh, "lfs", epsilon, options);

        REQUIRE(result.barycentric.size() == result.facet_indices.size() * 3);
        auto vertices = vertex_view(mesh);
        auto facets = facet_view(mesh);
        for (size_t i = 0; i < result.facet_indices.size(); ++i) {
            const Index f = result.facet_indices[i];
            const Scalar b0 = result.barycentric[i * 3 + 0];
            const Scalar b1 = result.barycentric[i * 3 + 1];
            const Scalar b2 = result.barycentric[i * 3 + 2];
            REQUIRE(b0 + b1 + b2 == Catch::Approx(1.0));
            const Index v0 = facets(f, 0);
            const Index v1 = facets(f, 1);
            const Index v2 = facets(f, 2);
            for (int d = 0; d < 3; ++d) {
                const Scalar expected =
                    b0 * vertices(v0, d) + b1 * vertices(v1, d) + b2 * vertices(v2, d);
                REQUIRE(result.samples[i * 3 + d] == Catch::Approx(expected));
            }
        }
    }

    SECTION("determinism")
    {
        ls::EpsilonSamplingOptions options;
        options.seed = 42;
        auto a = ls::sample_epsilon(mesh, "lfs", epsilon, options);
        auto b = ls::sample_epsilon(mesh, "lfs", epsilon, options);
        REQUIRE(a.samples == b.samples);
        REQUIRE(a.facet_indices == b.facet_indices);
    }

    SECTION("density grading")
    {
        ls::EpsilonSamplingOptions options;
        options.seed = 5;

        auto dense_mesh = make_grid<Scalar, Index>(n);
        set_constant_lfs(dense_mesh, Scalar(0.05));
        auto dense = ls::sample_epsilon(dense_mesh, "lfs", epsilon, options);

        auto sparse_mesh = make_grid<Scalar, Index>(n);
        set_constant_lfs(sparse_mesh, Scalar(0.15));
        auto sparse = ls::sample_epsilon(sparse_mesh, "lfs", epsilon, options);

        // Smaller feature size => smaller radius => more samples.
        REQUIRE(dense.facet_indices.size() > sparse.facet_indices.size());
    }

    SECTION("invalid inputs throw")
    {
        ls::EpsilonSamplingOptions options;
        LA_REQUIRE_THROWS(ls::sample_epsilon(mesh, "does_not_exist", epsilon, options));
        LA_REQUIRE_THROWS(ls::sample_epsilon(mesh, "lfs", Scalar(0), options));
        LA_REQUIRE_THROWS(ls::sample_epsilon(mesh, "lfs", Scalar(-1), options));

        auto bad_mesh = make_grid<Scalar, Index>(Index(2));
        set_constant_lfs(bad_mesh, lfs_value);
        bad_mesh.template ref_attribute<Scalar>("lfs").ref_all()[0] = Scalar(0);
        LA_REQUIRE_THROWS(ls::sample_epsilon(bad_mesh, "lfs", epsilon, options));

        ls::EpsilonSamplingOptions zero_attempts_options;
        zero_attempts_options.max_failed_attempts = 0;
        LA_REQUIRE_THROWS(ls::sample_epsilon(mesh, "lfs", epsilon, zero_attempts_options));

        lagrange::SurfaceMesh<Scalar, Index> quad_mesh;
        quad_mesh.add_vertex({Scalar(0), Scalar(0), Scalar(0)});
        quad_mesh.add_vertex({Scalar(1), Scalar(0), Scalar(0)});
        quad_mesh.add_vertex({Scalar(1), Scalar(1), Scalar(0)});
        quad_mesh.add_vertex({Scalar(0), Scalar(1), Scalar(0)});
        quad_mesh.add_quad(0, 1, 2, 3);
        set_constant_lfs(quad_mesh, lfs_value);
        LA_REQUIRE_THROWS(ls::sample_epsilon(quad_mesh, "lfs", epsilon, options));
    }
}
