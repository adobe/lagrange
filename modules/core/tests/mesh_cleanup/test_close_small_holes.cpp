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

#include <lagrange/compute_seam_edges.h>
#include <lagrange/mesh_cleanup/close_small_holes.h>
#include <lagrange/orientation.h>
#include <lagrange/testing/check_mesh.h>
#include <lagrange/testing/common.h>
#include <lagrange/views.h>

namespace {

template <typename Scalar, typename Index>
Index count_boundary_edges(lagrange::SurfaceMesh<Scalar, Index> mesh)
{
    mesh.initialize_edges();
    const Index num_edges = mesh.get_num_edges();
    Index num_bd_edges = 0;
    for (Index ei = 0; ei < num_edges; ei++) {
        if (mesh.is_boundary_edge(ei)) {
            num_bd_edges++;
        }
    }
    return num_bd_edges;
}

} // namespace

TEST_CASE("close_small_holes", "[close_small_holes][surface][mesh_cleanup]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    auto has_holes = [&](SurfaceMesh<Scalar, Index>& mesh) {
        return count_boundary_edges(mesh) != 0;
    };

    SECTION("simple")
    {
        CloseSmallHolesOptions options;
        auto mesh =
            lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/stanford-bunny.obj");
        REQUIRE(has_holes(mesh));

        SECTION("hole size < 10")
        {
            options.max_hole_size = 10;
            close_small_holes(mesh, options);
            REQUIRE(has_holes(mesh));
            REQUIRE(is_oriented(mesh));
        }
        SECTION("hole size < 100")
        {
            options.max_hole_size = 100;
            close_small_holes(mesh, options);
            REQUIRE(!has_holes(mesh));
            REQUIRE(is_oriented(mesh));
        }
    }

    SECTION("complex")
    {
        CloseSmallHolesOptions options;
        auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/grid_holes.obj");
        REQUIRE(has_holes(mesh));

        SECTION("hole size < 20")
        {
            options.max_hole_size = 20;
            close_small_holes(mesh, options);
            REQUIRE(count_boundary_edges(mesh) == 8 * 8 + 15);
            REQUIRE(is_oriented(mesh));
        }
        SECTION("hole size < 100")
        {
            options.max_hole_size = 100;
            close_small_holes(mesh, options);
            REQUIRE(count_boundary_edges(mesh) == 15);
            REQUIRE(is_oriented(mesh));
        }
    }

    SECTION("non-manifold")
    {
        CloseSmallHolesOptions options;
        auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/prout.obj");
        REQUIRE(has_holes(mesh));
        REQUIRE(count_boundary_edges(mesh) == 141);

        options.max_hole_size = 3;
        close_small_holes(mesh, options);
        REQUIRE(count_boundary_edges(mesh) == 105);
    }

    SECTION("with uv")
    {
        CloseSmallHolesOptions options;
        auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/blub_open.obj");
        REQUIRE(has_holes(mesh));

        SECTION("hole size < 20")
        {
            options.max_hole_size = 20;
            close_small_holes(mesh, options);
            REQUIRE(count_boundary_edges(mesh) == 64);
            REQUIRE(is_oriented(mesh));
        }
        SECTION("hole size < 100")
        {
            options.max_hole_size = 100;
            close_small_holes(mesh, options);
            REQUIRE(count_boundary_edges(mesh) == 0);

            REQUIRE(mesh.has_attribute("texcoord"));
            auto& attr = mesh.get_indexed_attribute<Scalar>("texcoord");
            auto uv_values = matrix_view<Scalar>(attr.values());
            auto uv_indices = matrix_view<Index>(attr.indices());

            auto ref_mesh = lagrange::testing::load_surface_mesh<Scalar, Index>(
                "open/core/blub_open_filled.obj");
            REQUIRE(ref_mesh.has_attribute("texcoord"));
            auto& ref_attr = ref_mesh.get_indexed_attribute<Scalar>("texcoord");
            auto uv_ref_values = matrix_view<Scalar>(ref_attr.values());
            auto uv_ref_indices = matrix_view<Index>(ref_attr.indices());

            REQUIRE(uv_values.rows() == uv_ref_values.rows());
            REQUIRE(uv_indices.rows() == uv_ref_indices.rows());
            REQUIRE(uv_values.colwise().mean().isApprox(uv_ref_values.colwise().mean(), 1e-6));
            REQUIRE(is_oriented(mesh));
        }
    }

    SECTION("edge attribute")
    {
        auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/blub_open.obj");
        AttributeId uv_id = mesh.get_attribute_id("texcoord");
        auto seam_id = lagrange::compute_seam_edges(mesh, uv_id);
        auto seam_name = mesh.get_attribute_name(seam_id);
        REQUIRE(has_holes(mesh));

        CloseSmallHolesOptions options;
        options.max_hole_size = 100;
        close_small_holes(mesh, options);
        REQUIRE(count_boundary_edges(mesh) == 0);
        REQUIRE(is_oriented(mesh));
        REQUIRE(mesh.has_attribute(seam_name));

        auto seam_value = attribute_vector_view<uint8_t>(mesh, seam_name);
        REQUIRE(static_cast<Index>(seam_value.rows()) == mesh.get_num_edges());
        REQUIRE(seam_value.minCoeff() == 0);
        REQUIRE(seam_value.maxCoeff() == 1);
    }
}


TEST_CASE("close_small_holes (reused indices)", "[close_small_holes][surface][mesh_cleanup]")
{
    using Scalar = double;
    using Index = uint32_t;

    lagrange::SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertices(12);

    mesh.add_triangle(0, 1, 2);
    mesh.add_triangle(2, 3, 4);
    mesh.add_triangle(4, 5, 6);
    mesh.add_triangle(6, 7, 0);

    mesh.add_triangle(0, 7, 8);
    mesh.add_triangle(8, 1, 0);

    mesh.add_triangle(1, 9, 2);
    mesh.add_triangle(2, 9, 3);

    mesh.add_triangle(4, 3, 10);
    mesh.add_triangle(10, 5, 4);

    mesh.add_triangle(11, 6, 5);
    mesh.add_triangle(6, 11, 7);

    {
        auto id = mesh.create_attribute<Scalar>("attr", lagrange::AttributeElement::Indexed);
        auto& attr = mesh.ref_indexed_attribute<Scalar>(id);
        attr.values().insert_elements(13);
        auto I = lagrange::reshaped_ref<Index>(attr.indices(), 3);
        auto V = lagrange::vector_ref<Scalar>(attr.values());

        I.row(0) << 0, 3, 1;
        I.row(1) << 1, 4, 2;
        I.row(2) << 8, 5, 1;
        I.row(3) << 1, 6, 7;

        I.row(4) << 7, 6, 12;
        I.row(5) << 12, 3, 0;

        I.row(6) << 3, 9, 1;
        I.row(7) << 1, 9, 4;

        I.row(8) << 2, 4, 10;
        I.row(9) << 10, 5, 8;

        I.row(10) << 11, 1, 5;
        I.row(11) << 1, 11, 6;

        V << 2, 10, 3, 100, 100, 100, 100, 1, 4, 100, 100, 100, 100;
    }
    REQUIRE(count_boundary_edges(mesh) == 12);
    REQUIRE(mesh.get_num_facets() == 12);

    lagrange::CloseSmallHolesOptions options;
    options.max_hole_size = 4;
    options.triangulate_holes = false;
    close_small_holes(mesh, options);
    REQUIRE(count_boundary_edges(mesh) == 8);
    REQUIRE(mesh.get_num_facets() == 16);
    REQUIRE(mesh.get_num_vertices() == 13);

    {
        auto& attr = mesh.get_indexed_attribute<Scalar>("attr");
        auto I = lagrange::vector_view<Index>(attr.indices());
        auto V = lagrange::vector_view<Scalar>(attr.values());
        for (Index f = 12; f < 16; ++f) {
            auto facet = mesh.get_facet_vertices(f);
            Index c0 = mesh.get_facet_corner_begin(f);
            Index c_bary = c0;
            bool found = false;
            bool is_lower = false;
            bool is_upper = false;
            for (size_t lv = 0; lv < facet.size(); ++lv) {
                if (facet[lv] == 12) {
                    c_bary = c0 + Index(lv);
                    found = true;
                }
                if (facet[lv] == 2) {
                    is_lower = true;
                }
                if (facet[lv] == 6) {
                    is_upper = true;
                }
            }
            REQUIRE(found);
            REQUIRE((is_lower || is_upper));
            CAPTURE(V(I(c_bary)), I(c_bary));
            if (is_lower) {
                REQUIRE(V(I(c_bary)) == (2 + 3 + 10) / 3);
            } else {
                REQUIRE(V(I(c_bary)) == (1 + 4 + 10) / 3);
            }
        }
    }
}

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
    #include <lagrange/Mesh.h>
    #include <lagrange/MeshTrait.h>
    #include <lagrange/chain_edges_into_simple_loops.h>
    #include <lagrange/common.h>
    #include <lagrange/io/load_mesh.h>
    #include <lagrange/mesh_cleanup/close_small_holes.h>
    #include <lagrange/utils/warning.h>

    #include <lagrange/testing/common.h>

namespace {

template <typename MeshType>
int count_boundary_edges(MeshType& mesh)
{
    static_assert(lagrange::MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");
    using Index = typename MeshType::Index;

    mesh.initialize_edge_data();
    int num_boundary_edges = 0;
    for (Index e = 0; e < mesh.get_num_edges(); ++e) {
        if (mesh.is_boundary_edge(e)) {
            ++num_boundary_edges;
        }
    }
    return num_boundary_edges;
}

template <typename MeshType>
bool has_holes(MeshType& mesh)
{
    return count_boundary_edges(mesh) != 0;
}

} // namespace

LA_IGNORE_DEPRECATION_WARNING_BEGIN
TEST_CASE("legacy::chain_edges_into_simple_loops", "[cleanup][close_small_holes]")
{
    SECTION("graph_1")
    {
        Eigen::MatrixXi edges(4, 2);
        // clang-format off
        edges <<
            0, 1,
            1, 2,
            2, 0,
            2, 3;
        // clang-format on

        Eigen::MatrixXi remaining;
        std::vector<std::vector<int>> loops;
        bool all_loops = lagrange::chain_edges_into_simple_loops(edges, loops, remaining);
        REQUIRE(all_loops == false);

        Eigen::MatrixXi expected_remaining(1, 2);
        expected_remaining << 2, 3;
        const std::vector<std::vector<int>> expected_loops = {{2, 0, 1}};
        REQUIRE(loops == expected_loops);
        REQUIRE(remaining.size() == expected_remaining.size());
        REQUIRE(remaining == expected_remaining);
    }

    SECTION("graph_2")
    {
        Eigen::MatrixXi edges(4, 2);
        // clang-format off
        edges <<
            0, 1,
            1, 2,
            2, 3,
            3, 4;
        // clang-format on

        Eigen::MatrixXi remaining;
        std::vector<std::vector<int>> loops;
        bool all_loops = lagrange::chain_edges_into_simple_loops(edges, loops, remaining);
        REQUIRE(all_loops == false);

        Eigen::MatrixXi expected_remaining = edges;
        const std::vector<std::vector<int>> expected_loops = {};
        REQUIRE(loops == expected_loops);
        REQUIRE(remaining.size() == expected_remaining.size());
        REQUIRE(remaining == expected_remaining);
    }

    SECTION("graph_3")
    {
        Eigen::MatrixXi edges(7, 2);
        // clang-format off
        edges <<
            0, 1,
            1, 2,
            2, 0,
            2, 5,
            5, 3,
            3, 4,
            4, 5;
        // clang-format on

        Eigen::MatrixXi remaining;
        std::vector<std::vector<int>> loops;
        bool all_loops = lagrange::chain_edges_into_simple_loops(edges, loops, remaining);
        REQUIRE(all_loops == false);

        Eigen::MatrixXi expected_remaining(1, 2);
        expected_remaining << 2, 5;
        const std::vector<std::vector<int>> expected_loops = {{4, 5, 6}, {2, 0, 1}};
        REQUIRE(loops == expected_loops);
        REQUIRE(remaining.size() == expected_remaining.size());
        REQUIRE(remaining == expected_remaining);
    }

    SECTION("graph_4")
    {
        Eigen::MatrixXi edges(6, 2);
        // clang-format off
        edges <<
            0, 1,
            1, 0,
            1, 2,
            2, 1,
            2, 0,
            0, 2;
        // clang-format on

        Eigen::MatrixXi remaining;
        std::vector<std::vector<int>> loops;
        bool all_loops = lagrange::chain_edges_into_simple_loops(edges, loops, remaining);
        REQUIRE(all_loops == false);

        // This graph has no simple loop, so it cannot be simplified by pruning "ears".
        Eigen::MatrixXi expected_remaining = edges;
        const std::vector<std::vector<int>> expected_loops = {};
        REQUIRE(loops == expected_loops);
        REQUIRE(remaining.size() == expected_remaining.size());
        REQUIRE(remaining == expected_remaining);
    }
}
LA_IGNORE_DEPRECATION_WARNING_END

TEST_CASE("legacy::close_small_holes (simple)", "[cleanup][close_small_holes]" LA_SLOW_DEBUG_FLAG)
{
    auto mesh =
        lagrange::testing::load_mesh<lagrange::TriangleMesh3D>("open/core/stanford-bunny.obj");
    REQUIRE(mesh);

    REQUIRE(has_holes(*mesh));

    auto mesh1 = lagrange::close_small_holes(*mesh, 10);
    REQUIRE(has_holes(*mesh1));
    auto mesh2 = lagrange::close_small_holes(*mesh, 100);
    REQUIRE(!has_holes(*mesh2));
}

TEST_CASE("legacy::close_small_holes (complex)", "[cleanup][close_small_holes]")
{
    auto mesh = lagrange::testing::load_mesh<lagrange::TriangleMesh3D>("open/core/grid_holes.obj");
    REQUIRE(mesh);

    REQUIRE(has_holes(*mesh));
    lagrange::logger().info("num boundary edges: {}", count_boundary_edges(*mesh));

    auto mesh1 = lagrange::close_small_holes(*mesh, 20);
    REQUIRE(count_boundary_edges(*mesh1) == 8 * 8 + 15);
    auto mesh2 = lagrange::close_small_holes(*mesh, 100);
    REQUIRE(count_boundary_edges(*mesh2) == 15);
}

TEST_CASE("legacy::close_small_holes (non-manifold)", "[cleanup][close_small_holes]")
{
    auto mesh = lagrange::testing::load_mesh<lagrange::TriangleMesh3D>("open/core/prout.obj");
    REQUIRE(mesh);

    REQUIRE(has_holes(*mesh));
    lagrange::logger().info("num boundary edges: {}", count_boundary_edges(*mesh));
    REQUIRE(count_boundary_edges(*mesh) == 141);

    auto mesh1 = lagrange::close_small_holes(*mesh, 3);
    REQUIRE(count_boundary_edges(*mesh1) == 105);
}

TEST_CASE("legacy::close_small_holes (with uv)", "[cleanup][close_small_holes]")
{
    auto mesh = lagrange::testing::load_mesh<lagrange::TriangleMesh3D>("open/core/blub_open.obj");
    REQUIRE(mesh);

    REQUIRE(has_holes(*mesh));
    lagrange::logger().info("num boundary edges: {}", count_boundary_edges(*mesh));

    auto mesh1 = lagrange::close_small_holes(*mesh, 20);
    REQUIRE(count_boundary_edges(*mesh1) == 64);
    auto mesh2 = lagrange::close_small_holes(*mesh, 100);
    REQUIRE(count_boundary_edges(*mesh2) == 0);

    auto mesh0 =
        lagrange::testing::load_mesh<lagrange::TriangleMesh3D>("open/core/blub_open_filled.obj");
    REQUIRE(mesh2->get_vertices().isApprox(mesh0->get_vertices()));
    REQUIRE(mesh2->get_facets() == mesh0->get_facets());
    REQUIRE(mesh2->get_uv().isApprox(mesh0->get_uv()));
    REQUIRE(mesh2->get_uv_indices() == mesh0->get_uv_indices());
}

TEST_CASE(
    "legacy::close_small_holes (with attributes)",
    "[cleanup][close_small_holes]" LA_SLOW_DEBUG_FLAG)
{
    auto mesh = lagrange::testing::load_mesh<lagrange::TriangleMesh3D>("open/core/blub_open.obj");
    REQUIRE(mesh);

    REQUIRE(has_holes(*mesh));
    lagrange::logger().info("num boundary edges: {}", count_boundary_edges(*mesh));

    using AttributeArray = lagrange::TriangleMesh3D::AttributeArray;
    using Index = lagrange::TriangleMesh3D::Index;
    mesh->initialize_edge_data();

    srand(0);
    AttributeArray vertex_attr = AttributeArray::Random(mesh->get_num_vertices(), 4);
    AttributeArray facet_attr = AttributeArray::Random(mesh->get_num_facets(), 5);
    AttributeArray corner_attr = AttributeArray::Random(mesh->get_num_facets() * 3, 3);
    AttributeArray edge_new_attr = AttributeArray::Random(mesh->get_num_edges(), 1);
    mesh->add_vertex_attribute("color");
    mesh->set_vertex_attribute("color", vertex_attr);
    mesh->add_facet_attribute("normal");
    mesh->set_facet_attribute("normal", facet_attr);
    mesh->add_corner_attribute("kwak");
    mesh->set_corner_attribute("kwak", corner_attr);
    mesh->add_edge_attribute("bary");
    mesh->set_edge_attribute("bary", edge_new_attr);

    auto mesh1 = lagrange::close_small_holes(*mesh, 100);
    REQUIRE(count_boundary_edges(*mesh1) == 0);

    const auto& vertex_attr1 = mesh1->get_vertex_attribute("color");
    const auto& facet_attr1 = mesh1->get_facet_attribute("normal");
    const auto& corner_attr1 = mesh1->get_corner_attribute("kwak");
    const auto& edge_new_attr1 = mesh1->get_edge_attribute("bary");
    REQUIRE(vertex_attr == vertex_attr1.topRows(vertex_attr.rows()));
    REQUIRE(facet_attr == facet_attr1.topRows(facet_attr.rows()));
    REQUIRE(corner_attr == corner_attr1.topRows(corner_attr.rows()));

    // New edges might be indexed differently, so let's iterate over facet corners and compare
    // values
    bool all_same = true;
    for (Index f = 0; f < mesh->get_num_facets(); ++f) {
        for (Index lv = 0; lv < 3; ++lv) {
            auto e = mesh->get_edge(f, lv);
            auto e1 = mesh1->get_edge(f, lv);
            all_same &= (edge_new_attr.row(e) == edge_new_attr1.row(e1));
        }
    }
    REQUIRE(all_same);
}
#endif
