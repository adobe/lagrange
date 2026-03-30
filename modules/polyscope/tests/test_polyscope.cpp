/*
 * Copyright 2025 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */

#include <lagrange/polyscope/register_edge_network.h>
#include <lagrange/polyscope/register_mesh.h>
#include <lagrange/polyscope/register_point_cloud.h>
#include <lagrange/polyscope/register_structure.h>
#include <lagrange/testing/common.h>
#include <lagrange/uv_mesh.h>
#include <lagrange/views.h>

namespace {
#ifdef LAGRANGE_POLYSCOPE_MOCK_BACKEND
const std::string g_backend = "openGL_mock";
#else
const std::string g_backend = "";
#endif
} // namespace

TEST_CASE("register_mesh", "[polyscope]")
{
    using Scalar = double;
    using Index = uint32_t;

    std::vector<std::string> paths = {
        "open/core/simple/cube.obj",
        "open/core/simple/quad_meshes/cube.obj",
        "open/core/poly/mixedFaringPart.obj"};

    polyscope::init(g_backend);
    for (const auto& path : paths) {
        auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>(path);
        auto ps_mesh = lagrange::polyscope::register_mesh(path, mesh);
        REQUIRE(ps_mesh != nullptr);

        auto ps_struct = lagrange::polyscope::register_structure(path + "_struct", mesh);
        REQUIRE(dynamic_cast<polyscope::SurfaceMesh*>(ps_struct) != nullptr);

        auto& positions = mesh.get_vertex_to_position();
        auto attr1 = lagrange::polyscope::register_attribute(*ps_mesh, "positions", positions);
        REQUIRE(attr1 != nullptr);

        auto attr2 = lagrange::polyscope::register_attribute(*ps_struct, "positions", positions);
        REQUIRE(attr2 != nullptr);
        REQUIRE(dynamic_cast<polyscope::SurfaceMeshQuantity*>(attr2) != nullptr);
    }
}

namespace {

template <typename Scalar, typename Index>
lagrange::SurfaceMesh<Scalar, Index> project_to_2d(const lagrange::SurfaceMesh<Scalar, Index>& mesh)
{
    lagrange::SurfaceMesh<Scalar, Index> mesh_2d(2);
    auto positions = lagrange::vertex_view(mesh);
    mesh_2d.add_vertices(mesh.get_num_vertices(), [&](Index v, lagrange::span<Scalar> p) {
        p[0] = positions(v, 0);
        p[1] = positions(v, 1);
    });
    for (Index f = 0; f < mesh.get_num_facets(); ++f) {
        auto facet = mesh.get_facet_vertices(f);
        mesh_2d.add_polygon(facet);
    }
    return mesh_2d;
}

} // namespace

TEST_CASE("register_mesh_2d", "[polyscope]")
{
    using Scalar = double;
    using Index = uint32_t;

    std::vector<std::string> paths = {
        "open/core/simple/cube.obj",
        "open/core/simple/quad_meshes/cube.obj",
        "open/core/poly/mixedFaringPart.obj"};

    polyscope::init(g_backend);
    for (const auto& path : paths) {
        auto mesh = project_to_2d(lagrange::testing::load_surface_mesh<Scalar, Index>(path));
        REQUIRE(mesh.get_dimension() == 2);
        auto ps_mesh = lagrange::polyscope::register_mesh(path, mesh);
        REQUIRE(ps_mesh != nullptr);

        auto ps_struct = lagrange::polyscope::register_structure(path + "_struct", mesh);
        REQUIRE(dynamic_cast<polyscope::SurfaceMesh*>(ps_struct) != nullptr);

        auto& positions = mesh.get_vertex_to_position();
        auto attr1 = lagrange::polyscope::register_attribute(*ps_mesh, "positions", positions);
        REQUIRE(attr1 != nullptr);

        auto attr2 = lagrange::polyscope::register_attribute(*ps_struct, "positions", positions);
        REQUIRE(attr2 != nullptr);
        REQUIRE(dynamic_cast<polyscope::SurfaceMeshQuantity*>(attr2) != nullptr);
    }
}

TEST_CASE("register_uv_mesh", "[polyscope]")
{
    using Scalar = double;
    using Index = uint32_t;

    polyscope::init(g_backend);
    auto mesh =
        lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/simple/cube_with_uv.obj");
    auto uv_mesh = lagrange::uv_mesh_view(mesh);
    auto ps_mesh = lagrange::polyscope::register_mesh("mesh", uv_mesh);
    REQUIRE(ps_mesh != nullptr);

    auto ps_struct = lagrange::polyscope::register_structure("mesh_struct", uv_mesh);
    REQUIRE(dynamic_cast<polyscope::SurfaceMesh*>(ps_struct) != nullptr);
}

TEST_CASE("register_points", "[polyscope]")
{
    using Scalar = double;
    using Index = uint32_t;

    auto points = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/simple/cube.obj");
    points.clear_facets();

    polyscope::init(g_backend);
    auto ps_points = lagrange::polyscope::register_point_cloud("point_cloud", points);
    REQUIRE(ps_points != nullptr);

    auto ps_struct = lagrange::polyscope::register_structure("point_cloud_struct", points);
    REQUIRE(dynamic_cast<polyscope::PointCloud*>(ps_struct) != nullptr);

    auto& positions = points.get_vertex_to_position();
    auto attr1 = lagrange::polyscope::register_attribute(*ps_points, "positions", positions);
    REQUIRE(attr1 != nullptr);

    auto attr2 = lagrange::polyscope::register_attribute(*ps_struct, "positions", positions);
    REQUIRE(attr2 != nullptr);
    REQUIRE(dynamic_cast<polyscope::PointCloudQuantity*>(attr2) != nullptr);
}

TEST_CASE("register_points_2d", "[polyscope]")
{
    using Scalar = double;
    using Index = uint32_t;

    polyscope::init(g_backend);
    auto mesh =
        lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/simple/cube_with_uv.obj");
    mesh.clear_facets();
    auto uv_mesh = lagrange::uv_mesh_view(mesh);
    auto ps_points = lagrange::polyscope::register_point_cloud("point_cloud", uv_mesh);
    REQUIRE(ps_points != nullptr);

    auto ps_struct = lagrange::polyscope::register_structure("point_cloud_struct", uv_mesh);
    REQUIRE(dynamic_cast<polyscope::PointCloud*>(ps_struct) != nullptr);
}

TEST_CASE("register_edges", "[polyscope]")
{
    using Scalar = double;
    using Index = uint32_t;

    auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/simple/cube.obj");
    mesh.initialize_edges();

    auto edges = mesh;
    edges.clear_facets();
    for (Index e = 0; e < mesh.get_num_edges(); ++e) {
        auto v = mesh.get_edge_vertices(e);
        edges.add_polygon({v[0], v[1]});
    }
    REQUIRE(edges.get_num_facets() == mesh.get_num_edges());

    polyscope::init(g_backend);
    auto ps_edges = lagrange::polyscope::register_edge_network("edge_network", edges);
    REQUIRE(ps_edges != nullptr);

    auto ps_struct = lagrange::polyscope::register_structure("edge_network_struct", edges);
    REQUIRE(dynamic_cast<polyscope::CurveNetwork*>(ps_struct) != nullptr);

    auto& positions = edges.get_vertex_to_position();
    auto attr1 = lagrange::polyscope::register_attribute(*ps_edges, "positions", positions);
    REQUIRE(attr1 != nullptr);

    auto attr2 = lagrange::polyscope::register_attribute(*ps_struct, "positions", positions);
    REQUIRE(attr2 != nullptr);
    REQUIRE(dynamic_cast<polyscope::CurveNetworkQuantity*>(attr2) != nullptr);
}

TEST_CASE("register_edges_2d", "[polyscope]")
{
    using Scalar = double;
    using Index = uint32_t;

    polyscope::init(g_backend);
    auto mesh =
        lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/simple/cube_with_uv.obj");
    mesh.initialize_edges();

    auto edges = mesh;
    edges.clear_facets();
    for (Index e = 0; e < mesh.get_num_edges(); ++e) {
        auto v = mesh.get_edge_vertices(e);
        edges.add_polygon({v[0], v[1]});
    }
    REQUIRE(edges.get_num_facets() == mesh.get_num_edges());

    auto uv_mesh = lagrange::uv_mesh_view(edges);
    auto ps_edges = lagrange::polyscope::register_edge_network("edge_network", uv_mesh);
    REQUIRE(ps_edges != nullptr);

    auto ps_struct = lagrange::polyscope::register_structure("edge_network_struct", uv_mesh);
    REQUIRE(dynamic_cast<polyscope::CurveNetwork*>(ps_struct) != nullptr);
}

TEST_CASE("register_4channel_attributes", "[polyscope]")
{
    using Scalar = double;
    using Index = uint32_t;

    polyscope::init(g_backend);
    auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/simple/cube.obj");

    // Add 4-channel vertex color attribute (RGBA)
    auto vertex_color_id = mesh.template create_attribute<Scalar>(
        "vertex_color",
        lagrange::AttributeElement::Vertex,
        lagrange::AttributeUsage::Color,
        4);
    auto& vertex_color = mesh.template ref_attribute<Scalar>(vertex_color_id);

    // Add 4-channel vertex vector attribute
    auto vertex_vec_id = mesh.template create_attribute<Scalar>(
        "vertex_vec",
        lagrange::AttributeElement::Vertex,
        lagrange::AttributeUsage::Vector,
        4);
    auto& vertex_vec = mesh.template ref_attribute<Scalar>(vertex_vec_id);

    // Add 4-channel facet color attribute (RGBA)
    auto facet_color_id = mesh.template create_attribute<Scalar>(
        "facet_color",
        lagrange::AttributeElement::Facet,
        lagrange::AttributeUsage::Color,
        4);
    auto& facet_color = mesh.template ref_attribute<Scalar>(facet_color_id);

    // Register mesh and attributes
    auto ps_mesh = lagrange::polyscope::register_mesh("mesh_4ch", mesh);
    REQUIRE(ps_mesh != nullptr);

    auto attr1 = lagrange::polyscope::register_attribute(*ps_mesh, "vertex_color", vertex_color);
    REQUIRE(attr1 != nullptr);

    auto attr2 = lagrange::polyscope::register_attribute(*ps_mesh, "vertex_vec", vertex_vec);
    REQUIRE(attr2 != nullptr);

    auto attr3 = lagrange::polyscope::register_attribute(*ps_mesh, "facet_color", facet_color);
    REQUIRE(attr3 != nullptr);
}
