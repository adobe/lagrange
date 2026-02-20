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
#include <lagrange/io/load_simple_scene.h>
#include <lagrange/io/save_mesh.h>

#include <lagrange/AttributeValueType.h>
#include <lagrange/compute_normal.h>
#include <lagrange/foreach_attribute.h>
#include <lagrange/map_attribute.h>
#include <lagrange/subdivision/compute_sharpness.h>
#include <lagrange/subdivision/mesh_subdivision.h>
#include <lagrange/subdivision/midpoint_subdivision.h>
#include <lagrange/subdivision/sqrt_subdivision.h>
#include <lagrange/topology.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/fmt_eigen.h>
#include <lagrange/utils/utils.h>
#include <lagrange/views.h>
#include <lagrange/weld_indexed_attribute.h>

#include <CLI/CLI.hpp>

#include <map>

int main(int argc, char** argv)
{
    struct
    {
        std::string input;
        std::string output = "output.obj";
        std::string scheme = "auto";
        bool output_btn = false;
        std::optional<float> autodetect_normal_threshold_deg;
        int log_level = 1; // debug
    } args;

    lagrange::subdivision::SubdivisionOptions subdivision_options;
    lagrange::subdivision::SharpnessOptions sharpness_options;

    std::map<std::string, lagrange::subdivision::RefinementType> map{
        {"Uniform", lagrange::subdivision::RefinementType::Uniform},
        {"EdgeAdaptive", lagrange::subdivision::RefinementType::EdgeAdaptive}};

    CLI::App app{argv[0]};
    app.option_defaults()->always_capture_default();
    app.add_option("input", args.input, "Input mesh.")->required()->check(CLI::ExistingFile);
    app.add_option("output", args.output, "Output mesh.");
    app.add_option("-s,--scheme", args.scheme, "Subdivision scheme")
        ->check(CLI::IsMember({"auto", "bilinear", "loop", "catmark", "sqrt", "midpoint"}));
    app.add_option(
        "-n,--num-levels",
        subdivision_options.num_levels,
        "Number of subdivision levels");
    app.add_option(
        "-a,--autodetect-normal-threshold",
        args.autodetect_normal_threshold_deg,
        "Normal angle threshold (in degree) for autodetecting sharp edges");
    app.add_flag(
        "--limit",
        subdivision_options.use_limit_surface,
        "Project vertex attributes to the limit surface");
    app.add_option("--refinement", subdivision_options.refinement, "Mesh refinement method")
        ->transform(CLI::CheckedTransformer(map, CLI::ignore_case));
    app.add_option(
        "--edge-length",
        subdivision_options.max_edge_length,
        "Max edge length target for adaptive refinement");
    app.add_flag("--normal", args.output_btn, "Compute limit normal as a vertex attribute");
    app.add_option("-l,--level", args.log_level, "Log level (0 = most verbose, 6 = off).");
    CLI11_PARSE(app, argc, argv);

    args.log_level = std::max(0, std::min(6, args.log_level));
    spdlog::set_level(static_cast<spdlog::level::level_enum>(args.log_level));

    /////////////////////
    // Load input mesh //
    /////////////////////

    lagrange::logger().info("Loading input mesh: {}", args.input);
    lagrange::SurfaceMesh32d mesh;

    bool is_gltf = std::set<std::string>{".gltf", ".glb"}.count(
        lagrange::fs::path(args.input).extension().string());
    if (is_gltf) {
        lagrange::logger().warn(
            "Input mesh is a glTF file. Essential connectivity information is lost when loading "
            "from a glTF asset. We strongly advise using .fbx or .obj as an input file format "
            "rather than glTF.");
    }
    lagrange::io::LoadOptions load_options;
    load_options.stitch_vertices = true;
    mesh = lagrange::io::load_mesh<lagrange::SurfaceMesh32d>(args.input, load_options);
    lagrange::logger().info(
        "Input mesh has {} vertices and {} facets",
        mesh.get_num_vertices(),
        mesh.get_num_facets());

    ///////////////////////
    // Asset preparation //
    ///////////////////////

    // Weld any indexed attribute that we find
    seq_foreach_named_attribute_read(mesh, [&](auto name, auto&& attr) {
        if (mesh.attr_name_is_reserved(name)) {
            return;
        }
        if (attr.get_element_type() == lagrange::AttributeElement::Indexed) {
            lagrange::logger().info("Welding indexed attribute: {}", name);
            lagrange::WeldOptions w_opts;
            w_opts.epsilon_rel = 1e-3;
            w_opts.epsilon_abs = 1e-3;
            lagrange::weld_indexed_attribute(mesh, mesh.get_attribute_id(name), w_opts);
        }
    });
    // Compute sharpness information
    if (args.autodetect_normal_threshold_deg.has_value()) {
        sharpness_options.feature_angle_threshold =
            lagrange::to_radians(args.autodetect_normal_threshold_deg.value());
    }
    auto sharpness_results = lagrange::subdivision::compute_sharpness(mesh, sharpness_options);
    subdivision_options.vertex_sharpness_attr = sharpness_results.vertex_sharpness_attr;
    subdivision_options.edge_sharpness_attr = sharpness_results.edge_sharpness_attr;

    //////////////////////
    // Mesh subdivision //
    //////////////////////

    if ((std::set<std::string>{"auto", "bilinear", "loop", "catmark"}).count(args.scheme)) {
        // Convert subdiv scheme to enum
        if (args.scheme == "loop") {
            subdivision_options.scheme = lagrange::subdivision::SchemeType::Loop;
        } else if (args.scheme == "catmark") {
            subdivision_options.scheme = lagrange::subdivision::SchemeType::CatmullClark;
        } else if (args.scheme == "bilinear") {
            subdivision_options.scheme = lagrange::subdivision::SchemeType::Bilinear;
        }

        if (args.output_btn && sharpness_results.normal_attr.has_value()) {
            // Only output a single set of normals in this example
            mesh.delete_attribute(mesh.get_attribute_name(sharpness_results.normal_attr.value()));
        }

        if (args.output_btn) {
            subdivision_options.output_limit_normals = "normal";
        }
        mesh = lagrange::subdivision::subdivide_mesh(mesh, subdivision_options);
        if (args.output_btn) {
            map_attribute_in_place(mesh, "normal", lagrange::AttributeElement::Indexed);
        }
    } else if (args.scheme == "sqrt") {
        for (unsigned i = 0; i < subdivision_options.num_levels; ++i) {
            mesh = lagrange::subdivision::sqrt_subdivision(mesh);
            if (i + 1 < subdivision_options.num_levels) {
                lagrange::logger().info(
                    "Intermediate mesh has {} vertices and {} facets",
                    mesh.get_num_vertices(),
                    mesh.get_num_facets());
            }
        }
    } else if (args.scheme == "midpoint") {
        for (unsigned i = 0; i < subdivision_options.num_levels; ++i) {
            mesh = lagrange::subdivision::midpoint_subdivision(mesh);
            if (i + 1 < subdivision_options.num_levels) {
                lagrange::logger().info(
                    "Intermediate mesh has {} vertices and {} facets",
                    mesh.get_num_vertices(),
                    mesh.get_num_facets());
            }
        }
    } else {
        throw std::runtime_error("Unsupported argument");
    }

    //////////////////////
    // Save output mesh //
    //////////////////////

    lagrange::logger().info(
        "Output mesh has {} vertices and {} facets",
        mesh.get_num_vertices(),
        mesh.get_num_facets());

    lagrange::logger().info("Saving output mesh: {}", args.output);
    lagrange::io::save_mesh(args.output, mesh);

    return 0;
}
