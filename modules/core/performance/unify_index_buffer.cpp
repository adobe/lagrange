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
#include <lagrange/attributes/condense_indexed_attribute.h>
#include <lagrange/attributes/unify_index_buffer.h>
#include <lagrange/common.h>
#include <lagrange/compute_normal.h>
#include <lagrange/internal/constants.h>
#include <lagrange/io/load_mesh.h>
#include <lagrange/io/save_mesh.h>
#include <lagrange/utils/timing.h>

std::unique_ptr<lagrange::TriangleMesh3D> import_mesh(const std::string& filename)
{
    using MeshType = lagrange::TriangleMesh3D;
    auto mesh = lagrange::io::load_mesh<MeshType>(filename);
    lagrange::logger().info("=== Before index unification ===");
    lagrange::logger().info("      # vertices: {}", mesh->get_num_vertices());
    lagrange::logger().info("         # faces: {}", mesh->get_num_facets());
    if (mesh->is_uv_initialized()) {
        lagrange::condense_indexed_attribute(*mesh, "uv");
        lagrange::logger().info("     # UV coords: {}", mesh->get_uv().rows());
    }
    {
        lagrange::compute_normal(*mesh, lagrange::internal::pi * 0.5);
        la_runtime_assert(mesh->has_indexed_attribute("normal"));
        lagrange::logger().info(
            "# normals values: {}",
            std::get<0>(mesh->get_indexed_attribute("normal")).rows());
    }

    return mesh;
}

void export_mesh(const std::string& filename, const lagrange::TriangleMesh3D& mesh)
{
    lagrange::logger().info("=== After index unification ===");
    lagrange::logger().info("      # vertices: {}", mesh.get_num_vertices());
    lagrange::logger().info("         # faces: {}", mesh.get_num_facets());
    if (mesh.has_indexed_attribute("uv")) {
        lagrange::logger().info("     # UV coords: {}", mesh.get_vertex_attribute("uv").rows());
    }
    if (mesh.has_indexed_attribute("normal")) {
        lagrange::logger().info("# normals values: {}", mesh.get_vertex_attribute("normal").rows());
    }

    lagrange::io::save_mesh(filename, mesh);
}

int main(int argc, char** argv)
{
    spdlog::set_level(spdlog::level::debug);
    spdlog::flush_every(std::chrono::seconds(3));
    if (argc != 3) {
        lagrange::logger().error("Usage: {} input_mesh output_mesh", argv[0]);
        return -1;
    }

    auto mesh = import_mesh(argv[1]);
    std::vector<std::string> attrib_names = {"normal"};
    if (mesh->has_indexed_attribute("uv")) attrib_names.push_back("uv");

    constexpr int N = 10;
    Eigen::Matrix<double, N, 1> runtime;
    std::unique_ptr<lagrange::TriangleMesh3D> out_mesh;
    for (int i = 0; i < N; i++) {
        auto start_time = lagrange::get_timestamp();
        out_mesh = lagrange::unify_index_buffer(*mesh, attrib_names);
        auto end_time = lagrange::get_timestamp();
        runtime[i] = lagrange::timestamp_diff_in_seconds(start_time, end_time);
    }

    export_mesh(argv[2], *out_mesh);

    lagrange::logger().info("=== Performance ===");
    for (int i = 0; i < N; i++) {
        lagrange::logger().info("  run {}: {}", i, runtime[i]);
    }
    lagrange::logger().info("Average run time: {}", runtime.mean());

    return 0;
}
