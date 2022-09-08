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
#include <lagrange/SurfaceMesh.h>
#include <lagrange/SurfaceMeshTypes.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <catch2/catch_test_macros.hpp>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <random>

namespace {

template <typename Scalar, typename Index>
void test_rng_vertices()
{
    std::mt19937 gen;
    std::uniform_real_distribution<Scalar> dist(0, 1);

    const Index num_pts = 100;

    std::vector<std::array<Scalar, 3>> vertices;
    lagrange::SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertices(
        Index(vertices.size()),
        {vertices.empty() ? nullptr : vertices[0].data(), 3 * vertices.size()});

    mesh.add_vertices(num_pts, [&gen, &dist](Index, lagrange::span<Scalar> p) noexcept {
        p[0] = dist(gen);
        p[1] = dist(gen);
        p[2] = dist(gen);
    });

    std::mt19937 gen2;
    for (Index i = 0; i < num_pts; ++i) {
        const auto v = mesh.get_position(i);
        for (Index k = 0; k < 3; ++k) {
            const Scalar x = dist(gen2);
            REQUIRE(v[k] == x);
        }
    }
}

} // namespace

TEST_CASE("SurfaceMesh: Random Vertices Lambda", "[next]")
{
#define LA_X_test_rng_vertices(_, S, I) test_rng_vertices<S, I>();
    LA_SURFACE_MESH_X(test_rng_vertices, 0)
}
