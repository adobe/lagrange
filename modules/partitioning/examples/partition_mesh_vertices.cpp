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
////////////////////////////////////////////////////////////////////////////////
#include <lagrange/Mesh.h>
#include <lagrange/create_mesh.h>
#include <lagrange/extract_submesh.h>
#include <lagrange/io/load_mesh.h>
#include <lagrange/io/save_mesh.h>
#include <lagrange/partitioning/partition_mesh_vertices.h>
#include <lagrange/utils/range.h>
#include <lagrange/utils/timing.h>

#include <spdlog/sinks/basic_file_sink.h>
#include <CLI/CLI.hpp>
////////////////////////////////////////////////////////////////////////////////

namespace {

template <typename VA, typename FA, typename DerivedP>
std::vector<std::unique_ptr<lagrange::Mesh<VA, FA>>> split_mesh(
    const lagrange::Mesh<VA, FA>& mesh,
    const Eigen::MatrixBase<DerivedP>& partitions)
{
    auto num_parts = partitions.maxCoeff() + 1;
    using Index = typename FA::Scalar;
    std::vector<std::vector<Index>> selected_faces(num_parts + 1);
    for (Index f = 0; f < mesh.get_num_facets(); ++f) {
        auto face = mesh.get_facets().row(f).eval();
        auto id = partitions(face(0));
        for (Index lv = 0; lv < mesh.get_vertex_per_facet(); ++lv) {
            if (partitions(face[lv]) != id) {
                id = num_parts;
            }
        }
        selected_faces[id].push_back(f);
    }
    return lagrange::extract_submeshes(mesh, selected_faces);
}

} // namespace

int main(int argc, char const* argv[])
{
    namespace fs = lagrange::fs;

    // Default arguments
    struct
    {
        std::string input;
        std::string output = "output.obj";
        std::string log_file;
        int num_parts = 8;
        bool quiet = false;
        int log_level = 2; // normal
    } args;

    // Parse arguments
    CLI::App app{"partitioning"};
    app.add_option("input,-i,--input", args.input, "Input triangle mesh.")
        ->required()
        ->check(CLI::ExistingFile);
    app.add_option("output,-o,--output", args.output, "Output partitioned meshes.");
    app.add_option("-n,--num-parts", args.num_parts, "Number of partitions.");

    // Logging options
    app.add_flag("-q,--quiet", args.quiet, "Hide logger on stdout.");
    app.add_option("-l,--level", args.log_level, "Log level (0 = most verbose, 6 = off).");
    app.add_option("-f,--log-file", args.log_file, "Log file.");
    CLI11_PARSE(app, argc, argv)

    // Global spdlog behavior, set by the client
    args.log_level = std::max(0, std::min(6, args.log_level));
    if (args.quiet) {
        lagrange::logger().sinks().clear();
    }
    if (!args.log_file.empty()) {
        lagrange::logger().sinks().emplace_back(
            std::make_shared<spdlog::sinks::basic_file_sink_mt>(args.log_file, true));
    }
    spdlog::set_level(static_cast<spdlog::level::level_enum>(args.log_level));
    spdlog::flush_every(std::chrono::seconds(3));

    // Mesh data
    lagrange::logger().info("Loading model...");
    auto mesh = lagrange::io::load_mesh<lagrange::TriangleMesh3D>(args.input);
    lagrange::logger().info("Done");

    try {
        auto start_time = lagrange::get_timestamp();
        auto partitions =
            lagrange::partitioning::partition_mesh_vertices(mesh->get_facets(), args.num_parts);
        auto finish_time = lagrange::get_timestamp();
        auto timing = lagrange::timestamp_diff_in_seconds(start_time, finish_time);
        lagrange::logger().info("Partition running time: {:.3f} s", timing);

        auto res = split_mesh(*mesh, partitions);
        lagrange::logger().info(
            "Writing output to file: {}_*.obj",
            fs::path(args.output).stem().string());
        for (size_t i = 0; i < res.size(); ++i) {
            std::string name = fs::path(args.output).stem().string() + fmt::format("_{}.obj", i);
            lagrange::io::save_mesh(name, *res[i]);
        }
    } catch (std::exception& e) {
        lagrange::logger().error("Exception thrown: {}", e.what());
    }

    return 0;
}
