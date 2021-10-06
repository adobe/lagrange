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
#include <iostream>

#include <lagrange/Mesh.h>
#include <lagrange/io/load_mesh.h>
#include <lagrange/utils/timing.h>
#include <lagrange/create_mesh.h>

using namespace lagrange;

int main(int argc, char** argv)
{
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " input_mesh" << std::endl;
        return 1;
    }

    // load the mesh. Once. I/O not part of the test.
    auto mesh = io::load_mesh<lagrange::TriangleMesh3D>(argv[1]);

    const int n = 1000;
    timestamp_type start, end;
    double total_time;
    std::cout << "Creating each mesh " << n << " times." << std::endl;
    std::cout << "The mesh has " << mesh->get_num_vertices() << " vertices and "
              << mesh->get_num_facets() << " faces." << std::endl
              << std::endl;

    // 1. Create mesh, copying V and F
    start = get_timestamp();
    for (int i = 0; i < n; ++i) {
        // note: this involves a copy of vertices and facets
        auto mesh2 = create_mesh(mesh->get_vertices(), mesh->get_facets());
    }
    end = get_timestamp();
    total_time = lagrange::timestamp_diff_in_seconds(start, end);
    std::cout << "Simple mesh creation: " << total_time << " s" << std::endl;

    // 2. Create mesh, wrapping, so not copying
    start = get_timestamp();
    for (int i = 0; i < n; ++i) {
        // note: this involves a copy of vertices and facets
        auto mesh2 = wrap_with_mesh(mesh->get_vertices(), mesh->get_facets());
    }
    end = get_timestamp();
    total_time = lagrange::timestamp_diff_in_seconds(start, end);
    std::cout << "Wrap mesh creation: " << total_time << " s" << std::endl;

    // 3. Create mesh and initialize, copying V and F
    start = get_timestamp();
    for (int i = 0; i < n; ++i) {
        // note: this involves a copy of vertices and facets
        auto mesh2 = create_mesh(mesh->get_vertices(), mesh->get_facets());
        mesh2->initialize_connectivity();
        mesh2->initialize_components();
        mesh2->initialize_edge_data();
        mesh2->initialize_topology();
    }
    end = get_timestamp();
    total_time = lagrange::timestamp_diff_in_seconds(start, end);
    std::cout << "Simple mesh creation + init: " << total_time << " s" << std::endl;

    // 4. Create mesh and initialize, wrapping, so not copying
    start = get_timestamp();
    for (int i = 0; i < n; ++i) {
        // note: this involves a copy of vertices and facets
        auto mesh2 = wrap_with_mesh(mesh->get_vertices(), mesh->get_facets());
        mesh2->initialize_connectivity();
        mesh2->initialize_components();
        mesh2->initialize_edge_data();
        mesh2->initialize_topology();
    }
    end = get_timestamp();
    total_time = lagrange::timestamp_diff_in_seconds(start, end);
    std::cout << "Wrap mesh creation + init: " << total_time << " s" << std::endl;
    std::cout << std::endl;

    // 5. measure each component
    double mesh_time = 0;
    double connectivity_time = 0;
    double topology_time = 0;
    double components_time = 0;
    double edges_time = 0;
    for (int i = 0; i < n; ++i) {
        start = get_timestamp();
        auto mesh2 = create_mesh(mesh->get_vertices(), mesh->get_facets());
        end = get_timestamp();
        mesh_time += timestamp_diff_in_seconds(start, end);

        start = get_timestamp();
        mesh2->initialize_connectivity();
        end = get_timestamp();
        connectivity_time += timestamp_diff_in_seconds(start, end);

        start = get_timestamp();
        mesh2->initialize_components();
        end = get_timestamp();
        components_time += timestamp_diff_in_seconds(start, end);

        start = get_timestamp();
        mesh2->initialize_edge_data();
        end = get_timestamp();
        edges_time += timestamp_diff_in_seconds(start, end);

        start = get_timestamp();
        mesh2->initialize_topology();
        end = get_timestamp();
        topology_time += timestamp_diff_in_seconds(start, end);
    }
    std::cout << "Timing by task:" << std::endl;
    std::cout << "  create_mesh: " << mesh_time << std::endl;
    std::cout << "  initialize_connectivity: " << connectivity_time << std::endl;
    std::cout << "  initialize_topology: " << topology_time << std::endl;
    std::cout << "  initialize_components: " << components_time << std::endl;
    std::cout << "  initialize_edge_data: " << edges_time << std::endl;
}
