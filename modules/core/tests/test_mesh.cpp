/*
 * Copyright 2017 Adobe. All rights reserved.
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
#include <string>

#include <lagrange/Mesh.h>
#include <lagrange/common.h>
#include <lagrange/create_mesh.h>
#include <Eigen/Core>


TEST_CASE("MeshCreation", "[Mesh][Creation]")
{
    using namespace lagrange;

    Vertices3D vertices(3, 3);
    vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0;
    Triangles facets(1, 3);
    facets << 0, 1, 2;

    auto mesh = create_mesh(vertices, facets);
    using MeshType = typename decltype(mesh)::element_type;

    REQUIRE(mesh->get_dim() == 3);
    REQUIRE(mesh->get_vertex_per_facet() == 3);
    REQUIRE(mesh->get_num_vertices() == 3);
    REQUIRE(mesh->get_num_facets() == 1);

    SECTION("export vertices")
    {
        Vertices3D V;
        mesh->export_vertices(V);
        REQUIRE(V.rows() == vertices.rows());
        REQUIRE(V.cols() == vertices.cols());
        REQUIRE((vertices - V).maxCoeff() == 0.0);
        REQUIRE((vertices - V).minCoeff() == 0.0);
        REQUIRE(mesh->get_num_vertices() == 0);
    }

    SECTION("export facets")
    {
        Triangles F;
        mesh->export_facets(F);
        REQUIRE(F.rows() == facets.rows());
        REQUIRE(F.cols() == facets.cols());
        REQUIRE((facets - F).maxCoeff() == 0.0);
        REQUIRE((facets - F).minCoeff() == 0.0);
        REQUIRE(mesh->get_num_facets() == 0);
    }

    SECTION("edges_new")
    {
        if (!mesh->is_edge_data_initialized()) {
            mesh->initialize_edge_data();
        }

        const auto num_edges = mesh->get_num_edges();
        REQUIRE(3 == num_edges);

        for (MeshType::Index i = 0; i < num_edges; ++i) {
            REQUIRE(mesh->is_boundary_edge(i));
            REQUIRE(mesh->get_num_facets_around_edge(i) == 1);
        }
    }

    SECTION("vertex attributes")
    {
        using AttributeArray = MeshType::AttributeArray;
        const std::string attr_name = "tmp";
        AttributeArray attr(3, 2);
        attr << 1, 2, 3, 4, 5, 6;
        mesh->add_vertex_attribute(attr_name);
        REQUIRE(mesh->has_vertex_attribute(attr_name));
        REQUIRE(!mesh->has_facet_attribute(attr_name));
        REQUIRE(!mesh->has_corner_attribute(attr_name));

        mesh->set_vertex_attribute(attr_name, attr);
        const auto& attr2 = mesh->get_vertex_attribute(attr_name);
        REQUIRE(attr.rows() == attr2.rows());
        REQUIRE(attr.cols() == attr2.cols());
        auto diff = (attr - attr2).norm();
        REQUIRE(diff == 0.0); // Should be exactly 0.

        mesh->remove_vertex_attribute(attr_name);
        REQUIRE(!mesh->has_vertex_attribute(attr_name));
    }

    SECTION("facet attributes")
    {
        using AttributeArray = MeshType::AttributeArray;
        const std::string attr_name = "tmp";
        AttributeArray attr(1, 2);
        attr << 1, 2;
        mesh->add_facet_attribute(attr_name);
        REQUIRE(!mesh->has_vertex_attribute(attr_name));
        REQUIRE(mesh->has_facet_attribute(attr_name));
        REQUIRE(!mesh->has_corner_attribute(attr_name));

        mesh->set_facet_attribute(attr_name, attr);
        const auto& attr2 = mesh->get_facet_attribute(attr_name);
        REQUIRE(attr.rows() == attr2.rows());
        REQUIRE(attr.cols() == attr2.cols());
        auto diff = (attr - attr2).norm();
        REQUIRE(diff == 0.0); // Should be exactly 0.

        mesh->remove_facet_attribute(attr_name);
        REQUIRE(!mesh->has_facet_attribute(attr_name));
    }

    SECTION("corner attributes")
    {
        using AttributeArray = MeshType::AttributeArray;
        const std::string attr_name = "tmp";
        AttributeArray attr(3, 2);
        attr << 1, 2, 3, 4, 5, 6;
        mesh->add_corner_attribute(attr_name);
        REQUIRE(!mesh->has_vertex_attribute(attr_name));
        REQUIRE(!mesh->has_facet_attribute(attr_name));
        REQUIRE(mesh->has_corner_attribute(attr_name));

        mesh->set_corner_attribute(attr_name, attr);
        const auto& attr2 = mesh->get_corner_attribute(attr_name);
        REQUIRE(attr.rows() == attr2.rows());
        REQUIRE(attr.cols() == attr2.cols());
        auto diff = (attr - attr2).norm();
        REQUIRE(diff == 0.0); // Should be exactly 0.

        mesh->remove_corner_attribute(attr_name);
        REQUIRE(!mesh->has_corner_attribute(attr_name));
    }

    SECTION("edge attributes")
    {
        using AttributeArray = MeshType::AttributeArray;
        const std::string attr_name = "L2_norm";
        if (!mesh->is_edge_data_initialized()) {
            mesh->initialize_edge_data();
        }
        REQUIRE(!mesh->has_edge_attribute(attr_name));
        REQUIRE(mesh->is_edge_data_initialized());
        const auto num_edges = mesh->get_num_edges();
        REQUIRE(3 == num_edges);
        AttributeArray attr = AttributeArray::Zero(num_edges, 1);
        for (auto i : range(mesh->get_num_edges())) {
            auto e = mesh->get_edge_vertices(i);
            attr(i, 0) = (vertices.row(e[0]) - vertices.row(e[1])).norm();
            REQUIRE(attr(i, 0) > 0);
        }
        mesh->add_edge_attribute(attr_name);
        mesh->set_edge_attribute(attr_name, attr);
        REQUIRE(mesh->has_edge_attribute(attr_name));

        const auto& attr2 = mesh->get_edge_attribute(attr_name);
        REQUIRE(attr.rows() == attr2.rows());
        REQUIRE(attr.cols() == attr2.cols());
        auto diff = (attr - attr2).norm();
        REQUIRE(diff == 0.0); // Should be exactly 0.

        mesh->remove_edge_attribute(attr_name);
        REQUIRE(!mesh->has_edge_attribute(attr_name));
    }
}

TEST_CASE("MeshCopy", "[Mesh][Import][Export]")
{
    using namespace lagrange;
    Eigen::Matrix<float, Eigen::Dynamic, 3, Eigen::RowMajor> vertices(3, 3);
    Eigen::Matrix<int, Eigen::Dynamic, 3, Eigen::RowMajor> facets(1, 3);

    void* vertices_ptr = vertices.data();
    void* facets_ptr = facets.data();

    auto mesh = create_mesh(std::move(vertices), std::move(facets));
    REQUIRE(mesh->get_vertices().data() == vertices_ptr);
    REQUIRE(mesh->get_facets().data() == facets_ptr);

    SECTION("import vertices")
    {
        // Importing vertices invokes no data copy.
        Eigen::Matrix<float, Eigen::Dynamic, 3, Eigen::RowMajor> new_vertices(10, 3);
        void* new_vertices_ptr = new_vertices.data();
        mesh->import_vertices(new_vertices);

        const auto& V = mesh->get_vertices();
        REQUIRE(V.data() == new_vertices_ptr);

        const auto V1 = mesh->get_vertices();
        REQUIRE(V1.data() != new_vertices_ptr);
    }

    SECTION("import facets")
    {
        // Importing facets invokes no data copy.
        Eigen::Matrix<int, Eigen::Dynamic, 3, Eigen::RowMajor> new_facets(10, 3);
        void* new_facets_ptr = new_facets.data();
        mesh->import_facets(new_facets);

        const auto& F = mesh->get_facets();
        REQUIRE(F.data() == new_facets_ptr);

        const auto F1 = mesh->get_facets();
        REQUIRE(F1.data() != new_facets_ptr);
    }

    SECTION("export vertices")
    {
        Eigen::Matrix<float, Eigen::Dynamic, 3, Eigen::RowMajor> V;
        mesh->export_vertices(V);
        REQUIRE(V.data() == vertices_ptr);
    }

    SECTION("export facets")
    {
        Eigen::Matrix<int, Eigen::Dynamic, 3, Eigen::RowMajor> F;
        mesh->export_facets(F);
        REQUIRE(F.data() == facets_ptr);
    }
}

TEST_CASE("MeshCreation2", "[Mesh][Creation][RowMajor]")
{
    using namespace lagrange;
    Eigen::Matrix<double, Eigen::Dynamic, 3, Eigen::RowMajor> vertices(3, 3);
    vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0;
    Eigen::Matrix<int, Eigen::Dynamic, 3, Eigen::RowMajor> facets(1, 3);
    facets << 0, 1, 2;

    auto mesh = create_mesh(vertices, facets);
    REQUIRE(mesh->get_dim() == 3);
    REQUIRE(mesh->get_vertex_per_facet() == 3);
    REQUIRE(mesh->get_num_vertices() == 3);
    REQUIRE(mesh->get_num_facets() == 1);
}

TEST_CASE("MeshWrapper", "[Mesh][Wrapper][RowMajor]")
{
    using namespace lagrange;
    Eigen::Matrix<double, Eigen::Dynamic, 3, Eigen::RowMajor> vertices(3, 3);
    vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0;
    Eigen::Matrix<int, Eigen::Dynamic, 3, Eigen::RowMajor> facets(1, 3);
    facets << 0, 1, 2;

    auto mesh = wrap_with_mesh(vertices, facets);
    REQUIRE(mesh->get_dim() == 3);
    REQUIRE(mesh->get_vertex_per_facet() == 3);
    REQUIRE(mesh->get_num_vertices() == 3);
    REQUIRE(mesh->get_num_facets() == 1);
}

TEST_CASE("ConnectivityInit", "[Mesh][Connectivity]")
{
    using namespace lagrange;

    SECTION("artificial_example")
    {
        Eigen::Matrix<double, Eigen::Dynamic, 3, Eigen::RowMajor> vertices(3, 3);
        vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0;
        Eigen::Matrix<int, Eigen::Dynamic, 3, Eigen::RowMajor> facets(1, 3);
        facets << 0, 1, 2;

        auto mesh = wrap_with_mesh(vertices, facets);
        mesh->initialize_topology();
        REQUIRE(mesh->is_vertex_manifold());
    }

    SECTION("artificial_example_2")
    {
        Eigen::Matrix<double, Eigen::Dynamic, 3, Eigen::RowMajor> vertices(3, 3);
        vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0;
        Eigen::Matrix<int, Eigen::Dynamic, 3, Eigen::RowMajor> facets(2, 3);
        facets << 0, 1, 2, 2, 1, 0;

        auto mesh = wrap_with_mesh(vertices, facets);
        mesh->initialize_topology();
        REQUIRE(mesh->is_edge_manifold());
    }

    SECTION("artificial_example_3")
    {
        Eigen::Matrix<double, Eigen::Dynamic, 3, Eigen::RowMajor> vertices(3, 3);
        vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0;
        Eigen::Matrix<int, Eigen::Dynamic, 3, Eigen::RowMajor> facets(3, 3);
        facets << 0, 1, 2, 2, 1, 0, 0, 1, 2;

        auto mesh = wrap_with_mesh(vertices, facets);
        mesh->initialize_topology();
        REQUIRE(!mesh->is_vertex_manifold());
    }
}

TEST_CASE("ConnectivityInit_slow", "[Mesh][Connectivity]" LA_SLOW_FLAG LA_CORP_FLAG)
{
    using namespace lagrange;

    SECTION("wing")
    {
        auto mesh = lagrange::testing::load_mesh<TriangleMesh3D>("corp/core/wing.obj");
        mesh->initialize_topology();

        REQUIRE(mesh->is_vertex_manifold());
    }

    SECTION("splash")
    {
        auto mesh = lagrange::testing::load_mesh<TriangleMesh3D>("corp/core/splash_08_debug.obj");
        mesh->initialize_topology();

        REQUIRE(!mesh->is_vertex_manifold());
    }
}
