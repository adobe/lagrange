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
#include <lagrange/Attribute.h>
#include <lagrange/IndexedAttribute.h>
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMesh.h>
#include <lagrange/compute_normal.h>
#include <lagrange/compute_tangent_bitangent.h>
#include <lagrange/transform_mesh.h>
#include <lagrange/utils/fmt_eigen.h>
#include <lagrange/views.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <lagrange/utils/warnon.h>
// clang-format on

namespace {

enum class TestCase : int {
    Translation = 0,
    UniformScaling = 1,
    NonUniformScaling = 2,
    Rotation = 3,
    SymmetryXY = 4,
    SymmetryXZ = 5,
    NumTestCases = 6,
};

void test_transform_mesh_2d(TestCase test_case)
{
    using Scalar = double;
    using Index = uint32_t;

    lagrange::SurfaceMesh<Scalar, Index> mesh(2);
    mesh.add_vertex({0, 0});
    mesh.add_vertex({1, 0});
    mesh.add_vertex({0, 1});
    mesh.add_triangle(0, 1, 2);

    auto id_uv = mesh.create_attribute<Scalar>(
        "uv",
        lagrange::AttributeElement::Indexed,
        lagrange::AttributeUsage::UV,
        2,
        std::array<Scalar, 6>{
            0.,
            0.,
            1.,
            0.,
            0.,
            1.,
        },
        std::array<Index, 3>{0, 1, 2});

    auto& uv_attr = mesh.get_indexed_attribute<Scalar>(id_uv);

    auto vertices = vertex_view(mesh);
    auto uv = lagrange::matrix_view(uv_attr.values());

    for (Index v = 0; v < 3; ++v) {
        // These results are exact because coordinates are integers, no rounding error involved.
        REQUIRE(uv.row(v) == vertices.row(v));
    }

    switch (test_case) {
    case TestCase::Translation: {
        lagrange::transform_mesh(mesh, Eigen::Affine2d(Eigen::Translation<Scalar, 2>(1, 2)));
        REQUIRE(vertices.row(0) == Eigen::RowVector2d(1, 2));
        break;
    case TestCase::UniformScaling:
        lagrange::transform_mesh(mesh, Eigen::Affine2d(Eigen::Scaling(Scalar(2))));
        REQUIRE(vertices.row(1) == Eigen::RowVector2d(2, 0));
        REQUIRE(vertices.row(2) == Eigen::RowVector2d(0, 2));
        break;
    case TestCase::NonUniformScaling:
        lagrange::transform_mesh(mesh, Eigen::Affine2d(Eigen::Scaling(Scalar(2), Scalar(3))));
        REQUIRE(vertices.row(1) == Eigen::RowVector2d(2, 0));
        REQUIRE(vertices.row(2) == Eigen::RowVector2d(0, 3));
        break;
    case TestCase::Rotation: {
        Eigen::Affine2d M = Eigen::Affine2d::Identity();
        // Rotation of pi/2 around Z
        M.linear() << 0, -1, 1, 0;
        lagrange::transform_mesh(mesh, M);
        REQUIRE(vertices.row(1) == Eigen::RowVector2d(0, 1));
        REQUIRE(vertices.row(2) == Eigen::RowVector2d(-1, 0));
        break;
    }
    case TestCase::SymmetryXY:
        lagrange::transform_mesh(mesh, Eigen::Affine2d(Eigen::Scaling(Scalar(1), Scalar(1))));
        REQUIRE(vertices.row(1) == Eigen::RowVector2d(1, 0));
        REQUIRE(vertices.row(2) == Eigen::RowVector2d(0, 1));
        break;
    case TestCase::SymmetryXZ:
        lagrange::transform_mesh(mesh, Eigen::Affine2d(Eigen::Scaling(Scalar(1), Scalar(-1))));
        REQUIRE(vertices.row(1) == Eigen::RowVector2d(1, 0));
        REQUIRE(vertices.row(2) == Eigen::RowVector2d(0, -1));
        break;
    default: break;
    }
    }
}

void test_transform_mesh_3d(bool pad_with_sign, TestCase test_case)
{
    using Scalar = double;
    using Index = uint32_t;

    lagrange::SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_triangle(0, 1, 2);

    auto id_uv = mesh.create_attribute<Scalar>(
        "uv",
        lagrange::AttributeElement::Indexed,
        lagrange::AttributeUsage::UV,
        2,
        std::array<Scalar, 6>{
            0.,
            0.,
            1.,
            0.,
            0.,
            1.,
        },
        std::array<Index, 3>{0, 1, 2});

    auto id_nrm = lagrange::compute_normal(mesh, M_PI / 4);

    lagrange::TangentBitangentOptions opt;
    opt.pad_with_sign = pad_with_sign;
    auto [id_tangent, id_bitangent] = lagrange::compute_tangent_bitangent(mesh, opt);

    auto& uv_attr = mesh.get_indexed_attribute<Scalar>(id_uv);
    auto& nrm_attr = mesh.get_indexed_attribute<Scalar>(id_nrm);
    auto& tangent_attr = mesh.get_indexed_attribute<Scalar>(id_tangent);
    auto& bitangent_attr = mesh.get_indexed_attribute<Scalar>(id_bitangent);

    auto vertices = vertex_view(mesh);
    auto uv = lagrange::matrix_view(uv_attr.values());
    auto nrm = lagrange::matrix_view(nrm_attr.values());
    auto tangent = lagrange::matrix_view(tangent_attr.values());
    auto bitangent = lagrange::matrix_view(bitangent_attr.values());

    for (Index v = 0; v < 3; ++v) {
        // These results are exact because coordinates are integers, no rounding error involved.
        REQUIRE(uv.row(v) == vertices.row(v).head<2>());
        REQUIRE(nrm.row(v).head<3>() == Eigen::RowVector3d(0, 0, 1));
        REQUIRE(tangent.row(v).head<3>() == Eigen::RowVector3d(1, 0, 0));
        REQUIRE(bitangent.row(v).head<3>() == Eigen::RowVector3d(0, 1, 0));
    }

    switch (test_case) {
    case TestCase::Translation: {
        lagrange::transform_mesh(mesh, Eigen::Affine3d(Eigen::Translation<Scalar, 3>(1, 2, 3)));
        REQUIRE(vertices.row(0) == Eigen::RowVector3d(1, 2, 3));
        REQUIRE(nrm.row(0).head<3>() == Eigen::RowVector3d(0, 0, 1));
        REQUIRE(tangent.row(0).head<3>() == Eigen::RowVector3d(1, 0, 0));
        REQUIRE(bitangent.row(0).head<3>() == Eigen::RowVector3d(0, 1, 0));
        break;
    case TestCase::UniformScaling:
        lagrange::transform_mesh(mesh, Eigen::Affine3d(Eigen::Scaling(Scalar(2))));
        REQUIRE(vertices.row(1) == Eigen::RowVector3d(2, 0, 0));
        REQUIRE(vertices.row(2) == Eigen::RowVector3d(0, 2, 0));
        REQUIRE(nrm.row(0).head<3>() == Eigen::RowVector3d(0, 0, 1));
        REQUIRE(tangent.row(0).head<3>() == Eigen::RowVector3d(1, 0, 0));
        REQUIRE(bitangent.row(0).head<3>() == Eigen::RowVector3d(0, 1, 0));
        break;
    case TestCase::NonUniformScaling:
        lagrange::transform_mesh(
            mesh,
            Eigen::Affine3d(Eigen::Scaling(Scalar(2), Scalar(3), Scalar(4))));
        REQUIRE(vertices.row(1) == Eigen::RowVector3d(2, 0, 0));
        REQUIRE(vertices.row(2) == Eigen::RowVector3d(0, 3, 0));
        REQUIRE(nrm.row(0).head<3>() == Eigen::RowVector3d(0, 0, 1));
        REQUIRE(tangent.row(0).head<3>() == Eigen::RowVector3d(1, 0, 0));
        REQUIRE(bitangent.row(0).head<3>() == Eigen::RowVector3d(0, 1, 0));
        break;
    case TestCase::Rotation: {
        Eigen::Affine3d M = Eigen::Affine3d::Identity();
        // Rotation of pi/2 around X
        M.linear() << 1, 0, 0, 0, 0, -1, 0, 1, 0;
        lagrange::transform_mesh(mesh, M);
        REQUIRE(vertices.row(1) == Eigen::RowVector3d(1, 0, 0));
        REQUIRE(vertices.row(2) == Eigen::RowVector3d(0, 0, 1));
        REQUIRE(nrm.row(0).head<3>() == Eigen::RowVector3d(0, -1, 0));
        REQUIRE(tangent.row(0).head<3>() == Eigen::RowVector3d(1, 0, 0));
        REQUIRE(bitangent.row(0).head<3>() == Eigen::RowVector3d(0, 0, 1));
        break;
    }
    case TestCase::SymmetryXY:
        // This test would fail if we used transpose(inverse(M)) rather than cofactor(M) to
        // transform normals.
        lagrange::transform_mesh(
            mesh,
            Eigen::Affine3d(Eigen::Scaling(Scalar(1), Scalar(1), Scalar(-1))));
        REQUIRE(vertices.row(1) == Eigen::RowVector3d(1, 0, 0));
        REQUIRE(vertices.row(2) == Eigen::RowVector3d(0, 1, 0));
        // Positions have not changed, so neither should the normal/tangent/bitangent
        REQUIRE(nrm.row(0).head<3>() == Eigen::RowVector3d(0, 0, 1));
        REQUIRE(tangent.row(0).head<3>() == Eigen::RowVector3d(1, 0, 0));
        REQUIRE(bitangent.row(0).head<3>() == Eigen::RowVector3d(0, 1, 0));
        break;
    case TestCase::SymmetryXZ:
        // This test would fail if we used transpose(inverse(M)) rather than cofactor(M) to
        // transform normals.
        lagrange::transform_mesh(
            mesh,
            Eigen::Affine3d(Eigen::Scaling(Scalar(1), Scalar(-1), Scalar(1))));
        REQUIRE(vertices.row(1) == Eigen::RowVector3d(1, 0, 0));
        REQUIRE(vertices.row(2) == Eigen::RowVector3d(0, -1, 0));
        // Normal should be flipped now
        REQUIRE(nrm.row(0).head<3>() == Eigen::RowVector3d(0, 0, -1));
        REQUIRE(tangent.row(0).head<3>() == Eigen::RowVector3d(1, 0, 0));
        REQUIRE(bitangent.row(0).head<3>() == Eigen::RowVector3d(0, -1, 0));
        break;
    default: break;
    }
    }
}

} // namespace

TEST_CASE("transform_mesh_2d", "[next]")
{
    for (int i = 0; i < static_cast<int>(TestCase::NumTestCases); ++i) {
        test_transform_mesh_2d(static_cast<TestCase>(i));
    }
}

TEST_CASE("transform_mesh_3d", "[next]")
{
    for (int i = 0; i < static_cast<int>(TestCase::NumTestCases); ++i) {
        test_transform_mesh_3d(false, static_cast<TestCase>(i));
        test_transform_mesh_3d(true, static_cast<TestCase>(i));
    }
}
