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
#include <lagrange/SurfaceMesh.h>
#include <lagrange/serialization/serialize_mesh.h>
#include <lagrange/testing/common.h>

#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("serialization2: dragon benchmark", "[serialization2][!benchmark]")
{
    using Scalar = double;
    using Index = uint32_t;

    auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/dragon.obj");
    INFO(
        "Dragon mesh: " << mesh.get_num_vertices() << " vertices, " << mesh.get_num_facets()
                        << " facets");

    // Pre-serialize buffers for deserialization benchmarks
    lagrange::serialization::SerializeOptions opts_compressed;

    lagrange::serialization::SerializeOptions opts_uncompressed;
    opts_uncompressed.compress = false;

    auto buf_compressed = lagrange::serialization::serialize_mesh(mesh, opts_compressed);
    auto buf_uncompressed = lagrange::serialization::serialize_mesh(mesh, opts_uncompressed);

    BENCHMARK("serialize (uncompressed)")
    {
        return lagrange::serialization::serialize_mesh(mesh, opts_uncompressed);
    };

    BENCHMARK("serialize (compressed, level 3)")
    {
        return lagrange::serialization::serialize_mesh(mesh, opts_compressed);
    };

    BENCHMARK("deserialize (uncompressed)")
    {
        return lagrange::serialization::deserialize_mesh<lagrange::SurfaceMesh<Scalar, Index>>(
            buf_uncompressed);
    };

    BENCHMARK("deserialize (compressed)")
    {
        return lagrange::serialization::deserialize_mesh<lagrange::SurfaceMesh<Scalar, Index>>(
            buf_compressed);
    };

    // Also benchmark with edge topology initialized
    mesh.initialize_edges();

    auto buf_edges_compressed = lagrange::serialization::serialize_mesh(mesh, opts_compressed);
    auto buf_edges_uncompressed = lagrange::serialization::serialize_mesh(mesh, opts_uncompressed);

    BENCHMARK("serialize with edges (uncompressed)")
    {
        return lagrange::serialization::serialize_mesh(mesh, opts_uncompressed);
    };

    BENCHMARK("serialize with edges (compressed)")
    {
        return lagrange::serialization::serialize_mesh(mesh, opts_compressed);
    };

    BENCHMARK("deserialize with edges (uncompressed)")
    {
        return lagrange::serialization::deserialize_mesh<lagrange::SurfaceMesh<Scalar, Index>>(
            buf_edges_uncompressed);
    };

    BENCHMARK("deserialize with edges (compressed)")
    {
        return lagrange::serialization::deserialize_mesh<lagrange::SurfaceMesh<Scalar, Index>>(
            buf_edges_compressed);
    };
}
