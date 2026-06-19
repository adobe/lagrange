/*
 * Copyright 2026 Adobe. All rights reserved.
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
#include <lagrange/uv_mesh.h>
#include <lagrange/xatlas/unwrap_mesh.h>

#include <CLI/CLI.hpp>

#include <map>

int main(int argc, char** argv)
{
    using namespace lagrange;
    using Scalar = float;
    using Index = uint32_t;
    using SurfaceMesh = lagrange::SurfaceMesh<Scalar, Index>;

    const std::map<std::string, xatlas::MultiAtlasPolicy> multi_atlas_policy_map{
        {"normalize", xatlas::MultiAtlasPolicy::NormalizePerTile},
        {"udim", xatlas::MultiAtlasPolicy::Udim},
        {"error", xatlas::MultiAtlasPolicy::ErrorIfMultiple},
    };

    struct
    {
        fs::path input;
        fs::path output = "output.ply";
        fs::path uv_mesh_output;
        std::string uv_attribute_name = "texcoord";
        std::string atlas_attribute_name;
        std::string chart_attribute_name;
        xatlas::MultiAtlasPolicy multi_atlas_policy = xatlas::MultiAtlasPolicy::NormalizePerTile;
        bool enable_sharing_uvs_between_instances = false;
        int log_level = 2;
    } args;

    CLI::App app{argv[0]};
    app.option_defaults()->always_capture_default();
    app.add_option("input,-i,--input", args.input, "Input mesh.")
        ->required()
        ->check(CLI::ExistingFile);
    app.add_option("output,-o,--output", args.output, "Output unwrapped mesh.");
    app.add_option(
        "--uv-mesh",
        args.uv_mesh_output,
        "Output the UV layout as a flat mesh (vertices are the UV coordinates).");
    app.add_option("--uv-attr-name", args.uv_attribute_name, "UV attribute name.");
    app.add_option("--atlas-attr-name", args.atlas_attribute_name, "Atlas attribute name.");
    app.add_option("--chart-attr-name", args.chart_attribute_name, "Chart attribute name.");
    app.add_option(
           "--multi-atlas-policy",
           args.multi_atlas_policy,
           "Multi-atlas policy: 'normalize', 'udim', or 'error'.")
        ->transform(CLI::CheckedTransformer(multi_atlas_policy_map, CLI::ignore_case));
    app.add_flag(
        "--share-uvs,!--no-share-uvs",
        args.enable_sharing_uvs_between_instances,
        "Share UVs between instances of the same mesh.");
    app.add_option("-l,--log-level", args.log_level, "Log level.");

    CLI11_PARSE(app, argc, argv)

    logger().set_level(static_cast<spdlog::level::level_enum>(args.log_level));


    logger().info("Loading input mesh \"{}\"", args.input.string());
    auto mesh = io::load_mesh<SurfaceMesh>(args.input);
    logger().info(
        "{}v {}f{}",
        mesh.get_num_vertices(),
        mesh.get_num_facets(),
        mesh.is_triangle_mesh() ? " tri" : "");

    xatlas::UnwrapOptions options;
    options.output_uv_attribute_name = args.uv_attribute_name;
    options.output_atlas_attribute_name = args.atlas_attribute_name;
    options.output_chart_attribute_name = args.chart_attribute_name;
    options.enable_sharing_uvs_between_instances = args.enable_sharing_uvs_between_instances;
    options.multi_atlas_policy = args.multi_atlas_policy;

    logger().info("Unwrapping mesh with xatlas");

    auto mesh_ = xatlas::unwrap_mesh(mesh, options, [](const std::string& msg, float progress) {
        logger().debug("[{:03.0f}%] {}", progress * 100.f, msg);
    });

    la_runtime_assert(mesh_.has_attribute(args.uv_attribute_name));
    la_runtime_assert(mesh_.is_attribute_indexed(args.uv_attribute_name));
    la_runtime_assert(mesh_.template is_attribute_type<Scalar>(args.uv_attribute_name));

    if (!args.atlas_attribute_name.empty()) {
        la_runtime_assert(mesh_.has_attribute(args.atlas_attribute_name));
        la_runtime_assert(!mesh_.is_attribute_indexed(args.atlas_attribute_name));
        la_runtime_assert(mesh_.template is_attribute_type<int32_t>(args.atlas_attribute_name));
    }

    if (!args.chart_attribute_name.empty()) {
        la_runtime_assert(mesh_.has_attribute(args.chart_attribute_name));
        la_runtime_assert(!mesh_.is_attribute_indexed(args.chart_attribute_name));
        la_runtime_assert(mesh_.template is_attribute_type<int32_t>(args.chart_attribute_name));
    }

    if (!args.output.empty()) {
        logger().info("Saving output mesh \"{}\"", args.output.string());
        io::SaveOptions save_options;
        save_options.attribute_conversion_policy =
            io::SaveOptions::AttributeConversionPolicy::ConvertAsNeeded;
        io::save_mesh(args.output, mesh_, save_options);
    }

    if (!args.uv_mesh_output.empty()) {
        logger().info("Saving UV mesh \"{}\"", args.uv_mesh_output.string());

        // Extract a flat mesh whose vertex positions are the UV coordinates.
        UVMeshOptions uv_mesh_options;
        uv_mesh_options.uv_attribute_name = args.uv_attribute_name;
        auto uv_mesh = uv_mesh_view(mesh_, uv_mesh_options);

        io::save_mesh(args.uv_mesh_output, uv_mesh);
    }

    return 0;
}
