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
#include <lagrange/attribute_names.h>
#include <lagrange/attributes/attribute_utils.h>
#include <lagrange/attributes/condense_indexed_attribute.h>
#include <lagrange/compute_normal.h>
#include <lagrange/compute_tangent_bitangent.h>
#include <lagrange/compute_vertex_normal.h>
#include <lagrange/foreach_attribute.h>
#include <lagrange/fs/filesystem.h>
#include <lagrange/internal/attribute_string_utils.h>
#include <lagrange/io/load_mesh.impl.h>
#include <lagrange/io/save_mesh.h>
#include <lagrange/map_attribute.h>
#include <lagrange/triangulate_polygonal_facets.h>
#include <lagrange/unify_index_buffer.h>
#include <lagrange/views.h>
#include <lagrange/weld_indexed_attribute.h>

#include <lagrange/testing/common.h>

#include "compute_tangent_bitangent_mikktspace.h"

#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <lagrange/testing/require_approx.h>

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
    #include <lagrange/mesh_convert.h>
#endif


namespace {

template <typename Scalar, typename Index>
auto corner_tangent_bitangent(
    lagrange::SurfaceMesh<Scalar, Index>& mesh,
    bool pad,
    bool ortho = false)
{
    REQUIRE(!mesh.has_attribute("@tangent"));
    REQUIRE(!mesh.has_attribute("@bitangent"));
    // Note: With C++20, we should switch to using designated initializers!
    lagrange::TangentBitangentOptions opt;
    opt.pad_with_sign = pad;
    opt.orthogonalize_bitangent = ortho;
    opt.output_element_type = lagrange::AttributeElement::Corner;
    auto res = lagrange::compute_tangent_bitangent(mesh, opt);
    REQUIRE(mesh.has_attribute("@tangent"));
    REQUIRE(mesh.has_attribute("@bitangent"));
    return std::make_pair(
        lagrange::attribute_matrix_view<Scalar>(mesh, res.tangent_id),
        lagrange::attribute_matrix_view<Scalar>(mesh, res.bitangent_id));
}

template <typename Scalar, typename Index>
auto indexed_tangent_bitangent(
    lagrange::SurfaceMesh<Scalar, Index>& mesh,
    bool pad,
    bool ortho = false)
{
    REQUIRE(!mesh.has_attribute("@tangent"));
    REQUIRE(!mesh.has_attribute("@bitangent"));
    lagrange::TangentBitangentOptions opt;
    opt.pad_with_sign = pad;
    opt.orthogonalize_bitangent = ortho;
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

TEST_CASE("compute_tangent_bitangent_orthogonal", "[core][tangent]" LA_SLOW_DEBUG_FLAG)
{
    using Scalar = double;
    using Index = uint32_t;
    auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/blub/blub.obj");

    const Scalar EPS = 1e-3;
    lagrange::logger().info("Computing indexed normals");
    lagrange::AttributeId normal_id = lagrange::compute_normal(mesh, M_PI * 0.5 - EPS);

    lagrange::AttributeId corner_normal_id =
        map_attribute(mesh, normal_id, "corner_normal", lagrange::AttributeElement::Corner);
    auto N = lagrange::attribute_matrix_view<Scalar>(mesh, corner_normal_id);

    lagrange::logger().info("Computing tangent frame");

    bool pad = true;
    bool ortho = true;

    SECTION("corner tangent/bitangent")
    {
        auto [T, B] = corner_tangent_bitangent(mesh, pad, ortho);
        for (Index i = 0; i < static_cast<Index>(T.rows()); i++) {
            Eigen::RowVector3<Scalar> Nv = N.row(i);
            Eigen::RowVector3<Scalar> Tv = T.row(i).head(3);
            Eigen::RowVector3<Scalar> Bv = B.row(i).head(3);
            Scalar sign = T(i, 3);
            Eigen::RowVector3<Scalar> expected = sign * Nv.cross(Tv);
            REQUIRE(Bv.isApprox(expected, 1e-6));
        }
    }

    SECTION("indexed tangent/bitangent")
    {
        auto mesh0 = mesh;
        auto mesh1 = mesh;
        auto [T0, I0, B0, J0] = indexed_tangent_bitangent(mesh0, pad, true);
        auto [T1, I1, B1, J1] = indexed_tangent_bitangent(mesh1, pad, false);
        REQUIRE(!B0.isApprox(B1, 1e-6));
    }
}

TEST_CASE("compute_tangent_bitangent_keep_existing_indexed", "[core][tangent]")
{
    using Scalar = double;
    using Index = uint32_t;

    const std::string name = "open/core/hemisphere.obj";
    auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>(name);

    // create UV by projecting vertex position to z = 0, then convert to indexed attribute
    auto vertices = vertex_view(mesh);
    std::vector<Scalar> uv_values(2 * mesh.get_num_vertices());
    for (Index i = 0; i < mesh.get_num_vertices(); ++i) {
        uv_values[2 * i + 0] = vertices.row(i)[0];
        uv_values[2 * i + 1] = vertices.row(i)[1];
    }
    mesh.wrap_as_const_indexed_attribute<Scalar>(
        "@uv",
        lagrange::AttributeUsage::UV,
        mesh.get_num_vertices(),
        2,
        uv_values,
        mesh.get_corner_to_vertex().get_all());

    // create normal and tangent attributes as indexed attributes of length 1
    std::vector<Index> indices(mesh.get_num_corners());
    std::fill(indices.begin(), indices.end(), Index(0));

    std::vector<Scalar> normal_values(3, Scalar(0));
    normal_values[1] = Scalar(1); // normal is UnitY
    std::vector<Scalar> tangent_values(3, Scalar(0));
    tangent_values[2] = Scalar(1); // normal is UnitZ

    mesh.wrap_as_const_indexed_attribute<Scalar>(
        "@dubious_normal",
        lagrange::AttributeUsage::Normal,
        1,
        3,
        normal_values,
        indices);
    mesh.wrap_as_const_indexed_attribute<Scalar>(
        "@dubious_tangent",
        lagrange::AttributeUsage::Tangent,
        1,
        3,
        tangent_values,
        indices);

    lagrange::TangentBitangentOptions opt;
    opt.keep_existing_tangent = true;
    opt.orthogonalize_bitangent = true;
    opt.tangent_attribute_name = "@dubious_tangent";
    opt.bitangent_attribute_name = "@dubious_bitangent";
    opt.normal_attribute_name = "@dubious_normal";
    opt.uv_attribute_name = "@uv";
    opt.output_element_type = lagrange::AttributeElement::Indexed;
    opt.pad_with_sign = true;
    auto result = lagrange::compute_tangent_bitangent(mesh, opt);
    auto& bitangent_attrib = mesh.template get_indexed_attribute<Scalar>(result.bitangent_id);
    auto bitangent_ref = matrix_view(bitangent_attrib.values());
    for (Index i = 0; i < static_cast<Index>(bitangent_ref.rows()); ++i) {
        // bitangent should be normal.cross(tangent) = UnitY.cross(UnitZ) = exactly UnitX
        REQUIRE(
            bitangent_ref.row(i).template head<3>() ==
            bitangent_ref(i, 3) * Eigen::RowVector3<Scalar>::UnitX());
    }
}

TEST_CASE("compute_tangent_bitangent_keep_existing_corner", "[core][tangent]")
{
    using Scalar = double;
    using Index = uint32_t;

    const std::string name = "open/core/hemisphere.obj";
    auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>(name);

    // create UV by projecting vertex position to z = 0, then convert to indexed attribute
    auto vertices = vertex_view(mesh);
    std::vector<Scalar> uv_values(2 * mesh.get_num_vertices());
    for (Index i = 0; i < mesh.get_num_vertices(); ++i) {
        uv_values[2 * i + 0] = vertices.row(i)[0];
        uv_values[2 * i + 1] = vertices.row(i)[1];
    }
    mesh.wrap_as_const_indexed_attribute<Scalar>(
        "@uv",
        lagrange::AttributeUsage::UV,
        mesh.get_num_vertices(),
        2,
        uv_values,
        mesh.get_corner_to_vertex().get_all());

    // let normal be vertex position, and then store as indexed attribute
    std::vector<Scalar> normal_values(3 * mesh.get_num_vertices());
    std::copy(
        vertices.data(),
        vertices.data() + 3 * mesh.get_num_vertices(),
        normal_values.begin());
    mesh.wrap_as_const_indexed_attribute<Scalar>(
        "@normal",
        lagrange::AttributeUsage::Normal,
        mesh.get_num_vertices(),
        3,
        normal_values,
        mesh.get_corner_to_vertex().get_all());

    // let tangent be latitude direction, and then store as corner attribute
    lagrange::AttributeId tangent_id = mesh.create_attribute<Scalar>(
        "@tangent",
        lagrange::AttributeElement::Corner,
        3,
        lagrange::AttributeUsage::Tangent);
    auto tangent_ref = lagrange::attribute_matrix_ref<Scalar>(mesh, tangent_id);
    for (Index i = 0; i < mesh.get_num_corners(); ++i) {
        auto vtx_pos = vertices.row(mesh.get_corner_vertex(i));
        tangent_ref.row(i) << vtx_pos[1], -vtx_pos[0], Scalar(0);
    }

    lagrange::TangentBitangentOptions opt;
    opt.keep_existing_tangent = true;
    opt.orthogonalize_bitangent = true;
    opt.pad_with_sign = false;
    opt.tangent_attribute_name = "@tangent";
    opt.bitangent_attribute_name = "@bitangent";
    opt.normal_attribute_name = "@normal";
    opt.uv_attribute_name = "@uv";
    opt.output_element_type = lagrange::AttributeElement::Corner;
    lagrange::compute_tangent_bitangent(mesh, opt);
    lagrange::AttributeId bitangent_id = mesh.get_attribute_id(opt.bitangent_attribute_name);
    auto bitangent_values = lagrange::attribute_matrix_view<Scalar>(mesh, bitangent_id);
    for (Index i = 0; i < static_cast<Index>(bitangent_values.rows()); ++i) {
        REQUIRE_THAT(
            bitangent_values.row(i).dot(tangent_ref.row(i)),
            Catch::Matchers::WithinAbs(0.0, 1e-6));
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

TEST_CASE("compute_tangent_bitangent_nonmanifold", "[core][tangent]")
{
    using Scalar = double;
    using Index = uint32_t;

    auto filenames = {
        "moebius-160-10.ply",
        "nonmanifold_edge.obj",
        "nonmanifold_vertex.obj",
        "nonoriented_edge.obj"};

    for (auto filename : filenames) {
        auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>(
            std::string("open/core/topology/") + filename);
        lagrange::triangulate_polygonal_facets(mesh);

        // Compute trivial UVs
        auto id = mesh.template create_attribute<Scalar>(
            "uv",
            lagrange::AttributeElement::Indexed,
            2,
            lagrange::AttributeUsage::UV);
        auto& attr = mesh.template ref_indexed_attribute<Scalar>(id);
        attr.values().resize_elements(mesh.get_num_vertices());
        matrix_ref(attr.values()) = vertex_view(mesh).leftCols(2);
        vector_ref(attr.indices()) = vector_view(mesh.get_corner_to_vertex());

        const Scalar eps = 1e-3;
        lagrange::logger().debug("compute_normal()");
        lagrange::compute_normal(mesh, M_PI * 0.5 - eps);
        lagrange::logger().debug("compute_indexed_tangent_bitangent()");
        lagrange::compute_tangent_bitangent(mesh);
    }
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
        REQUIRE_THAT(n.dot(t), Catch::Matchers::WithinAbs(Scalar(0), Scalar(1e-6)));
        REQUIRE_THAT(n.dot(b), Catch::Matchers::WithinAbs(Scalar(0), Scalar(1e-6)));
        REQUIRE_THAT(t.dot(b), Catch::Matchers::WithinAbs(Scalar(0), Scalar(1e-6)));
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
    la_runtime_assert(mesh.has_attribute(lagrange::AttributeName::texcoord));
    la_runtime_assert(mesh.has_attribute(lagrange::AttributeName::normal));
    lagrange::WeldOptions options;

    const auto texcoord_id = mesh.get_attribute_id(lagrange::AttributeName::texcoord);
    {
        const auto& attr = mesh.template get_indexed_attribute<Scalar>(texcoord_id);
        lagrange::logger().info(
            "Number of values before welding texcoords: {}",
            attr.values().get_num_elements());
    }
    lagrange::weld_indexed_attribute(mesh, texcoord_id, options);
    {
        const auto& attr = mesh.template get_indexed_attribute<Scalar>(texcoord_id);
        lagrange::logger().info(
            "Number of values after welding texcoords: {}",
            attr.values().get_num_elements());
    }

    const auto normal_id = mesh.get_attribute_id(lagrange::AttributeName::normal);
    {
        const auto& attr = mesh.template get_indexed_attribute<Scalar>(normal_id);
        lagrange::logger().info(
            "Number of values before welding normals: {}",
            attr.values().get_num_elements());
    }
    lagrange::weld_indexed_attribute(mesh, normal_id, options);
    {
        const auto& attr = mesh.template get_indexed_attribute<Scalar>(normal_id);
        lagrange::logger().info(
            "Number of values after welding normals: {}",
            attr.values().get_num_elements());
    }
    return mesh;
};

} // namespace

TEST_CASE("compute_tangent_bitangent nmtest", "[core][tangent]" LA_CORP_FLAG)
{
    using Scalar = float;
    using Index = uint32_t;

    auto original_mesh =
        lagrange::testing::load_surface_mesh<Scalar, Index>("corp/core/nmtest_no_tb_tri.obj");
    original_mesh = weld_mesh(std::move(original_mesh));
    original_mesh.delete_attribute(lagrange::AttributeName::normal);

    for (auto output_element_type :
         {lagrange::AttributeElement::Corner, lagrange::AttributeElement::Indexed}) {
        for (double angle_threshold_deg : {0., 45., 90., 180.}) {
            auto mesh = original_mesh;
            const Scalar EPS = static_cast<Scalar>(1e-3);
            auto nrm_id = lagrange::compute_normal(
                mesh,
                std::max(Scalar(0), Scalar(angle_threshold_deg) * Scalar(M_PI / 180.0) - EPS));

            lagrange::TangentBitangentOptions opt;
            opt.output_element_type = output_element_type;
            std::string nrm_name(mesh.get_attribute_name(nrm_id));
            auto [t_id, bt_id] = lagrange::compute_tangent_bitangent(mesh, opt);
            mesh = lagrange::unify_index_buffer(mesh, {nrm_id, t_id, bt_id});
            mesh.rename_attribute(nrm_name, "Vertex_Normal"); // match ply attribute name

            auto filename = fmt::format(
                "nmtest_{}_{}.ply",
                lagrange::internal::to_string(output_element_type),
                angle_threshold_deg);

            // Uncomment to save a new output
            // lagrange::io::save_mesh(filename, mesh);

            auto expected = lagrange::testing::load_surface_mesh<Scalar, Index>(
                fmt::format("corp/core/regression/{}", filename));

            lagrange::seq_foreach_named_attribute_read<lagrange::AttributeElement::Vertex>(
                mesh,
                [&](const auto& name, auto& attr) {
                    using AttributeType = std::decay_t<decltype(attr)>;
                    using ValueType = typename AttributeType::ValueType;
                    if constexpr (std::is_same_v<ValueType, Scalar>) {
                        CAPTURE(angle_threshold_deg, name);
                        REQUIRE(expected.has_attribute(name));
                        const ValueType eps(static_cast<ValueType>(1e-3));
                        auto X = matrix_view(attr);
                        auto Y = lagrange::attribute_matrix_view<ValueType>(expected, name);
                        for (Eigen::Index i = 0; i < X.size(); ++i) {
                            REQUIRE_THAT(
                                X.data()[i],
                                Catch::Matchers::WithinRel(Y.data()[i], eps) ||
                                    (Catch::Matchers::WithinAbs(Y.data()[i], eps) &&
                                     Catch::Matchers::WithinAbs(0, eps)));
                        }
                    }
                });
        }
    }
}

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
            const Scalar EPS = static_cast<Scalar>(1e-3);
            lagrange::compute_normal(
                mesh,
                std::max(Scalar(0), Scalar(angle_threshold_deg) * Scalar(M_PI / 180.0) - EPS));
            mesh.rename_attribute("@normal", "normal");
            mesh = weld_mesh(std::move(mesh));
        }
    };

    auto compare_tangent_bitangent = [](const lagrange::SurfaceMesh<Scalar, Index>& mesh,
                                        bool ortho) {
        lagrange::logger().info("Computing tangent frame");
        auto mesh_mk = mesh;
        auto mesh_in = mesh;

        lagrange::TangentBitangentOptions opt;
        opt.orthogonalize_bitangent = ortho;

        // Mikktspace tangent/bitangent
        lagrange::compute_tangent_bitangent_mikktspace(mesh_mk, opt);
        {
            // Indexed tangent/bitangent
            lagrange::compute_tangent_bitangent(mesh_in, opt);
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

    for (auto normal_type : {NormalType::Indexed, NormalType::Original, NormalType::Vertex}) {
        for (auto orthogonalize_bitangent : {true, false}) {
            if (normal_type == NormalType::Indexed) {
                // NOTE: For some reason I had to change `0 -> 0.1` for arm64 Xcode14 unit test to
                // pass. There's another mysterious floating point behavior with Xcode 14 that
                // caused a discrepancy between Lagrange's implementation and mikktspace's
                // implementation.
                for (double angle_threshold_deg : {0., 45., 90., 180.}) {
                    auto mesh = original_mesh;
                    compute_normals(mesh, normal_type, static_cast<Scalar>(angle_threshold_deg));
                    CAPTURE(angle_threshold_deg);
                    compare_tangent_bitangent(mesh, orthogonalize_bitangent);
                }
            } else {
                auto mesh = original_mesh;
                compute_normals(mesh, normal_type, 0);
                compare_tangent_bitangent(mesh, orthogonalize_bitangent);
            }
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

    auto mesh = weld_mesh(
        lagrange::testing::load_surface_mesh<Scalar, Index>(
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
        auto copy = mesh;
        copy.rename_attribute(lagrange::AttributeName::texcoord, "uv");
        auto legacy_mesh = lagrange::to_legacy_mesh<lagrange::TriangleMesh3Df>(copy);
        seq_foreach_named_attribute_read(mesh, [&](auto name, auto&&) {
            lagrange::logger().warn("attribute {}", name);
        });
        meter.measure(
            [&]() { return lagrange::compute_indexed_tangent_bitangent(*legacy_mesh, false); });
    };
#endif
}
