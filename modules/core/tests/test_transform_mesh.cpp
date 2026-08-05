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
#include <lagrange/internal/constants.h>
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
    NegativeScaling = 6,
    NegativeScalingReorient = 7,
    NumTestCases = 8,
};

void test_transform_mesh_2d(TestCase test_case)
{
    using Scalar = double;
    using Index = uint32_t;
    using RowVector3i = Eigen::Matrix<Index, 1, 3>;

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
    auto facets = facet_view(mesh);
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
        REQUIRE(facets.row(0) == RowVector3i(0, 1, 2));
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
    case TestCase::NegativeScaling:
        lagrange::transform_mesh(mesh, Eigen::Affine2d(Eigen::Scaling(Scalar(-1), Scalar(2))));
        REQUIRE(vertices.row(1) == Eigen::RowVector2d(-1, 0));
        REQUIRE(vertices.row(2) == Eigen::RowVector2d(0, 2));
        REQUIRE(facets.row(0) == RowVector3i(0, 1, 2));
        break;
    case TestCase::NegativeScalingReorient: {
        lagrange::TransformOptions topt;
        topt.reorient = true;
        lagrange::transform_mesh(
            mesh,
            Eigen::Affine2d(Eigen::Scaling(Scalar(-1), Scalar(2))),
            topt);
        REQUIRE(vertices.row(1) == Eigen::RowVector2d(-1, 0));
        REQUIRE(vertices.row(2) == Eigen::RowVector2d(0, 2));
        REQUIRE(facets.row(0) == RowVector3i(2, 1, 0));
        break;
    }
    default: break;
    }
    }
}

void test_transform_mesh_3d(bool pad_with_sign, TestCase test_case)
{
    using Scalar = double;
    using Index = uint32_t;
    using RowVector3i = Eigen::Matrix<Index, 1, 3>;

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

    auto id_nrm = lagrange::compute_normal(mesh, lagrange::internal::pi / 4);

    lagrange::TangentBitangentOptions opt;
    opt.pad_with_sign = pad_with_sign;
    auto [id_tangent, id_bitangent] = lagrange::compute_tangent_bitangent(mesh, opt);

    auto& uv_attr = mesh.get_indexed_attribute<Scalar>(id_uv);
    auto& nrm_attr = mesh.get_indexed_attribute<Scalar>(id_nrm);
    auto& tangent_attr = mesh.get_indexed_attribute<Scalar>(id_tangent);
    auto& bitangent_attr = mesh.get_indexed_attribute<Scalar>(id_bitangent);

    auto vertices = vertex_view(mesh);
    auto facets = facet_view(mesh);
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

    case TestCase::NegativeScaling:
        lagrange::transform_mesh(mesh, Eigen::Affine3d(Eigen::Scaling(Scalar(-1))));
        REQUIRE(vertices.row(1) == Eigen::RowVector3d(-1, 0, 0));
        REQUIRE(vertices.row(2) == Eigen::RowVector3d(0, -1, 0));
        REQUIRE(nrm.row(0).head<3>() == Eigen::RowVector3d(0, 0, 1));
        REQUIRE(tangent.row(0).head<3>() == Eigen::RowVector3d(-1, 0, 0));
        REQUIRE(bitangent.row(0).head<3>() == Eigen::RowVector3d(0, -1, 0));
        REQUIRE(facets.row(0) == RowVector3i(0, 1, 2));
        break;
    case TestCase::NegativeScalingReorient: {
        lagrange::TransformOptions topt;
        topt.reorient = true;
        lagrange::transform_mesh(mesh, Eigen::Affine3d(Eigen::Scaling(Scalar(-1))), topt);
        REQUIRE(vertices.row(1) == Eigen::RowVector3d(-1, 0, 0));
        REQUIRE(vertices.row(2) == Eigen::RowVector3d(0, -1, 0));
        REQUIRE(nrm.row(0).head<3>() == Eigen::RowVector3d(0, 0, -1));
        REQUIRE(tangent.row(0).head<3>() == Eigen::RowVector3d(1, 0, 0));
        REQUIRE(bitangent.row(0).head<3>() == Eigen::RowVector3d(0, 1, 0));
        REQUIRE(facets.row(0) == RowVector3i(2, 1, 0));
        break;
    }
    default: break;
    }
    }
}

// Regression test for normalizing Normal/Tangent attributes on a 2D mesh. The transform used to
// normalize a hardcoded head<3>, which (a) reads past the end of a 2-channel (== dim) row on a 2D
// mesh (out-of-bounds / Eigen assert), and (b) would normalize the extra channel of a 3-channel
// (== dim+1) attribute. Normalization must operate on exactly the Dimension geometric components.
void test_transform_mesh_2d_attribute_normalization()
{
    using Scalar = double;
    using Index = uint32_t;

    lagrange::SurfaceMesh<Scalar, Index> mesh(2);
    mesh.add_vertex({0, 0});
    mesh.add_vertex({1, 0});
    mesh.add_vertex({0, 1});
    mesh.add_triangle(0, 1, 2);

    // dim (2-channel) Normal/Tangent attributes with deliberately non-unit values so normalization
    // is observable: each normal row is (3, 4) (norm 5), each tangent row is (0, 2).
    auto id_nrm = mesh.create_attribute<Scalar>(
        "normal",
        lagrange::AttributeElement::Vertex,
        lagrange::AttributeUsage::Normal,
        2,
        std::array<Scalar, 6>{3., 4., 3., 4., 3., 4.});
    auto id_tan = mesh.create_attribute<Scalar>(
        "tangent",
        lagrange::AttributeElement::Vertex,
        lagrange::AttributeUsage::Tangent,
        2,
        std::array<Scalar, 6>{0., 2., 0., 2., 0., 2.});

    // dim+1 (3-channel) Normal/Tangent attributes: the first 2 channels are the geometric vector
    // (3, 4); the 3rd is an extra channel (e.g. a sign/padding) set to 7 that must be preserved
    // verbatim by both the transform (leftCols<Dimension>) and the normalization (head<Dimension>).
    auto id_nrm3 = mesh.create_attribute<Scalar>(
        "normal3",
        lagrange::AttributeElement::Vertex,
        lagrange::AttributeUsage::Normal,
        3,
        std::array<Scalar, 9>{3., 4., 7., 3., 4., 7., 3., 4., 7.});
    auto id_tan3 = mesh.create_attribute<Scalar>(
        "tangent3",
        lagrange::AttributeElement::Vertex,
        lagrange::AttributeUsage::Tangent,
        3,
        std::array<Scalar, 9>{0., 2., 7., 0., 2., 7., 0., 2., 7.});

    lagrange::TransformOptions opt;
    opt.normalize_normals = true;
    opt.normalize_tangents_bitangents = true;

    // Identity transform: leaves directions unchanged, so we exercise (and isolate) the
    // normalization path. With the previous head<3> code the 2-channel case would read out of
    // bounds and the 3-channel case would normalize across the extra channel.
    lagrange::transform_mesh(mesh, Eigen::Affine2d::Identity(), opt);

    auto nrm = lagrange::matrix_view(mesh.get_attribute<Scalar>(id_nrm));
    auto tan = lagrange::matrix_view(mesh.get_attribute<Scalar>(id_tan));
    auto nrm3 = lagrange::matrix_view(mesh.get_attribute<Scalar>(id_nrm3));
    auto tan3 = lagrange::matrix_view(mesh.get_attribute<Scalar>(id_tan3));
    REQUIRE(nrm.cols() == 2);
    REQUIRE(tan.cols() == 2);
    REQUIRE(nrm3.cols() == 3);
    REQUIRE(tan3.cols() == 3);
    for (Index v = 0; v < 3; ++v) {
        // dim case: the 2-vector is normalized.
        REQUIRE_THAT(nrm.row(v).norm(), Catch::Matchers::WithinAbs(1.0, 1e-12));
        REQUIRE_THAT(nrm.row(v)(0), Catch::Matchers::WithinAbs(0.6, 1e-12));
        REQUIRE_THAT(nrm.row(v)(1), Catch::Matchers::WithinAbs(0.8, 1e-12));
        REQUIRE_THAT(tan.row(v).norm(), Catch::Matchers::WithinAbs(1.0, 1e-12));
        REQUIRE_THAT(tan.row(v)(0), Catch::Matchers::WithinAbs(0.0, 1e-12));
        REQUIRE_THAT(tan.row(v)(1), Catch::Matchers::WithinAbs(1.0, 1e-12));

        // dim+1 case: only the first 2 (geometric) channels are normalized; the extra 3rd channel
        // is left untouched at 7.
        REQUIRE_THAT(nrm3.row(v).head<2>().norm(), Catch::Matchers::WithinAbs(1.0, 1e-12));
        REQUIRE_THAT(nrm3.row(v)(0), Catch::Matchers::WithinAbs(0.6, 1e-12));
        REQUIRE_THAT(nrm3.row(v)(1), Catch::Matchers::WithinAbs(0.8, 1e-12));
        REQUIRE_THAT(nrm3.row(v)(2), Catch::Matchers::WithinAbs(7.0, 1e-12));
        REQUIRE_THAT(tan3.row(v).head<2>().norm(), Catch::Matchers::WithinAbs(1.0, 1e-12));
        REQUIRE_THAT(tan3.row(v)(0), Catch::Matchers::WithinAbs(0.0, 1e-12));
        REQUIRE_THAT(tan3.row(v)(1), Catch::Matchers::WithinAbs(1.0, 1e-12));
        REQUIRE_THAT(tan3.row(v)(2), Catch::Matchers::WithinAbs(7.0, 1e-12));
    }
}

} // namespace

TEST_CASE("transform_mesh_2d", "[next]")
{
    for (int i = 0; i < static_cast<int>(TestCase::NumTestCases); ++i) {
        test_transform_mesh_2d(static_cast<TestCase>(i));
    }
}

TEST_CASE("transform_mesh_2d_attribute_normalization", "[next]")
{
    test_transform_mesh_2d_attribute_normalization();
}

TEST_CASE("transform_mesh_3d", "[next]")
{
    for (int i = 0; i < static_cast<int>(TestCase::NumTestCases); ++i) {
        for (bool pad_with_sign : {false, true}) {
            test_transform_mesh_3d(pad_with_sign, static_cast<TestCase>(i));
        }
    }
}
