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
#include <lagrange/testing/common.h>
#include <cmath>

#include <lagrange/Mesh.h>
#include <lagrange/common.h>
#include <lagrange/compute_normal.h>
#include <lagrange/compute_vertex_normal.h>
#include <lagrange/create_mesh.h>
#include <lagrange/utils/geometry3d.h>



TEST_CASE("compute_normal", "[mesh][attribute][normal]")
{
    using namespace lagrange;

    SECTION("Cube")
    {
        auto mesh = create_cube();
        using MeshType = typename decltype(mesh)::element_type;

        SECTION("Keep edge sharp")
        {
            compute_normal(*mesh, M_PI * 0.25);
            REQUIRE(mesh->has_indexed_attribute("normal"));

            MeshType::AttributeArray normal_values;
            MeshType::FacetArray normal_indices;
            std::tie(normal_values, normal_indices) = mesh->get_indexed_attribute("normal");

            REQUIRE(normal_values.rows() == 24);
            REQUIRE(normal_values.cols() == 3);
            REQUIRE(normal_indices.rows() == mesh->get_num_facets());
            REQUIRE(normal_indices.cols() == mesh->get_dim());

            auto normal_mesh = wrap_with_mesh(normal_values, normal_indices);
            normal_mesh->initialize_components();
            REQUIRE(normal_mesh->get_num_components() == 6);
        }

        SECTION("Smooth edge")
        {
            compute_normal(*mesh, M_PI);
            REQUIRE(mesh->has_indexed_attribute("normal"));

            MeshType::AttributeArray normal_values;
            MeshType::FacetArray normal_indices;
            std::tie(normal_values, normal_indices) = mesh->get_indexed_attribute("normal");

            REQUIRE(normal_values.rows() == 8);
            REQUIRE(normal_values.cols() == 3);
            REQUIRE(normal_indices.rows() == mesh->get_num_facets());
            REQUIRE(normal_indices.cols() == mesh->get_dim());

            auto normal_mesh = wrap_with_mesh(normal_values, normal_indices);
            normal_mesh->initialize_components();
            REQUIRE(normal_mesh->get_num_components() == 1);
        }
    }

    SECTION("Pyramid")
    {
        Eigen::Matrix<double, Eigen::Dynamic, 3> vertices(5, 3);
        vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0, 1.0, 0.0, 0.5, 0.5, 1.0;
        Eigen::Matrix<uint64_t, Eigen::Dynamic, 3> facets(6, 3);
        facets << 0, 1, 4, 1, 2, 4, 2, 3, 4, 3, 0, 4, 0, 2, 1, 0, 3, 2;

        auto mesh = wrap_with_mesh(vertices, facets);
        using MeshType = typename decltype(mesh)::element_type;

        SECTION("No cone vertices")
        {
            compute_normal(*mesh, M_PI * 0.5 - 0.1);
            REQUIRE(mesh->has_indexed_attribute("normal"));

            MeshType::AttributeArray normal_values;
            MeshType::FacetArray normal_indices;
            std::tie(normal_values, normal_indices) = mesh->get_indexed_attribute("normal");

            const Eigen::Matrix<double, 1, 3> up_dir(0, 0, 1);
            REQUIRE((normal_values.row(normal_indices(0, 2)) - up_dir).norm() == Approx(0.0));
            REQUIRE((normal_values.row(normal_indices(1, 2)) - up_dir).norm() == Approx(0.0));
            REQUIRE((normal_values.row(normal_indices(2, 2)) - up_dir).norm() == Approx(0.0));
            REQUIRE((normal_values.row(normal_indices(3, 2)) - up_dir).norm() == Approx(0.0));
        }

        SECTION("With cone vertices")
        {
            compute_normal(*mesh, M_PI * 0.5 - 0.1, {4});
            REQUIRE(mesh->has_indexed_attribute("normal"));

            MeshType::AttributeArray normal_values;
            MeshType::FacetArray normal_indices;
            std::tie(normal_values, normal_indices) = mesh->get_indexed_attribute("normal");

            const Eigen::Matrix<double, 1, 3> up_dir(0, 0, 1);
            REQUIRE((normal_values.row(normal_indices(0, 2)) - up_dir).norm() > 0.5);
            REQUIRE((normal_values.row(normal_indices(1, 2)) - up_dir).norm() > 0.5);
            REQUIRE((normal_values.row(normal_indices(2, 2)) - up_dir).norm() > 0.5);
            REQUIRE((normal_values.row(normal_indices(3, 2)) - up_dir).norm() > 0.5);
        }
    }

    SECTION("Blub")
    {
        using MeshType = TriangleMesh3D;
        auto mesh = lagrange::testing::load_mesh<MeshType>("open/core/blub/blub.obj");
        REQUIRE(mesh->get_num_vertices() == 7106);
        REQUIRE(mesh->get_num_facets() == 14208);

        compute_normal(*mesh, M_PI * 0.25);
        REQUIRE(mesh->has_indexed_attribute("normal"));

        compute_triangle_normal(*mesh);
        REQUIRE(mesh->has_facet_attribute("normal"));

        MeshType::AttributeArray normal_values;
        MeshType::FacetArray normal_indices;
        std::tie(normal_values, normal_indices) = mesh->get_indexed_attribute("normal");

        const auto& triangle_normals = mesh->get_facet_attribute("normal");

        const auto num_facets = mesh->get_num_facets();
        REQUIRE(normal_indices.rows() == num_facets);

        for (auto i : range(num_facets)) {
            for (auto j : {0, 1, 2}) {
                Eigen::Vector3d c_normal = normal_values.row(normal_indices(i, j));
                Eigen::Vector3d v_normal = triangle_normals.row(i);
                auto theta = angle_between(c_normal, v_normal);
                REQUIRE(theta < M_PI * 0.5);
            }
        }
    }

    SECTION("Degenerate")
    {
        Eigen::Matrix<double, Eigen::Dynamic, 3> vertices(2, 3);
        vertices << 0.1, 1.0, 0.5, 0.9, 0.3, 0.7;
        Eigen::Matrix<uint64_t, Eigen::Dynamic, 3> facets(2, 3);
        facets << 0, 1, 1, 1, 1, 1;

        auto mesh = wrap_with_mesh(vertices, facets);
        using MeshType = typename decltype(mesh)::element_type;

        compute_normal(*mesh, M_PI * 0.25);
        REQUIRE(mesh->has_indexed_attribute("normal"));

        compute_triangle_normal(*mesh);
        REQUIRE(mesh->has_facet_attribute("normal"));

        MeshType::AttributeArray normal_values;
        MeshType::FacetArray normal_indices;
        std::tie(normal_values, normal_indices) = mesh->get_indexed_attribute("normal");

        const auto& triangle_normals = mesh->get_facet_attribute("normal");

        REQUIRE(normal_values.isZero(0));
        REQUIRE(triangle_normals.isZero(0));
    }
}
