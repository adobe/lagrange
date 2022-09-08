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
#include <lagrange/bvh/project_attributes_closest_vertex.h>
#include <lagrange/compute_edge_lengths.h>
#include <lagrange/io/load_mesh.h>
#include <lagrange/testing/common.h>
#include <lagrange/create_mesh.h>

#include <random>

std::unique_ptr<lagrange::TriangleMesh3D> create_perturbed_mesh(lagrange::TriangleMesh3D &mesh)
{
    compute_edge_lengths(mesh);

    double min_len = mesh.get_edge_attribute("length").minCoeff();

    std::mt19937 gen;
    std::uniform_real_distribution<double> dist(0.0, 0.1 * min_len);

    lagrange::Vertices3D vertices = mesh.get_vertices();
    for (int i = 0; i < mesh.get_num_vertices(); ++i) {
        vertices.row(i) += Eigen::RowVector3d(dist(gen), dist(gen), dist(gen));
    }

    return lagrange::create_mesh(vertices, mesh.get_facets());
}

TEST_CASE("project_attributes_closest_vertex", "[bvh]")
{
    auto mesh = lagrange::testing::load_mesh<lagrange::TriangleMesh3D>("open/core/bunny_simple.obj");
    auto mesh2 = create_perturbed_mesh(*mesh);

    {
        lagrange::TriangleMesh3D::AttributeArray id(mesh->get_num_vertices(), 1);
        for (int i = 0; i < mesh->get_num_vertices(); ++i) {
            id(i, 0) = i; // Argh, why??
        }
        mesh->add_vertex_attribute("id");
        mesh->import_vertex_attribute("id", id);
    }

    REQUIRE(mesh->get_vertices() != mesh2->get_vertices());
    REQUIRE(mesh2->has_vertex_attribute("id") == false);
    lagrange::bvh::project_attributes_closest_vertex(*mesh, *mesh2, {"id"});
    REQUIRE(mesh2->has_vertex_attribute("id"));

    {
        auto id = mesh->get_vertex_attribute("id");
        auto id2 = mesh2->get_vertex_attribute("id");
        REQUIRE(id == id2);
    }
}
