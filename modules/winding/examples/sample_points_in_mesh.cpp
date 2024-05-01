/*
 * Copyright 2023 Adobe. All rights reserved.
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
#include <lagrange/io/load_mesh.h>
#include <lagrange/views.h>
#include <lagrange/winding/FastWindingNumber.h>

#include <spdlog/fmt/ranges.h>
#include <CLI/CLI.hpp>
#include <Eigen/Geometry>

#include <random>

namespace fs = lagrange::fs;

int main(int argc, char** argv)
{
    using Scalar = float;
    using Index = uint32_t;
    using SurfaceMeshType = lagrange::SurfaceMesh<Scalar, Index>;

    struct
    {
        std::string input;
        std::string output = "output.xyz";
        size_t num_samples = 10000;
    } args;

    CLI::App app{argv[0]};
    app.option_defaults()->always_capture_default();
    app.add_option("input", args.input, "Input mesh.")->required()->check(CLI::ExistingFile);
    app.add_option("output", args.output, "Output points.");
    app.add_option(
        "-n,--num-samples",
        args.num_samples,
        "Number of points to sample (before filtering).");
    CLI11_PARSE(app, argc, argv)

    if (auto ext = fs::path(args.output).extension(); ext != ".xyz") {
        lagrange::logger().error(
            "Output file extension should be .xyz. '{}' was given.",
            ext.string());
    }

    // Load input mesh
    lagrange::logger().info("Loading input mesh: {}", args.input);
    auto mesh = lagrange::io::load_mesh<SurfaceMeshType>(args.input);

    // Compute bbox
    Eigen::AlignedBox<Scalar, 3> bbox;
    for (auto p : vertex_view(mesh).rowwise()) {
        bbox.extend(p.transpose());
    }
    std::uniform_real_distribution<Scalar> px(bbox.min().x(), bbox.max().x());
    std::uniform_real_distribution<Scalar> py(bbox.min().y(), bbox.max().y());
    std::uniform_real_distribution<Scalar> pz(bbox.min().z(), bbox.max().z());

    // Build fast winding number engine
    lagrange::winding::FastWindingNumber engine(mesh);

    // Sample points
    std::mt19937 gen;
    std::vector<std::array<float, 3>> points;
    for (size_t k = 0; k < args.num_samples; ++k) {
        std::array<float, 3> pos = {px(gen), py(gen), pz(gen)};
        if (engine.is_inside(pos)) {
            points.push_back(pos);
        }
    }

    // Save result
    lagrange::logger().info("Saving filtered sample points: {}", args.output);
    fs::ofstream out(args.output);
    for (auto p : points) {
        out << fmt::format("{}\n", fmt::join(p, " "));
    }

    return 0;
}
