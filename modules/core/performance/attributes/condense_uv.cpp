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
#include <lagrange/Logger.h>
#include <lagrange/Mesh.h>
#include <lagrange/attributes/condense_indexed_attribute.h>
#include <lagrange/common.h>
#include <lagrange/io/load_mesh.h>
#include <lagrange/io/save_mesh.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/timing.h>

int main(int argc, char** argv)
{
    spdlog::set_level(spdlog::level::trace);
    spdlog::flush_every(std::chrono::seconds(3));
    if (argc != 3) {
        lagrange::logger().error("Usage: {} input_mesh, output_mesh", argv[0]);
        return -1;
    }

    using MeshType = lagrange::TriangleMesh3D;
    auto mesh = lagrange::io::load_mesh<MeshType>(argv[1]);

    if (!mesh->is_uv_initialized()) {
        lagrange::logger().error("Mesh does not contain UV field.  Nothing to do.");
        return 0;
    }

    lagrange::logger().info("Before condensing # UVs: {}", mesh->get_uv().rows());

    auto start_time = lagrange::get_timestamp();
    lagrange::condense_indexed_attribute(*mesh, "uv");
    auto finish_time = lagrange::get_timestamp();
    auto timing = lagrange::timestamp_diff_in_seconds(start_time, finish_time);

    lagrange::logger().info("After condensing # UVs: {}", mesh->get_uv().rows());
    lagrange::logger().info("Ave running time: {}", timing);

    lagrange::io::save_mesh(argv[2], *mesh);
    return 0;
}
