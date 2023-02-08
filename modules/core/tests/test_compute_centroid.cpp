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
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <lagrange/compute_centroid.h>
#include <lagrange/views.h>

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
    #include <lagrange/compute_mesh_centroid.h>
    #include <lagrange/mesh_convert.h>
#endif

#include <limits>

#include <Eigen/Core>
#include <Eigen/Geometry>

TEST_CASE("compute_mesh_centroid", "[core][surface]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    constexpr Scalar a = 0.5;
    constexpr Scalar b = 2.0;
    constexpr Scalar large_number = 1e3;

    SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertex({-a / 2, -b / 2, 0}); // 0
    mesh.add_vertex({a / 2, -b / 2, 0}); // 1
    mesh.add_vertex({a / 2, 0, 0}); // 2
    mesh.add_vertex({a / 2, b / 4, 0}); // 3
    mesh.add_vertex({a / 2, b / 2, 0}); // 4
    mesh.add_vertex({-a / 2, b / 2, 0}); // 5
    mesh.add_vertex({large_number, large_number, large_number}); // 6
    mesh.add_vertex({-large_number, -large_number, -large_number}); // 7
    mesh.add_vertex({2 * large_number, 2 * large_number, 2 * large_number}); // 8

    mesh.add_triangle(0, 1, 2);
    mesh.add_triangle(6, 7, 8); // facet with 0 area.
    mesh.add_triangle(0, 2, 3);
    mesh.add_triangle(0, 3, 4);
    mesh.add_triangle(0, 4, 5);

    constexpr Scalar eps = std::numeric_limits<Scalar>::epsilon();

    MeshCentroidOptions options;
    options.weighting_type = MeshCentroidOptions::Area;
    std::array<Scalar, 3> centroid;

    SECTION("Uniform")
    {
        options.weighting_type = MeshCentroidOptions::Uniform;
        compute_mesh_centroid<Scalar, Index>(mesh, centroid, options);
        REQUIRE(centroid[0] > 100);
        REQUIRE(centroid[1] > 100);
        REQUIRE(centroid[2] > 100);
    }
    SECTION("No transformation")
    {
        compute_mesh_centroid<Scalar, Index>(mesh, centroid, options);
        REQUIRE_THAT(centroid[0], Catch::Matchers::WithinAbs(0, eps));
        REQUIRE_THAT(centroid[1], Catch::Matchers::WithinAbs(0, eps));
        REQUIRE_THAT(centroid[2], Catch::Matchers::WithinAbs(0, eps));
    }

    SECTION("With transformation")
    {
        using Matrix = Eigen::Matrix<Scalar, 3, 3>;
        using Vector = Eigen::Matrix<Scalar, 3, 1>;
        // Rigid transform the mesh.
        Vector tr(-1, 3, 4);
        Matrix rot =
            Eigen::AngleAxis<Scalar>(1.2365, Vector(-1, 2, 5.1).normalized()).toRotationMatrix();

        auto vertices = vertex_ref(mesh);
        Eigen::Matrix<Scalar, -1, -1, Eigen::RowMajor> new_vertices =
            ((vertices * rot.transpose()).rowwise() + tr.transpose()).eval();
        vertices = new_vertices;

        compute_mesh_centroid<Scalar, Index>(mesh, centroid, options);
        REQUIRE_THAT(centroid[0], Catch::Matchers::WithinAbs(tr[0], eps));
        REQUIRE_THAT(centroid[1], Catch::Matchers::WithinAbs(tr[1], eps));
        REQUIRE_THAT(centroid[2], Catch::Matchers::WithinAbs(tr[2], eps));
    }
}

TEST_CASE("compute_mesh_centroid benchmark", "[core][surface][centroid][!benchmark]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    MeshCentroidOptions options;

    auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/dragon.obj");

    BENCHMARK_ADVANCED("compute_mesh_centroid")(Catch::Benchmark::Chronometer meter)
    {
        if (mesh.has_attribute(options.facet_area_attribute_name))
            mesh.delete_attribute(options.facet_area_attribute_name, AttributeDeletePolicy::Force);
        if (mesh.has_attribute(options.facet_centroid_attribute_name))
            mesh.delete_attribute(
                options.facet_centroid_attribute_name,
                AttributeDeletePolicy::Force);
        std::vector<Scalar> centroid(3);
        meter.measure(
            [&]() { return compute_mesh_centroid<Scalar, Index>(mesh, centroid, options); });
    };

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
    using MeshType = TriangleMesh3D;
    auto legacy_mesh = to_legacy_mesh<MeshType>(mesh);

    BENCHMARK_ADVANCED("legacy::compute_mesh_centroid")(Catch::Benchmark::Chronometer meter)
    {
        if (legacy_mesh->has_facet_attribute("area")) legacy_mesh->remove_facet_attribute("area");
        meter.measure([&]() { return compute_mesh_centroid(*legacy_mesh); });
    };
#endif
}
