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
#include <iostream>

#include <lagrange/Mesh.h>
#include <lagrange/common.h>
#include <lagrange/io/load_mesh.h>
#include <lagrange/utils/timing.h>

int main(int argc, char** argv)
{
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " input_mesh" << std::endl;
        return 1;
    }

    auto mesh = lagrange::io::load_mesh<lagrange::TriangleMesh3D>(argv[1]);

    lagrange::timestamp_type start, finish;
    lagrange::get_timestamp(&start);
    mesh->initialize_connectivity();
    lagrange::get_timestamp(&finish);
    double duration = lagrange::timestamp_diff_in_seconds(start, finish);

    lagrange::logger().info("Connectivity computation: {}s", duration);

    return 0;
}
