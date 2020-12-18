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
#include <lagrange/mesh_cleanup/split_long_edges.h>
#include <iostream>

int main(int argc, char** argv)
{
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " tol input_mesh output_mesh" << std::endl;
        return 1;
    }

    double tol = atof(argv[1]);

    lagrange::TriangleMesh3D::VertexArray vertices;
    lagrange::TriangleMesh3D::FacetArray facets;
    auto mesh = lagrange::io::load_mesh<lagrange::TriangleMesh3D>(argv[2]);
    mesh = lagrange::split_long_edges(*mesh, tol * tol, true);
    lagrange::io::save_mesh(argv[3], *mesh);
    return 0;
}
