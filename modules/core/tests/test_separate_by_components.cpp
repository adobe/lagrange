/*
 * Copyright 2023 Adobe. All rights reserved.
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
#include <lagrange/testing/create_test_mesh.h>
#include <catch2/benchmark/catch_benchmark.hpp>

#include <lagrange/combine_meshes.h>
#include <lagrange/compute_facet_normal.h>
#include <lagrange/compute_vertex_normal.h>
#include <lagrange/mesh_convert.h>
#include <lagrange/separate_by_components.h>

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
    #include <lagrange/create_mesh.h>
    #include <lagrange/legacy/extract_submesh.h>
#endif


TEST_CASE("separate_by_components", "[core][utilities][component]")
{
    using Scalar = float;
    using Index = uint32_t;
    auto cube = lagrange::testing::create_test_cube<Scalar, Index>();
    lagrange::compute_facet_normal(cube);
    lagrange::compute_vertex_normal(cube);

    auto sphere = lagrange::testing::create_test_sphere<Scalar, Index>();
    lagrange::compute_facet_normal(sphere);
    lagrange::compute_vertex_normal(sphere);

    auto mesh = lagrange::combine_meshes({&cube, &sphere});

    auto components = lagrange::separate_by_components(mesh);
    REQUIRE(components.size() == 2);

    for (const auto& m : components) {
        if (m.get_num_vertices() == 8) {
            REQUIRE(m.get_num_facets() == 12);
        } else {
            REQUIRE(m.get_num_vertices() == sphere.get_num_vertices());
            REQUIRE(m.get_num_facets() == sphere.get_num_facets());
        }
    }
}

TEST_CASE("separate_by_components benchmark", "[core][utilities][component][!benchmark]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    auto dragon = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/dragon.obj");
    auto mesh = lagrange::combine_meshes({&dragon, &dragon});
    mesh.initialize_edges();
    lagrange::SeparateByComponentsOptions options;
    options.map_attributes = false;

    BENCHMARK("extract_components")
    {
        return lagrange::separate_by_components(mesh, options);
    };

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
    using MeshType = TriangleMesh3D;
    auto legacy_mesh = to_legacy_mesh<MeshType>(mesh);
    std::vector<std::vector<typename MeshType::Index>> vertex_mapping, facet_mapping;

    BENCHMARK("legacy::extract_component_submeshes")
    {
        return lagrange::legacy::extract_component_submeshes(
            *legacy_mesh,
            &vertex_mapping,
            &facet_mapping);
    };
#endif
}
