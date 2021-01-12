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

#include <lagrange/Mesh.h>
#include <lagrange/MeshTrait.h>
#include <lagrange/common.h>
#include <lagrange/io/load_mesh.h>
#include <lagrange/mesh_cleanup/close_small_holes.h>

#include <lagrange/testing/common.h>

namespace {

template <typename MeshType>
int count_boundary_edges(MeshType &mesh)
{
    static_assert(lagrange::MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");
    using Index = typename MeshType::Index;

    mesh.initialize_edge_data();
    int num_boundary_edges = 0;
    for (Index e = 0; e < mesh.get_num_edges(); ++e) {
        if (mesh.get_is_edge_boundary(e)) {
            ++num_boundary_edges;
        }
    }
    return num_boundary_edges;
}

template <typename MeshType>
bool has_holes(MeshType &mesh) { return count_boundary_edges(mesh) != 0; }

} // namespace

TEST_CASE("close_small_holes (simple)", "[cleanup][close_small_holes]")
{
    auto mesh = lagrange::testing::load_mesh<lagrange::TriangleMesh3D>("open/core/stanford-bunny.obj");
    REQUIRE(mesh);

    REQUIRE(has_holes(*mesh));

    auto mesh1 = lagrange::close_small_holes(*mesh, 10);
    REQUIRE(has_holes(*mesh1));
    auto mesh2 = lagrange::close_small_holes(*mesh, 100);
    REQUIRE(!has_holes(*mesh2));
}

TEST_CASE("close_small_holes (complex)", "[cleanup][close_small_holes]")
{
    auto mesh = lagrange::testing::load_mesh<lagrange::TriangleMesh3D>("open/core/grid_holes.obj");
    REQUIRE(mesh);

    REQUIRE(has_holes(*mesh));
    lagrange::logger().info("num boundary edges: {}", count_boundary_edges(*mesh));

    auto mesh1 = lagrange::close_small_holes(*mesh, 20);
    REQUIRE(count_boundary_edges(*mesh1) == 8*8 + 15);
    auto mesh2 = lagrange::close_small_holes(*mesh, 100);
    REQUIRE(count_boundary_edges(*mesh2) == 15);
}

TEST_CASE("close_small_holes (with uv)", "[cleanup][close_small_holes]")
{
    auto mesh = lagrange::testing::load_mesh<lagrange::TriangleMesh3D>("open/core/blub_open.obj");
    REQUIRE(mesh);

    REQUIRE(has_holes(*mesh));
    lagrange::logger().info("num boundary edges: {}", count_boundary_edges(*mesh));

    auto mesh1 = lagrange::close_small_holes(*mesh, 20);
    REQUIRE(count_boundary_edges(*mesh1) == 64);
    auto mesh2 = lagrange::close_small_holes(*mesh, 100);
    REQUIRE(count_boundary_edges(*mesh2) == 0);

    auto mesh0 = lagrange::testing::load_mesh<lagrange::TriangleMesh3D>("open/core/blub_open_filled.obj");
    REQUIRE(mesh2->get_vertices().isApprox(mesh0->get_vertices()));
    REQUIRE(mesh2->get_facets() == mesh0->get_facets());
    REQUIRE(mesh2->get_uv().isApprox(mesh0->get_uv()));
    REQUIRE(mesh2->get_uv_indices() == mesh0->get_uv_indices());
}

TEST_CASE("close_small_holes (with attributes)", "[cleanup][close_small_holes]")
{
    auto mesh = lagrange::testing::load_mesh<lagrange::TriangleMesh3D>("open/core/blub_open.obj");
    REQUIRE(mesh);

    REQUIRE(has_holes(*mesh));
    lagrange::logger().info("num boundary edges: {}", count_boundary_edges(*mesh));

    using AttributeArray = lagrange::TriangleMesh3D::AttributeArray;
    using Index = lagrange::TriangleMesh3D::Index;
    mesh->initialize_edge_data();
    mesh->initialize_edge_data_new();

    srand(0);
    AttributeArray vertex_attr = AttributeArray::Random(mesh->get_num_vertices(), 4);
    AttributeArray facet_attr = AttributeArray::Random(mesh->get_num_facets(), 5);
    AttributeArray corner_attr = AttributeArray::Random(mesh->get_num_facets() * 3, 3);
    AttributeArray edge_attr = AttributeArray::Random(mesh->get_num_edges(), 2);
    AttributeArray edge_new_attr = AttributeArray::Random(mesh->get_num_edges_new(), 1);
    mesh->add_vertex_attribute("color");
    mesh->set_vertex_attribute("color", vertex_attr);
    mesh->add_facet_attribute("normal");
    mesh->set_facet_attribute("normal", facet_attr);
    mesh->add_corner_attribute("kwak");
    mesh->set_corner_attribute("kwak", corner_attr);
    mesh->add_edge_attribute("weight");
    mesh->set_edge_attribute("weight", edge_attr);
    mesh->add_edge_attribute_new("bary");
    mesh->set_edge_attribute_new("bary", edge_new_attr);

    auto mesh1 = lagrange::close_small_holes(*mesh, 100);
    REQUIRE(count_boundary_edges(*mesh1) == 0);

    const auto &vertex_attr1 = mesh1->get_vertex_attribute("color");
    const auto &facet_attr1 = mesh1->get_facet_attribute("normal");
    const auto &corner_attr1 = mesh1->get_corner_attribute("kwak");
    const auto &edge_attr1 = mesh1->get_edge_attribute("weight");
    const auto &edge_new_attr1 = mesh1->get_edge_attribute_new("bary");
    REQUIRE(vertex_attr == vertex_attr1.topRows(vertex_attr.rows()));
    REQUIRE(facet_attr == facet_attr1.topRows(facet_attr.rows()));
    REQUIRE(corner_attr == corner_attr1.topRows(corner_attr.rows()));
    REQUIRE(edge_attr == edge_attr1.topRows(edge_attr.rows()));

    // New edges might be indexed differently, so let's iterate over facet corners and compare values
    bool all_same = true;
    for (Index f = 0; f < mesh->get_num_facets(); ++f) {
        for (Index lv = 0; lv < 3; ++lv) {
            auto e = mesh->get_edge_new(f, lv);
            auto e1 = mesh1->get_edge_new(f, lv);
            all_same &= (edge_new_attr.row(e) == edge_new_attr1.row(e1));
        }
    }
    REQUIRE(all_same);
}
