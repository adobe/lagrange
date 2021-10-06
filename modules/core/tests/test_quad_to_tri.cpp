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
#include <lagrange/common.h>
#include <lagrange/compute_facet_area.h>
#include <lagrange/io/load_mesh.impl.h>
#include <lagrange/quad_to_tri.h>
#include <Eigen/Core>
#include <lagrange/testing/common.h>


namespace lagrange_test_internal {

template <typename MeshType1, typename MeshType2>
void assert_same_area(MeshType1& mesh1, MeshType2& mesh2)
{
    compute_facet_area(mesh1);
    compute_facet_area(mesh2);

    const auto& area1 = mesh1.get_facet_attribute("area");
    const auto& area2 = mesh2.get_facet_attribute("area");

    REQUIRE(area1.sum() == Approx(area2.sum()));

    if (mesh1.is_uv_initialized() && mesh2.is_uv_initialized()) {
        const auto uv1 = mesh1.get_uv_mesh();
        const auto uv2 = mesh2.get_uv_mesh();

        compute_facet_area(*uv1);
        compute_facet_area(*uv2);
        const auto& uv_area1 = uv1->get_facet_attribute("area");
        const auto& uv_area2 = uv2->get_facet_attribute("area");

        REQUIRE(uv_area1.sum() == Approx(uv_area2.sum()));
    }
}

} // namespace lagrange_test_internal

TEST_CASE("quad_to_tri juicebox", "[mesh][quad_to_tri][juicebox][.slow]")
{
    using namespace lagrange;

    auto mesh = lagrange::testing::load_mesh<QuadMesh3D>("corp/core/juicebox.obj");
    REQUIRE(mesh->get_vertex_per_facet() == 4);
    auto tri_mesh = quad_to_tri(*mesh);
    REQUIRE(tri_mesh->get_vertex_per_facet() == 3);

    mesh->initialize_components();
    tri_mesh->initialize_components();
    REQUIRE(mesh->get_num_components() == tri_mesh->get_num_components());

    lagrange_test_internal::assert_same_area(*mesh, *tri_mesh);
}

TEST_CASE("quad_to_tri banner_single", "[mesh][quad_to_tri][banner_single][.slow]")
{
    using namespace lagrange;

    auto mesh = lagrange::testing::load_mesh<QuadMesh3D>("corp/core/banner_single.obj");
    REQUIRE(mesh->get_vertex_per_facet() == 4);
    auto tri_mesh = quad_to_tri(*mesh);
    REQUIRE(tri_mesh->get_vertex_per_facet() == 3);

    mesh->initialize_components();
    tri_mesh->initialize_components();
    REQUIRE(mesh->get_num_components() == tri_mesh->get_num_components());

    lagrange_test_internal::assert_same_area(*mesh, *tri_mesh);
}

TEST_CASE("quad_to_tri attribute", "[mesh][quad_to_tri][attribute]")
{
    using namespace lagrange;
    Eigen::Matrix<float, Eigen::Dynamic, 3> vertices(8, 3);
    vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 1.0, 0.0,
        1.0, 1.0, 1.0, 1.0, 0.0, 1.0, 1.0;

    Eigen::Matrix<int, Eigen::Dynamic, 4> facets(6, 4);
    facets << 3, 2, 1, 0, 4, 5, 6, 7, 0, 1, 5, 4, 1, 2, 6, 5, 7, 6, 2, 3, 4, 7, 3, 0;

    auto cube = wrap_with_mesh(vertices, facets);

    using MeshType = decltype(cube)::element_type;
    using AttributeArray = MeshType::AttributeArray;
    using Scalar = typename MeshType::Scalar;
    using Index = typename MeshType::Index;

    AttributeArray vertex_indices(8, 1);
    AttributeArray facet_indices(6, 1);
    AttributeArray corner_indices(24, 1);
    for (int i = 0; i < 8; i++) {
        vertex_indices(i, 0) = safe_cast<Scalar>(i);
    }
    for (int i = 0; i < 6; i++) {
        facet_indices(i, 0) = safe_cast<Scalar>(i);
    }
    for (int i = 0; i < 24; i++) {
        corner_indices(i, 0) = safe_cast<Scalar>(i);
    }

    cube->add_vertex_attribute("index");
    cube->set_vertex_attribute("index", vertex_indices);
    cube->add_facet_attribute("index");
    cube->set_facet_attribute("index", facet_indices);
    cube->add_corner_attribute("index");
    cube->set_corner_attribute("index", corner_indices);

    auto tri_cube = quad_to_tri(*cube);

    SECTION("vertex_attribute")
    {
        REQUIRE(tri_cube->has_vertex_attribute("index"));
        const auto& index = tri_cube->get_vertex_attribute("index");
        for (auto i : range(tri_cube->get_num_vertices())) {
            const auto& tri_vi = tri_cube->get_vertices().row(i);
            const auto v_idx = safe_cast<Index>(index(i, 0));
            const auto& cube_vi = cube->get_vertices().row(v_idx);
            REQUIRE((tri_vi - cube_vi).norm() == Approx(0.0));
        }
    }

    SECTION("facet_index")
    {
        REQUIRE(tri_cube->has_facet_attribute("index"));
        const auto& index = tri_cube->get_facet_attribute("index");
        std::vector<int> counter(6, 0);
        for (auto i : range(tri_cube->get_num_facets())) {
            counter[safe_cast<int>(index(i, 0))]++;
        }
        for (int i = 0; i < 6; i++) {
            REQUIRE(counter[i] == 2);
        }
    }

    SECTION("corner_index")
    {
        REQUIRE(tri_cube->has_corner_attribute("index"));
        const auto& index = tri_cube->get_corner_attribute("index").cast<int>();
        const auto& tri_facets = tri_cube->get_facets();
        for (int i = 0; i < 36; i++) {
            const auto& tri_vi = tri_cube->get_vertices().row(tri_facets(i / 3, i % 3));
            const auto& cube_vi =
                cube->get_vertices().row(facets(index(i, 0) / 4, index(i, 0) % 4));
            REQUIRE((tri_vi - cube_vi).norm() == Approx(0.0));
        }
    }
}
