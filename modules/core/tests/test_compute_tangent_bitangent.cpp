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
#include <lagrange/IndexedAttribute.h>
#include <lagrange/attributes/attribute_utils.h>
#include <lagrange/attributes/condense_indexed_attribute.h>
#include <lagrange/compute_normal.h>
#include <lagrange/compute_tangent_bitangent.h>
#include <lagrange/compute_vertex_normal.h>
#include <lagrange/foreach_attribute.h>
#include <lagrange/fs/filesystem.h>
#include <lagrange/internal/attribute_string_utils.h>
#include <lagrange/io/load_mesh.impl.h>
#include <lagrange/map_attribute.h>
#include <lagrange/views.h>

#include <lagrange/testing/common.h>

#include "compute_tangent_bitangent_mikktspace.h"

#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_approx.hpp>

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
    #include <lagrange/mesh_convert.h>
#endif

namespace {

template <typename Scalar, typename Index>
auto corner_tangent_bitangent(lagrange::SurfaceMesh<Scalar, Index>& mesh, bool pad)
{
    REQUIRE(!mesh.has_attribute("@tangent"));
    REQUIRE(!mesh.has_attribute("@bitangent"));
    // Note: With C++20, we should switch to using designated initializers!
    lagrange::TangentBitangentOptions opt;
    opt.pad_with_sign = pad;
    opt.output_element_type = lagrange::AttributeElement::Corner;
    auto res = lagrange::compute_tangent_bitangent(mesh, opt);
    REQUIRE(mesh.has_attribute("@tangent"));
    REQUIRE(mesh.has_attribute("@bitangent"));
    return std::make_pair(
        lagrange::attribute_matrix_view<Scalar>(mesh, res.tangent_id),
        lagrange::attribute_matrix_view<Scalar>(mesh, res.bitangent_id));
}

template <typename Scalar, typename Index>
auto indexed_tangent_bitangent(lagrange::SurfaceMesh<Scalar, Index>& mesh, bool pad)
{
    REQUIRE(!mesh.has_attribute("@tangent"));
    REQUIRE(!mesh.has_attribute("@bitangent"));
    lagrange::TangentBitangentOptions opt;
    opt.pad_with_sign = pad;
    opt.output_element_type = lagrange::AttributeElement::Indexed;
    auto res = lagrange::compute_tangent_bitangent(mesh, opt);
    REQUIRE(mesh.has_attribute("@tangent"));
    REQUIRE(mesh.has_attribute("@bitangent"));
    auto& tangent = mesh.template get_indexed_attribute<Scalar>(res.tangent_id);
    auto& bitangent = mesh.template get_indexed_attribute<Scalar>(res.bitangent_id);
    return std::make_tuple(
        lagrange::matrix_view<Scalar>(tangent.values()),
        lagrange::matrix_view<Index>(tangent.indices()),
        lagrange::matrix_view<Scalar>(bitangent.values()),
        lagrange::matrix_view<Index>(bitangent.indices()));
}

template <typename MeshType, typename DerivedT, typename DerivedB>
auto corner_tangent_bitangent_legacy(
    MeshType& mesh,
    bool pad,
    Eigen::PlainObjectBase<DerivedT>& T,
    Eigen::PlainObjectBase<DerivedB>& B)
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
auto indexed_tangent_bitangent_legacy(
    MeshType& mesh,
    bool pad,
    Eigen::PlainObjectBase<DerivedT>& T,
    Eigen::PlainObjectBase<DerivedB>& B,
    Eigen::PlainObjectBase<DerivedTI>& TI,
    Eigen::PlainObjectBase<DerivedBI>& BI)
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

TEST_CASE("compute_tangent_bitangent", "[core][tangent]" LA_SLOW_DEBUG_FLAG)
{
    using Scalar = double;
    using Index = uint32_t;
    auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/blub/blub.obj");

    const Scalar EPS = 1e-3;
    lagrange::logger().info("Computing indexed normals");
    lagrange::compute_normal(mesh, M_PI * 0.5 - EPS);

    lagrange::logger().info("Computing tangent frame");

    SECTION("corner tangent/bitangent")
    {
        for (bool pad : {true, false}) {
            auto mesh0 = mesh;
            auto mesh1 = mesh;
            auto [T0, B0] = corner_tangent_bitangent(mesh0, pad);
            auto [T1, B1] = corner_tangent_bitangent(mesh1, pad);
            REQUIRE(T0.cols() == (pad ? 4 : 3));
            REQUIRE(B0.cols() == (pad ? 4 : 3));
            REQUIRE(T0 == T1);
            REQUIRE(B0 == B1);
        }
    }

    SECTION("indexed tangent/bitangent")
    {
        for (bool pad : {true, false}) {
            auto mesh0 = mesh;
            auto mesh1 = mesh;
            auto [T0, I0, B0, J0] = indexed_tangent_bitangent(mesh0, pad);
            auto [T1, I1, B1, J1] = indexed_tangent_bitangent(mesh1, pad);
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

TEST_CASE("compute_tangent_bitangent: degenerate", "[core][tangent]")
{
    using Scalar = double;
    using Index = uint32_t;

    lagrange::SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertices(2, {0.1, 1.0, 0.5, 0.9, 0.3, 0.7});
    mesh.add_triangles(2, {0, 1, 1, 1, 1, 1});

    auto uv_id = mesh.create_attribute<Scalar>(
        "uv",
        lagrange::AttributeElement::Indexed,
        lagrange::AttributeUsage::UV,
        2);
    auto& uv_attr = mesh.ref_indexed_attribute<Scalar>(uv_id);
    uv_attr.values().insert_elements({0.0, 0.0});
    std::fill(uv_attr.indices().ref_all().begin(), uv_attr.indices().ref_all().end(), 0);

    lagrange::compute_normal(mesh, M_PI * 0.25);

    SECTION("corner tangent/bitangent")
    {
        for (bool pad : {true, false}) {
            auto mesh0 = mesh;
            auto [T0, B0] = corner_tangent_bitangent(mesh0, pad);
            REQUIRE(T0.cols() == (pad ? 4 : 3));
            REQUIRE(B0.cols() == (pad ? 4 : 3));
            REQUIRE(T0.leftCols(3).isZero(0));
            REQUIRE(B0.leftCols(3).isZero(0));
        }
    }

    SECTION("indexed tangent/bitangent")
    {
        for (bool pad : {true, false}) {
            auto mesh0 = mesh;
            auto [T0, I0, B0, J0] = indexed_tangent_bitangent(mesh0, pad);
            REQUIRE(T0.cols() == (pad ? 4 : 3));
            REQUIRE(B0.cols() == (pad ? 4 : 3));
            REQUIRE(T0.leftCols(3).isZero(0));
            REQUIRE(B0.leftCols(3).isZero(0));
        }
    }
}

TEST_CASE("compute_tangent_bitangent_bug01", "[core][tangent]" LA_CORP_FLAG LA_SLOW_DEBUG_FLAG)
{
    using Scalar = double;
    using Index = uint32_t;

    auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>(
        "corp/core/Erin_Kim__comfy_substance_6_dbg_objs/Erin_Kim__comfy_substance_6.20.obj");

    const Scalar EPS = 1e-3;
    lagrange::logger().debug("compute_normal()");
    lagrange::compute_normal(mesh, M_PI * 0.5 - EPS);
    lagrange::logger().debug("compute_indexed_tangent_bitangent()");
    lagrange::TangentBitangentOptions opt;
    opt.pad_with_sign = false;
    lagrange::compute_tangent_bitangent(mesh, opt);
    lagrange::logger().debug("map_attribute to corner (tangent)");
    map_attribute(mesh, "@tangent", "corner_tangent", lagrange::AttributeElement::Corner);
    lagrange::logger().debug("map_attribute to corner (bitangent)");
    map_attribute(mesh, "@bitangent", "corner_bitangent", lagrange::AttributeElement::Corner);
    lagrange::logger().debug("map_attribute to corner (normal)");
    map_attribute(mesh, "@normal", "corner_normal", lagrange::AttributeElement::Corner);
}

TEST_CASE("compute_tangent_bitangent cube", "[core][tangent]")
{
    using Scalar = double;
    using Index = uint32_t;

    // Initialize cube vertices/facets.
    lagrange::SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({1, 1, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_vertex({0, 0, 1});
    mesh.add_vertex({1, 0, 1});
    mesh.add_vertex({1, 1, 1});
    mesh.add_vertex({0, 1, 1});
    mesh.add_quad(0, 3, 2, 1);
    mesh.add_quad(4, 5, 6, 7);
    mesh.add_quad(1, 2, 6, 5);
    mesh.add_quad(4, 7, 3, 0);
    mesh.add_quad(2, 3, 7, 6);
    mesh.add_quad(0, 1, 5, 4);

    // Cube uvs.
    std::array<Scalar, 28> uv_values{0.25, 0,    0.5,  0,    0.25, 0.25, 0.5,  0.25, 0.25, 0.5,
                                     0.5,  0.5,  0.25, 0.75, 0.5,  0.75, 0.25, 1,    0.5,  1,
                                     0,    0.75, 0,    0.5,  0.75, 0.75, 0.75, 0.5};
    std::array<Index, 24> uv_indices{8,  6, 7, 9,  2, 3, 5, 4, 12, 7, 5, 13,
                                     11, 4, 6, 10, 7, 6, 4, 5, 0,  1, 3, 2};
    mesh.template create_attribute<Scalar>(
        "uv",
        lagrange::AttributeElement::Indexed,
        2,
        lagrange::AttributeUsage::UV,
        uv_values,
        uv_indices);

    // Cube normals.
    constexpr Scalar EPS = 1e-3;
    auto normal_id = lagrange::compute_normal(mesh, M_PI * 0.5 - EPS);

    lagrange::TangentBitangentOptions opt;
    opt.output_element_type = lagrange::AttributeElement::Corner;
    auto r = lagrange::compute_tangent_bitangent(mesh, opt);

    const auto& normal_attr = mesh.template get_indexed_attribute<Scalar>(normal_id);
    const auto& tangent_attr = mesh.template get_attribute<Scalar>(r.tangent_id);
    const auto& bitangent_attr = mesh.template get_attribute<Scalar>(r.bitangent_id);

    auto normals = matrix_view(normal_attr.values());
    auto normal_indices = vector_view(normal_attr.indices());
    auto tangents = matrix_view(tangent_attr);
    auto bitangents = matrix_view(bitangent_attr);

    for (auto cid : lagrange::range(mesh.get_num_corners())) {
        Eigen::RowVector<Scalar, 3> n = normals.row(normal_indices(cid));
        Eigen::RowVector<Scalar, 3> t = tangents.row(cid);
        Eigen::RowVector<Scalar, 3> b = bitangents.row(cid);
        REQUIRE(n.dot(t) == Catch::Approx(0));
        REQUIRE(n.dot(b) == Catch::Approx(0));
        REQUIRE(t.dot(b) == Catch::Approx(0));
    }
}

namespace {

template <typename Scalar, typename Index>
auto weld_mesh(lagrange::SurfaceMesh<Scalar, Index> mesh)
{
    // Condense UV and normal attributes. Mikktspace always welds together corners that share
    // identical pos/uv/normals, since they have no notion of indexed attributes. To reproduce
    // results from Mikktspace implementation, we must weld our input UV and normal attributes
    // as pre-processing.
    auto legacy_mesh = lagrange::to_legacy_mesh<lagrange::TriangleMesh3Df>(mesh);
    condense_indexed_attribute(*legacy_mesh, "uv");
    condense_indexed_attribute(*legacy_mesh, "normal");
    return lagrange::to_surface_mesh_copy<Scalar, Index>(*legacy_mesh);
};

} // namespace

#ifdef LAGRANGE_WITH_MIKKTSPACE

TEST_CASE("compute_tangent_bitangent mikktspace", "[core][tangent]" LA_CORP_FLAG)
{
    using Scalar = float;
    using Index = uint32_t;

    enum class NormalType { Original, Vertex, Indexed };

    auto compute_normals = [](lagrange::SurfaceMesh<Scalar, Index>& mesh,
                              NormalType normal_type,
                              Scalar angle_threshold_deg) {
        if (normal_type == NormalType::Original) {
            // Nothing to do
            lagrange::logger().info("Using original mesh normals");
        } else if (normal_type == NormalType::Vertex) {
            lagrange::logger().info("Computing vertex normals");
            mesh.delete_attribute("normal");
            compute_vertex_normal(mesh);
            map_attribute_in_place(mesh, "@vertex_normal", lagrange::AttributeElement::Indexed);
            mesh.rename_attribute("@vertex_normal", "normal");
        } else {
            lagrange::logger().info(
                "Computing indexed normals with angle thres={}",
                angle_threshold_deg);
            mesh.delete_attribute("normal");
            const Scalar EPS = 1e-3;
            lagrange::compute_normal(mesh, angle_threshold_deg * Scalar(M_PI / 180.0) - EPS);
            mesh.rename_attribute("@normal", "normal");
            mesh = weld_mesh(std::move(mesh));
        }
    };

    auto compare_tangent_bitangent = [](const lagrange::SurfaceMesh<Scalar, Index>& mesh) {
        lagrange::logger().info("Computing tangent frame");
        auto mesh_mk = mesh;
        auto mesh_in = mesh;
        // Mikktspace tangent/bitangent
        lagrange::compute_tangent_bitangent_mikktspace(mesh_mk);
        {
            // Indexed tangent/bitangent
            lagrange::compute_tangent_bitangent(mesh_in);
            map_attribute(
                mesh_in,
                "@tangent",
                "corner_tangent",
                lagrange::AttributeElement::Corner);
            map_attribute(
                mesh_in,
                "@bitangent",
                "corner_bitangent",
                lagrange::AttributeElement::Corner);
        }

        auto T_mk = lagrange::attribute_matrix_view<Scalar>(mesh_mk, "@tangent");
        auto B_mk = lagrange::attribute_matrix_view<Scalar>(mesh_mk, "@bitangent");

        auto T_in = lagrange::attribute_matrix_view<Scalar>(mesh_in, "corner_tangent");
        auto B_in = lagrange::attribute_matrix_view<Scalar>(mesh_in, "corner_bitangent");

        Scalar t_l2 = (T_in - T_mk).norm();
        Scalar b_l2 = (B_in - B_mk).norm();
        Scalar t_linf = (T_in - T_mk).lpNorm<Eigen::Infinity>();
        Scalar b_linf = (B_in - B_mk).lpNorm<Eigen::Infinity>();
        lagrange::logger().info("tangent l2 error: {}", t_l2);
        lagrange::logger().info("bitangent l2 error: {}", b_l2);
        lagrange::logger().info("tangent max error: {}", t_linf);
        lagrange::logger().info("bitangent max error: {}", b_linf);
        REQUIRE(t_l2 < 1e-5f);
        REQUIRE(b_l2 < 1e-5f);
        REQUIRE(t_linf < 1e-6f);
        REQUIRE(b_linf < 1e-6f);
    };

    auto original_mesh =
        lagrange::testing::load_surface_mesh<Scalar, Index>("corp/core/nmtest_no_tb_tri.obj");
    original_mesh = weld_mesh(std::move(original_mesh));

    for (auto normal_type : {NormalType::Original, NormalType::Vertex, NormalType::Indexed}) {
        if (normal_type == NormalType::Indexed) {
            for (Scalar angle_threshold_deg : {0, 45, 90, 180}) {
                auto mesh = original_mesh;
                compute_normals(mesh, normal_type, angle_threshold_deg);
                compare_tangent_bitangent(mesh);
            }
        } else {
            auto mesh = original_mesh;
            compute_normals(mesh, normal_type, 0);
            compare_tangent_bitangent(mesh);
        }
    }
}

#endif

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS

TEST_CASE("compute_tangent_bitangent old vs new", "[core][tangent]" LA_SLOW_DEBUG_FLAG)
{
    using Scalar = double;
    using Index = uint32_t;

    using VertexArray = Eigen::Matrix<Scalar, Eigen::Dynamic, 3, Eigen::RowMajor>;
    using FacetArray = Eigen::Matrix<Index, Eigen::Dynamic, 3, Eigen::RowMajor>;
    using MeshType = lagrange::Mesh<VertexArray, FacetArray>;
    using AttributeArray = MeshType::AttributeArray;

    auto legacy_mesh = lagrange::testing::load_mesh<MeshType>("open/core/blub/blub.obj");

    const double EPS = 1e-3;
    lagrange::logger().info("Computing indexed normals");
    lagrange::compute_normal(*legacy_mesh, M_PI * 0.5 - EPS);

    auto surface_mesh = lagrange::to_surface_mesh_copy<Scalar, Index>(*legacy_mesh);

    lagrange::logger().info("Computing tangent frame");

    SECTION("corner tangent/bitangent")
    {
        AttributeArray T0, B0;
        for (bool pad : {true, false}) {
            auto mesh1 = surface_mesh;
            auto [T1, B1] = corner_tangent_bitangent(mesh1, pad);
            corner_tangent_bitangent_legacy(*legacy_mesh, pad, T0, B0);
            REQUIRE(T0.cols() == (pad ? 4 : 3));
            REQUIRE(B0.cols() == (pad ? 4 : 3));
            REQUIRE(T1.cols() == (pad ? 4 : 3));
            REQUIRE(B1.cols() == (pad ? 4 : 3));
            // Old vs new corner T/BT won't match anymore, since we now project onto the plane
            // orthogonal to the per-corner input normal. (The previous mode was to project only
            // when aggregating indexed T/BT).
            // REQUIRE(T0 == T1);
            // REQUIRE(B0 == B1);
        }
    }

    SECTION("indexed tangent/bitangent")
    {
        AttributeArray T0, B0;
        FacetArray I0, J0;
        for (bool pad : {true, false}) {
            auto mesh1 = surface_mesh;
            auto [T1, I1, B1, J1] = indexed_tangent_bitangent(mesh1, pad);
            indexed_tangent_bitangent_legacy(*legacy_mesh, pad, T0, B0, I0, J0);
            REQUIRE(T0.cols() == (pad ? 4 : 3));
            REQUIRE(B0.cols() == (pad ? 4 : 3));
            REQUIRE(I0 == J0);
            REQUIRE(I1 == I1);
            REQUIRE(I0 == J0);
            REQUIRE(T0 == T1);
            REQUIRE(B0 == B1);
        }
    }
}

#endif

TEST_CASE("compute_tangent_bitangent benchmark", "[core][tangent][!benchmark]" LA_CORP_FLAG)
{
    using Scalar = float;
    using Index = uint32_t;

    auto mesh = weld_mesh(lagrange::testing::load_surface_mesh<Scalar, Index>(
        "corp/displacement/MeetMat2/MeetMat2_Rogelio.obj"));

#ifdef LAGRANGE_WITH_MIKKTSPACE
    BENCHMARK_ADVANCED("compute_tangent_bitangent_mikkt")
    (Catch::Benchmark::Chronometer meter)
    {
        auto copy = mesh;
        meter.measure([&]() { return lagrange::compute_tangent_bitangent_mikktspace(copy); });
    };
#endif

    BENCHMARK_ADVANCED("compute_tangent_bitangent_new")(Catch::Benchmark::Chronometer meter)
    {
        auto copy = mesh;
        meter.measure([&]() { return lagrange::compute_tangent_bitangent(copy); });
    };

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
    BENCHMARK_ADVANCED("compute_tangent_bitangent_old")(Catch::Benchmark::Chronometer meter)
    {
        auto legacy_mesh = lagrange::to_legacy_mesh<lagrange::TriangleMesh3Df>(mesh);
        meter.measure(
            [&]() { return lagrange::compute_indexed_tangent_bitangent(*legacy_mesh, false); });
    };
#endif
}
