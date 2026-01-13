/*
 * Copyright 2022 Adobe. All rights reserved.
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
#include <lagrange/curve/edge_network_utils.h>
#include <lagrange/curve_io/load_image_svg.h>
#include <lagrange/io/save_mesh.h>
#include <lagrange/primitive/SweepPath.h>
#include <lagrange/primitive/generate_swept_surface.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/chain_edges.h>

#include <Eigen/Geometry>

#include <CLI/CLI.hpp>

int main(int argc, char** argv)
{
    struct
    {
        std::string input_svg;
        std::string output_mesh;
        lagrange::primitive::SweptSurfaceOptions options;
    } args;

    CLI::App app{argv[0]};
    app.add_option("input_svg", args.input_svg, "Input .svg profile")->required();
    app.add_option("output_mesh", args.output_mesh, "Ouput mesh file")->required();
    app.add_flag(
        "--use-full-uv-domain",
        args.options.fixed_uv,
        "Use full UV domain [0, 1] x [0, 1] for the generated mesh");
    app.add_flag("--triangulate", args.options.triangulate, "Output triangle mesh");
    CLI11_PARSE(app, argc, argv);

    using Scalar = float;
    using Index = uint32_t;
    auto svg = lagrange::curve_io::load_image_svg<Scalar, Index>(args.input_svg);

    if (svg.size() != 1 || svg.front().paths.size() != 1) {
        throw std::runtime_error("Only a single curve is supported for profile.");
    }

    Eigen::AlignedBox<Scalar, 2> bbox;
    lagrange::curve::remove_duplicate_vertices(*svg.front().paths.front().edge_network);
    const auto& edge_network = svg.front().paths.front().edge_network;
    const auto& vertices = edge_network->get_vertices();
    const auto& edges = edge_network->get_edges();
    bbox.extend(vertices.colwise().minCoeff().transpose());
    bbox.extend(vertices.colwise().maxCoeff().transpose());

    const auto result =
        lagrange::chain_directed_edges<Index>({edges.data(), static_cast<size_t>(edges.size())});
    la_runtime_assert(result.loops.size() > 0 || result.chains.size() > 0);
    lagrange::logger().info(
        "Found {} chains and {} loops in the SVG profile.",
        result.chains.size(),
        result.loops.size());

    Eigen::Matrix<Scalar, Eigen::Dynamic, 2, Eigen::RowMajor> profile(vertices.rows() + 1, 2);
    size_t count = 0;
    if (result.chains.size() > 0) {
        for (auto vi : result.chains[0]) {
            profile.row(count).segment<2>(0) = vertices.row(vi);
            count++;
        }
    } else if (result.loops.size() > 0) {
        for (auto vi : result.loops[0]) {
            profile.row(count).segment<2>(0) = vertices.row(vi);
            count++;
        }
        profile.row(count).segment<2>(0) = profile.row(0); // Close the loop
        count++;
    }

    Eigen::Matrix<Scalar, 1, 2> bbox_center = bbox.center().transpose();
    Scalar bbox_diag = bbox.diagonal().norm();
    Eigen::Translation<Scalar, 3> translation(-bbox_center[0], -bbox_center[1], 0);
    auto scaling = Eigen::Scaling(2.0f / bbox_diag, 2.0f / bbox_diag, 2.0f / bbox_diag);

    auto sweep_setting =
        lagrange::primitive::SweepOptions<Scalar>::circular_sweep({1.2, 0, 0}, {0, 1, 0});
    sweep_setting.set_normalization(scaling * translation);
    sweep_setting.set_num_samples(64);

    auto mesh = lagrange::primitive::generate_swept_surface<Scalar, Index>(
        {profile.data(), count * 2},
        sweep_setting,
        args.options);
    lagrange::io::save_mesh(args.output_mesh, mesh);
}
