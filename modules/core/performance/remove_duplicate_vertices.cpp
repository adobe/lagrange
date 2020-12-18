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
#include <lagrange/io/load_mesh.h>
#include <lagrange/io/save_mesh.h>
#include <lagrange/mesh_cleanup/remove_duplicate_vertices.h>
#include <iostream>

int main(int argc, char** argv)
{
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " input_mesh output_mesh" << std::endl;
        return 1;
    }

    using MeshType = lagrange::TriangleMesh3D;
    using Index = MeshType::Index;
    auto mesh = lagrange::io::load_mesh<MeshType>(argv[1]);
    const auto num_vertices = mesh->get_num_vertices();
    const auto num_facets = mesh->get_num_facets();
    const auto& facets = mesh->get_facets();

    if (mesh->is_uv_initialized()) {
        const auto& uv = mesh->get_uv();
        const auto& uv_indices = mesh->get_uv_indices();
        MeshType::AttributeArray per_vertex_uv(num_vertices, 2);
        for (Index i = 0; i < num_facets; i++) {
            per_vertex_uv.row(facets(i, 0)) = uv.row(uv_indices(i, 0));
            per_vertex_uv.row(facets(i, 1)) = uv.row(uv_indices(i, 1));
            per_vertex_uv.row(facets(i, 2)) = uv.row(uv_indices(i, 2));
        }
        mesh->add_vertex_attribute("uv");
        mesh->set_vertex_attribute("uv", per_vertex_uv);
        mesh = lagrange::remove_duplicate_vertices(*mesh, "uv");
    } else {
        mesh = lagrange::remove_duplicate_vertices(*mesh);
    }

    lagrange::io::save_mesh(argv[2], *mesh);
    return 0;
}
