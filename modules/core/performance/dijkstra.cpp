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
#include <lagrange/compute_dijkstra_distance.h>
#include <lagrange/io/load_mesh.h>
#include <lagrange/utils/timing.h>
#include <Eigen/Core>

int main(int argc, char** argv)
{
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " mesh radius" << std::endl;
        return -1;
    }

    auto mesh = lagrange::io::load_mesh<lagrange::TriangleMesh3D>(argv[1]);
    const double radius = atof(argv[2]);
    // const size_t num_facets = mesh->get_num_facets();
    const int num_runs = 100;

    lagrange::timestamp_type t0, t1;
    lagrange::get_timestamp(&t0);
    for (int i = 0; i < num_runs; i++) {
        lagrange::compute_dijkstra_distance(*mesh, i, {0.3, 0.3, 0.4}, radius);
    }
    lagrange::get_timestamp(&t1);

    double total_time = lagrange::timestamp_diff_in_seconds(t0, t1);

    std::cout << "total time: " << total_time << " " << num_runs << " calls." << std::endl;
    std::cout << "  ave time: " << total_time / num_runs << std::endl;

    return 0;
}
