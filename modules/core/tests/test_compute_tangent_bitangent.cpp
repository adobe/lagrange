/*
 * Copyright 2020 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/MeshTrait.h>
#include <lagrange/attributes/attribute_utils.h>
#include <lagrange/compute_normal.h>
#include <lagrange/compute_tangent_bitangent.h>
#include <lagrange/create_mesh.h>

#include <lagrange/testing/common.h>

namespace {

template <typename MeshType, typename DerivedT, typename DerivedB>
auto corner_tangent_bitangent(
    MeshType &mesh,
    bool pad,
    Eigen::PlainObjectBase<DerivedT> &T,
    Eigen::PlainObjectBase<DerivedB> &B)
{
    static_assert(lagrange::MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");
    REQUIRE(!mesh.has_corner_attribute("tangent"));
    REQUIRE(!mesh.has_corner_attribute("bitangent"));
    lagrange::compute_corner_tangent_bitangent(mesh, pad);
    REQUIRE(mesh.has_corner_attribute("tangent"));
    REQUIRE(mesh.has_corner_attribute("bitangent"));
    mesh.export_corner_attribute("tangent", T);
    mesh.export_corner_attribute("bitangent", B);
    mesh.remove_corner_attribute("tangent");
    mesh.remove_corner_attribute("bitangent");
}

template <
    typename MeshType,
    typename DerivedT,
    typename DerivedB,
    typename DerivedTI,
    typename DerivedBI>
auto indexed_tangent_bitangent(
    MeshType &mesh,
    bool pad,
    Eigen::PlainObjectBase<DerivedT> &T,
    Eigen::PlainObjectBase<DerivedB> &B,
    Eigen::PlainObjectBase<DerivedTI> &TI,
    Eigen::PlainObjectBase<DerivedBI> &BI)
{
    static_assert(lagrange::MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");
    REQUIRE(!mesh.has_indexed_attribute("tangent"));
    REQUIRE(!mesh.has_indexed_attribute("bitangent"));
    lagrange::compute_indexed_tangent_bitangent(mesh, pad);
    REQUIRE(mesh.has_indexed_attribute("tangent"));
    REQUIRE(mesh.has_indexed_attribute("bitangent"));
    mesh.export_indexed_attribute("tangent", T, TI);
    mesh.export_indexed_attribute("bitangent", B, BI);
    mesh.remove_indexed_attribute("tangent");
    mesh.remove_indexed_attribute("bitangent");
}

} // namespace

TEST_CASE("compute_tangent_bitangent", "[core]")
{
    using AttributeArray = lagrange::TriangleMesh3D::AttributeArray;
    using FacetArray = lagrange::TriangleMesh3D::FacetArray;

    auto mesh = lagrange::testing::load_mesh<lagrange::TriangleMesh3D>("open/core/blub/blub.obj");

    const double EPS = 1e-3;
    lagrange::logger().info("Computing indexed normals");
    lagrange::compute_normal(*mesh, M_PI * 0.5 - EPS);

    lagrange::logger().info("Computing tangent frame");

    SECTION("corner tangent/bitangent")
    {
        AttributeArray T0, T1, B0, B1;
        for (bool pad : {true, false}) {
            corner_tangent_bitangent(*mesh, pad, T0, B0);
            corner_tangent_bitangent(*mesh, pad, T1, B1);
            REQUIRE(T0.cols() == (pad ? 4 : 3));
            REQUIRE(B0.cols() == (pad ? 4 : 3));
            REQUIRE(T0 == T1);
            REQUIRE(B0 == B1);
        }
    }

    SECTION("indexed tangent/bitangent")
    {
        AttributeArray T0, T1, B0, B1;
        FacetArray I0, I1, J0, J1;
        for (bool pad : {true, false}) {
            indexed_tangent_bitangent(*mesh, pad, T0, B0, I0, J0);
            indexed_tangent_bitangent(*mesh, pad, T1, B1, I1, J1);
            REQUIRE(T0.cols() == (pad ? 4 : 3));
            REQUIRE(B0.cols() == (pad ? 4 : 3));
            REQUIRE(T0 == T1);
            REQUIRE(B0 == B1);
            REQUIRE(I0 == I1);
            REQUIRE(J0 == J1);
            REQUIRE(I0 == J0);
        }
    }
}

TEST_CASE("compute_tangent_bitangent: degenerate", "[core]")
{
    Eigen::Matrix<double, Eigen::Dynamic, 3> vertices(2, 3);
    vertices << 0.1, 1.0, 0.5, 0.9, 0.3, 0.7;
    Eigen::Matrix<uint64_t, Eigen::Dynamic, 3> facets(2, 3);
    facets << 0, 1, 1, 1, 1, 1;

    lagrange::Vertices2D uvs(1, 2);
    uvs << 0.0, 0.0;
    Eigen::Matrix<uint64_t, Eigen::Dynamic, 3> uv_indices(2, 3);
    uv_indices << 0, 0, 0, 0, 0, 0;

    auto mesh = lagrange::wrap_with_mesh(vertices, facets);
    mesh->initialize_uv(uvs, uv_indices);

    using MeshType = typename decltype(mesh)::element_type;
    using AttributeArray = MeshType::AttributeArray;
    using FacetArray = MeshType::FacetArray;

    compute_normal(*mesh, M_PI * 0.25);
    REQUIRE(mesh->has_indexed_attribute("normal"));

    SECTION("corner tangent/bitangent")
    {
        AttributeArray T0, B0;
        for (bool pad : {true, false}) {
            corner_tangent_bitangent(*mesh, pad, T0, B0);
            REQUIRE(T0.cols() == (pad ? 4 : 3));
            REQUIRE(B0.cols() == (pad ? 4 : 3));
            REQUIRE(T0.leftCols(3).isZero(0));
            REQUIRE(B0.leftCols(3).isZero(0));
        }
    }

    SECTION("indexed tangent/bitangent")
    {
        AttributeArray T0, B0;
        FacetArray I0, J0;
        for (bool pad : {true, false}) {
            indexed_tangent_bitangent(*mesh, pad, T0, B0, I0, J0);
            REQUIRE(T0.cols() == (pad ? 4 : 3));
            REQUIRE(B0.cols() == (pad ? 4 : 3));
            REQUIRE(T0.leftCols(3).isZero(0));
            REQUIRE(B0.leftCols(3).isZero(0));
        }
    }
}

TEST_CASE("compute_tangent_bitangent_bug01", "[core]" LA_CORP_FLAG)
{
    using namespace lagrange;
    auto mesh = lagrange::testing::load_mesh<TriangleMesh3D>(
        "corp/core/Erin_Kim__comfy_substance_6_dbg_objs/Erin_Kim__comfy_substance_6.20.obj");

    const double EPS = 1e-3;
    logger().debug("compute_normal()");
    compute_normal(*mesh, M_PI * 0.5 - EPS);
    logger().debug("compute_indexed_tangent_bitangent()");
    compute_indexed_tangent_bitangent(*mesh, false);
    logger().debug("map_indexed_attribute_to_corner_attribute()");
    map_indexed_attribute_to_corner_attribute(*mesh, "tangent");
    logger().debug("map_indexed_attribute_to_corner_attribute()");
    map_indexed_attribute_to_corner_attribute(*mesh, "bitangent");
    logger().debug("map_indexed_attribute_to_corner_attribute()");
    map_indexed_attribute_to_corner_attribute(*mesh, "normal");
}

// TODO: Add comparison against a serialized result from mikktspace
