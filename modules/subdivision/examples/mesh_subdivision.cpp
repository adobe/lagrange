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
#include <lagrange/cast_attribute.h>
#include <lagrange/compute_normal.h>
#include <lagrange/compute_seam_edges.h>
#include <lagrange/compute_vertex_valence.h>
#include <lagrange/find_matching_attributes.h>
#include <lagrange/foreach_attribute.h>
#include <lagrange/map_attribute.h>
#include <lagrange/mesh_cleanup/remove_duplicate_vertices.h>
#include <lagrange/mesh_cleanup/remove_topologically_degenerate_facets.h>
#include <lagrange/scene/simple_scene_convert.h>
#include <lagrange/subdivision/mesh_subdivision.h>
#include <lagrange/subdivision/midpoint_subdivision.h>
#include <lagrange/subdivision/sqrt_subdivision.h>
#include <lagrange/topology.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/fmt_eigen.h>
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
        std::optional<float> autodetect_normal_threshold;
        int log_level = 1; // debug
    } args;

    lagrange::subdivision::SubdivisionOptions options;

    std::map<std::string, lagrange::subdivision::RefinementType> map{
        {"Uniform", lagrange::subdivision::RefinementType::Uniform},
        {"EdgeAdaptive", lagrange::subdivision::RefinementType::EdgeAdaptive}};

    CLI::App app{argv[0]};
    app.option_defaults()->always_capture_default();
    app.add_option("input", args.input, "Input mesh.")->required()->check(CLI::ExistingFile);
    app.add_option("output", args.output, "Output mesh.");
    app.add_option("-s,--scheme", args.scheme, "Subdivision scheme")
        ->check(CLI::IsMember({"auto", "bilinear", "loop", "catmark", "sqrt", "midpoint"}));
    app.add_option("-n,--num-levels", options.num_levels, "Number of subdivision levels");
    app.add_option(
        "-a,--autodetect-normal-threshold",
        args.autodetect_normal_threshold,
        "Normal angle threshold (in degree) for autodetecting sharp edges");
    app.add_flag(
        "--limit",
        options.use_limit_surface,
        "Project vertex attributes to the limit surface");
    app.add_option("--refinement", options.refinement, "Mesh refinement method")
        ->transform(CLI::CheckedTransformer(map, CLI::ignore_case));
    app.add_option(
        "--edge-length",
        options.max_edge_length,
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
    // Find an attribute to use as facet normal if possible (defines sharp edges)
    std::optional<lagrange::AttributeId> normal_id =
        lagrange::find_matching_attribute(mesh, lagrange::AttributeUsage::Normal);
    if (normal_id.has_value()) {
        lagrange::logger().info(
            "Found indexed normal attribute: {}",
            mesh.get_attribute_name(normal_id.value()));
    }
    // If autosmooth normals are requested by user, compute them (unless input asset already has
    // normals)
    if (!normal_id.has_value() && args.autodetect_normal_threshold.has_value()) {
        lagrange::logger().info(
            "Computing autosmooth normals with a threshold of {} degrees",
            args.autodetect_normal_threshold.value());
        float feature_angle_threshold = args.autodetect_normal_threshold.value() * M_PI / 180.f;
        normal_id = lagrange::compute_normal<double, uint32_t>(mesh, feature_angle_threshold);
    }
    // Finally, compute edge sharpness info based on indexed normal topology
    if (normal_id.has_value()) {
        lagrange::logger().info("Using mesh normals to set sharpness flag.");
        auto seam_id = lagrange::compute_seam_edges(mesh, normal_id.value());
        auto edge_sharpness_id = lagrange::cast_attribute<float>(mesh, seam_id, "edge_sharpness");
        options.edge_sharpness_attr = edge_sharpness_id;

        // Set vertex sharpness to 1 for leaf and junction vertices
        lagrange::VertexValenceOptions v_opts;
        v_opts.induced_by_attribute = mesh.get_attribute_name(seam_id);
        auto valence_id = lagrange::compute_vertex_valence(mesh, v_opts);
        auto valence = lagrange::attribute_vector_view<uint32_t>(mesh, valence_id);
        auto vertex_sharpness_id = mesh.create_attribute<float>(
            "vertex_sharpness",
            lagrange::AttributeElement::Vertex,
            lagrange::AttributeUsage::Scalar);
        auto vertex_sharpness = lagrange::attribute_vector_ref<float>(mesh, vertex_sharpness_id);
        for (uint32_t v = 0; v < mesh.get_num_vertices(); ++v) {
            vertex_sharpness[v] = (valence[v] == 1 || valence[v] > 2 ? 1.f : 0.f);
        }
        options.vertex_sharpness_attr = vertex_sharpness_id;
    }

    //////////////////////
    // Mesh subdivision //
    //////////////////////

    if ((std::set<std::string>{"auto", "bilinear", "loop", "catmark"}).count(args.scheme)) {
        // Convert subdiv scheme to enum
        if (args.scheme == "loop") {
            options.scheme = lagrange::subdivision::SchemeType::Loop;
        } else if (args.scheme == "catmark") {
            options.scheme = lagrange::subdivision::SchemeType::CatmullClark;
        } else if (args.scheme == "bilinear") {
            options.scheme = lagrange::subdivision::SchemeType::Bilinear;
        }

        if (args.output_btn && normal_id.has_value()) {
            // Only output a single set of normals in this example
            mesh.delete_attribute(mesh.get_attribute_name(normal_id.value()));
        }

        if (args.output_btn) {
            options.output_limit_normals = "normal";
        }
        mesh = lagrange::subdivision::subdivide_mesh(mesh, options);
        if (args.output_btn) {
            map_attribute_in_place(mesh, "normal", lagrange::AttributeElement::Indexed);
        }
    } else if (args.scheme == "sqrt") {
        for (unsigned i = 0; i < options.num_levels; ++i) {
            mesh = lagrange::subdivision::sqrt_subdivision(mesh);
            if (i + 1 < options.num_levels) {
                lagrange::logger().info(
                    "Intermediate mesh has {} vertices and {} facets",
                    mesh.get_num_vertices(),
                    mesh.get_num_facets());
            }
        }
    } else if (args.scheme == "midpoint") {
        for (unsigned i = 0; i < options.num_levels; ++i) {
            mesh = lagrange::subdivision::midpoint_subdivision(mesh);
            if (i + 1 < options.num_levels) {
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
