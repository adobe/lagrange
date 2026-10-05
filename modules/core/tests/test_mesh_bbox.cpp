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
#include <lagrange/mesh_bbox.h>
#include <lagrange/testing/common.h>

TEST_CASE("mesh_bbox 3D", "[core][mesh_bbox]")
{
    using Scalar = double;
    using Index = uint32_t;

    SECTION("empty mesh")
    {
        lagrange::SurfaceMesh<Scalar, Index> mesh(3);
        auto bbox = lagrange::mesh_bbox<3>(mesh);
        REQUIRE(bbox.isEmpty());
    }

    SECTION("single vertex")
    {
        lagrange::SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({1, 2, 3});
        auto bbox = lagrange::mesh_bbox<3>(mesh);
        REQUIRE(bbox.min().x() == 1);
        REQUIRE(bbox.min().y() == 2);
        REQUIRE(bbox.min().z() == 3);
        REQUIRE(bbox.max() == bbox.min());
    }

    SECTION("triangle")
    {
        lagrange::SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_triangle(0, 1, 2);
        auto bbox = lagrange::mesh_bbox<3>(mesh);
        REQUIRE(bbox.min().x() == 0);
        REQUIRE(bbox.min().y() == 0);
        REQUIRE(bbox.min().z() == 0);
        REQUIRE(bbox.max().x() == 1);
        REQUIRE(bbox.max().y() == 1);
        REQUIRE(bbox.max().z() == 0);
    }

    SECTION("transformed triangle")
    {
        lagrange::SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({2, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_triangle(0, 1, 2);

        const Eigen::Affine3d transform =
            Eigen::Translation3d(1, 2, 3) *
            Eigen::AngleAxisd(0.7853981633974483, Eigen::Vector3d::UnitZ());
        const auto bbox = lagrange::mesh_bbox<3>(mesh, transform);
        const Eigen::Vector3d p0 = transform * Eigen::Vector3d(0, 0, 0);
        const Eigen::Vector3d p1 = transform * Eigen::Vector3d(2, 0, 0);
        const Eigen::Vector3d p2 = transform * Eigen::Vector3d(0, 1, 0);
        const Eigen::Vector3d expected_min = p0.cwiseMin(p1).cwiseMin(p2);
        const Eigen::Vector3d expected_max = p0.cwiseMax(p1).cwiseMax(p2);

        REQUIRE(bbox.min().isApprox(expected_min));
        REQUIRE(bbox.max().isApprox(expected_max));
    }

    SECTION("transformed empty mesh")
    {
        lagrange::SurfaceMesh<Scalar, Index> mesh;
        const auto bbox = lagrange::mesh_bbox<3>(mesh, Eigen::Affine3d::Identity());
        REQUIRE(bbox.isEmpty());
    }
}

TEST_CASE("mesh_bbox 2D", "[core][mesh_bbox]")
{
    using Scalar = float;
    using Index = uint32_t;

    SECTION("empty mesh")
    {
        lagrange::SurfaceMesh<Scalar, Index> mesh(2);
        auto bbox = lagrange::mesh_bbox<2>(mesh);
        REQUIRE(bbox.isEmpty());
    }

    SECTION("two vertices")
    {
        lagrange::SurfaceMesh<Scalar, Index> mesh(2);
        mesh.add_vertex({-1, 3});
        mesh.add_vertex({4, -2});
        auto bbox = lagrange::mesh_bbox<2>(mesh);
        REQUIRE(bbox.min().x() == -1);
        REQUIRE(bbox.min().y() == -2);
        REQUIRE(bbox.max().x() == 4);
        REQUIRE(bbox.max().y() == 3);
    }

    SECTION("transformed vertices")
    {
        lagrange::SurfaceMesh<Scalar, Index> mesh(2);
        mesh.add_vertex({-1, 3});
        mesh.add_vertex({4, -2});
        const Eigen::Affine2f transform = Eigen::Translation2f(2, -1) * Eigen::Scaling(2.f, 3.f);
        const auto bbox = lagrange::mesh_bbox<2>(mesh, transform);
        REQUIRE(bbox.min().x() == 0);
        REQUIRE(bbox.min().y() == -7);
        REQUIRE(bbox.max().x() == 10);
        REQUIRE(bbox.max().y() == 8);
    }
}
