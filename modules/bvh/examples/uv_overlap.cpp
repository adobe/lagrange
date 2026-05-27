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
#include <lagrange/IndexedAttribute.h>
#include <lagrange/Logger.h>
#include <lagrange/bvh/compute_uv_overlap.h>
#include <lagrange/cast_attribute.h>
#include <lagrange/compute_uv_charts.h>
#include <lagrange/compute_uv_orientation.h>
#include <lagrange/disconnect_uv_charts.h>
#include <lagrange/find_matching_attributes.h>
#include <lagrange/internal/compact_chart_ids.h>
#include <lagrange/io/load_mesh.h>
#include <lagrange/io/save_mesh.h>
#include <lagrange/map_attribute.h>
#include <lagrange/packing/repack_uv_charts.h>
#include <lagrange/polyscope/register_mesh.h>
#include <lagrange/triangulate_polygonal_facets.h>
#include <lagrange/unflip_uv_charts.h>
#include <lagrange/unify_index_buffer.h>
#include <lagrange/utils/fmt/join.h>
#include <lagrange/utils/timing.h>
#include <lagrange/uv_mesh.h>
#include <lagrange/views.h>

#include <CLI/CLI.hpp>
#include <glm/gtc/matrix_transform.hpp>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <polyscope/polyscope.h>
#include <polyscope/surface_mesh.h>
#include <polyscope/view.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <unordered_map>

// ============================================================================
// Mesh processing utilities
// ============================================================================

using SurfaceMesh = lagrange::SurfaceMesh32d;

///
/// Prepare a mesh for 3D display in polyscope by unifying non-UV index buffers
/// and converting indexed UV attributes to corner attributes.
///
void prepare_mesh_for_display(SurfaceMesh& mesh)
{
    lagrange::AttributeMatcher matcher;
    matcher.element_types = lagrange::AttributeElement::Indexed;

    matcher.usages = ~lagrange::BitField(lagrange::AttributeUsage::UV);
    auto ids = lagrange::find_matching_attributes(mesh, matcher);
    if (!ids.empty()) {
        std::vector<std::string> attr_names;
        for (auto id : ids) {
            attr_names.emplace_back(mesh.get_attribute_name(id));
        }
        lagrange::logger().info(
            "Unifying index buffers for {} non-UV indexed attributes: {}",
            ids.size(),
            lagrange::join(attr_names, ", "));
        mesh = lagrange::unify_index_buffer(mesh, ids);
    }

    matcher.usages = lagrange::AttributeUsage::UV;
    ids = lagrange::find_matching_attributes(mesh, matcher);
    for (auto id : ids) {
        lagrange::logger().info(
            "Converting indexed UV attribute to corner attribute: {}",
            mesh.get_attribute_name(id));
        map_attribute_in_place(mesh, id, lagrange::AttributeElement::Corner);
    }
}

///
/// Repack UV charts by splitting overlapping regions into separate packing layers.
///
void repack_overlapping_charts(
    SurfaceMesh& mesh,
    lagrange::AttributeId overlap_coloring_id,
    const std::string& uv_attribute_name)
{
    lagrange::logger().info("Repacking UV charts using overlap coloring...");

    // 1. Compute connectivity-based UV charts
    lagrange::UVChartOptions chart_options;
    chart_options.uv_attribute_name = uv_attribute_name;
    chart_options.output_attribute_name = "@chart_id";
    lagrange::compute_uv_charts(mesh, chart_options);

    // 2. Combine chart ID and overlap color into a split chart attribute:
    //    split_id = chart_id * num_colors + overlap_color
    auto chart_ids =
        lagrange::attribute_vector_view<uint32_t>(mesh, mesh.get_attribute_id("@chart_id"));
    auto color_ids = lagrange::attribute_vector_view<uint32_t>(mesh, overlap_coloring_id);

    uint32_t num_colors = color_ids.maxCoeff() + 1;
    auto num_facets = mesh.get_num_facets();
    std::vector<uint32_t> split_ids(num_facets);
    for (uint32_t f = 0; f < num_facets; ++f) {
        split_ids[f] = static_cast<uint32_t>(chart_ids[f] * num_colors + color_ids[f]);
    }

    // Compact per-facet ids so the downstream packer doesn't allocate empty slots for unused
    // combinations.
    auto compacted = lagrange::internal::compact_chart_ids<uint32_t>(
        lagrange::span<const uint32_t>(split_ids.data(), split_ids.size()));
    mesh.template create_attribute<uint32_t>(
        "@split_chart_id",
        lagrange::AttributeElement::Facet,
        lagrange::AttributeUsage::Scalar,
        1,
        compacted.first);

    // 3. Split UV indices so that facets in different split charts don't share UV vertices
    lagrange::DisconnectUVChartsOptions disconnect_options;
    disconnect_options.uv_attribute_name = uv_attribute_name;
    disconnect_options.chart_id_attribute_name = "@split_chart_id";
    lagrange::disconnect_uv_charts(mesh, disconnect_options);

    // 4. Repack using the split chart attribute
    lagrange::packing::RepackOptions repack_options;
    repack_options.chart_attribute_name = "@split_chart_id";
    lagrange::packing::repack_uv_charts(mesh, repack_options);
}

size_t apply_unflip(SurfaceMesh& mesh, const std::string& uv_attribute_name)
{
    // Resolve the target UV attribute before any mapping so the selection is stable.
    // map_attribute_in_place() can delete/recreate attributes and alter iteration order,
    // which would cause the default "first UV" selection to pick a different attribute.
    std::string resolved_name = uv_attribute_name;
    if (resolved_name.empty()) {
        lagrange::AttributeMatcher matcher;
        matcher.usages = lagrange::AttributeUsage::UV;
        auto uv_id = lagrange::find_matching_attribute(mesh, matcher);
        if (uv_id.has_value()) {
            resolved_name = std::string(mesh.get_attribute_name(uv_id.value()));
        }
    }

    // unflip_uv_charts requires an indexed UV attribute; map only the target attribute if needed.
    if (!resolved_name.empty() && mesh.has_attribute(resolved_name)) {
        auto id = mesh.get_attribute_id(resolved_name);
        if (mesh.get_attribute_base(id).get_element_type() != lagrange::AttributeElement::Indexed) {
            lagrange::logger().info(
                "Mapping non-indexed UV attribute '{}' to indexed for unflipping.",
                resolved_name);
            map_attribute_in_place(mesh, id, lagrange::AttributeElement::Indexed);
        }
    }

    lagrange::UnflipUVChartsOptions unflip_options;
    unflip_options.uv_attribute_name = resolved_name;
    size_t n = lagrange::unflip_uv_charts(mesh, unflip_options);
    lagrange::UVOrientationOptions flip_options;
    flip_options.uv_attribute_name = resolved_name;
    size_t still_flipped = lagrange::compute_uv_orientation(mesh, flip_options).negative;
    lagrange::logger().info(
        "Number of flipped triangles after unflipping charts: {}.",
        still_flipped);
    return n;
}

// ============================================================================
// GUI state and callbacks
// ============================================================================

struct DemoState
{
    // Original mesh with indexed UVs
    SurfaceMesh mesh_original;
    // Prepared mesh for polyscope display (unified indices, corner UVs)
    SurfaceMesh mesh_display;

    // Repacked versions (populated on button click)
    SurfaceMesh repacked_mesh_original;
    SurfaceMesh repacked_mesh_display;

    // Overlap coloring
    lagrange::AttributeId coloring_id = lagrange::invalid_attribute_id();
    std::string uv_attribute_name;
    std::string mesh_name;
    std::string output_path;

    // UI state
    bool uv_view = false;
    bool repacked = false;

    // Saved view state for each view mode
    struct SavedView
    {
        std::string camera_json;
        std::tuple<glm::vec3, glm::vec3> bounding_box = {glm::vec3{0.f}, glm::vec3{0.f}};
        float length_scale = 0.f;
        bool valid = false;
    };
    SavedView saved_3d_view;
    SavedView saved_uv_view;

    bool has_coloring() const { return coloring_id != lagrange::invalid_attribute_id(); }
};

/// Register a mesh as a 2D UV polyscope structure, with optional overlap coloring.
void register_uv_mesh(
    const std::string& name,
    SurfaceMesh& mesh,
    lagrange::AttributeId coloring_id,
    double x_offset = 0.0,
    const std::string& uv_attribute_name = "")
{
    lagrange::UVMeshOptions uv_opts;
    uv_opts.uv_attribute_name = uv_attribute_name;
    polyscope::SurfaceMesh* ps_mesh = [&] {
        using Scalar = SurfaceMesh::Scalar;
        using Index = SurfaceMesh::Index;
        using OtherScalar = std::conditional_t<std::is_same_v<Scalar, float>, double, float>;
        if (lagrange::uv_attribute_id<Scalar, Index, Scalar>(mesh, uv_opts)) {
            auto uv = lagrange::uv_mesh_view<Scalar, Index, Scalar>(mesh, uv_opts);
            return lagrange::polyscope::register_mesh(name, uv);
        } else if (lagrange::uv_attribute_id<Scalar, Index, OtherScalar>(mesh, uv_opts)) {
            auto uv = lagrange::uv_mesh_view<Scalar, Index, OtherScalar>(mesh, uv_opts);
            return lagrange::polyscope::register_mesh(name, uv);
        } else {
            throw std::runtime_error("Unable to find a UV attribute for mesh: " + name);
        }
    }();
    if (x_offset != 0.0) {
        glm::mat4 T = glm::translate(glm::mat4(1.0f), glm::vec3(x_offset, 0, 0));
        ps_mesh->setTransform(T);
    }
    if (coloring_id != lagrange::invalid_attribute_id()) {
        auto& ca = mesh.template get_attribute<uint32_t>(coloring_id);
        lagrange::polyscope::register_attribute(*ps_mesh, "uv_overlap_color", ca);
    }
    if (mesh.has_attribute("@uv_orientation")) {
        auto& fa = mesh.template get_attribute<int8_t>(mesh.get_attribute_id("@uv_orientation"));
        lagrange::polyscope::register_attribute(*ps_mesh, "uv_orientation", fa);
    }
}

/// Remove all polyscope structures and re-register them for the current view mode.
void register_view(DemoState& state)
{
    // Horizontal offset and scene extent for side-by-side UV layout display
    constexpr float k_uv_mesh_spacing = 1.2f;
    constexpr float k_uv_scene_extent_single = 1.f;
    constexpr float k_uv_scene_extent_dual = k_uv_mesh_spacing + k_uv_scene_extent_single;

    polyscope::removeAllStructures();

    auto repacked_name = state.mesh_name + "_repacked";

    if (state.uv_view) {
        register_uv_mesh(
            state.mesh_name,
            state.mesh_original,
            state.coloring_id,
            0.0,
            state.uv_attribute_name);
        if (state.repacked) {
            register_uv_mesh(
                repacked_name,
                state.repacked_mesh_original,
                state.coloring_id,
                k_uv_mesh_spacing,
                state.uv_attribute_name);
        }

        polyscope::options::automaticallyComputeSceneExtents = false;
        float x_extent = state.repacked ? k_uv_scene_extent_dual : k_uv_scene_extent_single;
        polyscope::state::boundingBox =
            std::make_tuple(glm::vec3{0.f, 0.f, 0.f}, glm::vec3{x_extent, 1.f, 0.f});
        polyscope::state::lengthScale = x_extent;

        polyscope::view::setUpDir(polyscope::UpDir::YUp);
        polyscope::view::setNavigateStyle(polyscope::NavigateStyle::Planar);
    } else {
        auto* ps3d = lagrange::polyscope::register_mesh(state.mesh_name, state.mesh_display);
        ps3d->setTransform(glm::mat4(1.0f));
        if (state.repacked) {
            auto* ps3d_repacked =
                lagrange::polyscope::register_mesh(repacked_name, state.repacked_mesh_display);
            ps3d_repacked->setTransform(glm::mat4(1.0f));
        }

        polyscope::options::automaticallyComputeSceneExtents = true;
        polyscope::view::setNavigateStyle(polyscope::NavigateStyle::Turntable);
    }

    polyscope::view::resetCameraToHomeView();
}

/// Toggle between 3D and UV view, saving/restoring camera and scene bounds.
void toggle_uv_view(DemoState& state)
{
    // Save state for the view we're leaving
    auto& leaving = state.uv_view ? state.saved_uv_view : state.saved_3d_view;
    leaving.camera_json = polyscope::view::getViewAsJson();
    leaving.bounding_box = polyscope::state::boundingBox;
    leaving.length_scale = polyscope::state::lengthScale;
    leaving.valid = true;

    state.uv_view = !state.uv_view;
    register_view(state);

    // Restore state for the view we're entering
    auto& entering = state.uv_view ? state.saved_uv_view : state.saved_3d_view;
    if (entering.valid) {
        polyscope::state::boundingBox = entering.bounding_box;
        polyscope::state::lengthScale = entering.length_scale;
        polyscope::view::setViewFromJson(entering.camera_json, false);
    }
}

/// ImGui callback for the polyscope UI.
void user_callback(DemoState& state)
{
    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantCaptureKeyboard && ImGui::IsKeyPressed(ImGuiKey_U)) {
        toggle_uv_view(state);
    }

    bool prev_uv_view = state.uv_view;
    ImGui::Checkbox("Show UV Layout (U)", &state.uv_view);
    if (state.uv_view != prev_uv_view) {
        // Undo checkbox toggle, then let toggle_uv_view handle state consistently
        state.uv_view = prev_uv_view;
        toggle_uv_view(state);
    }

    if (ImGui::Button("Unflip UV Charts")) {
        size_t n = apply_unflip(state.mesh_original, state.uv_attribute_name);
        lagrange::logger().info("Unflipped {} chart(s).", n);
        state.mesh_display = state.mesh_original;
        prepare_mesh_for_display(state.mesh_display);

        auto camera_json = polyscope::view::getViewAsJson();
        register_view(state);
        polyscope::view::setViewFromJson(camera_json, false);
    }

    if (state.has_coloring()) {
        ImGui::BeginDisabled(state.repacked);
        if (ImGui::Button("Repack UV Charts")) {
            state.repacked_mesh_original = state.mesh_original;
            repack_overlapping_charts(
                state.repacked_mesh_original,
                state.coloring_id,
                state.uv_attribute_name);
            state.repacked_mesh_display = state.repacked_mesh_original;
            prepare_mesh_for_display(state.repacked_mesh_display);
            state.repacked = true;

            auto camera_json = polyscope::view::getViewAsJson();
            register_view(state);
            polyscope::view::setViewFromJson(camera_json, false);
        }
        ImGui::EndDisabled();

        ImGui::BeginDisabled(!state.repacked);
        if (ImGui::Button("Export Repacked Mesh")) {
            lagrange::logger().info("Saving repacked mesh: {}", state.output_path);
            lagrange::io::save_mesh(state.output_path, state.repacked_mesh_original);
        }
        ImGui::EndDisabled();
    }
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv)
{
    struct
    {
        std::string input;
        std::string output = "output.obj";
        std::string method = "hybrid";
        std::string uv_attribute_name;
        bool gui = false;
        bool uv_view = false;
        bool repack = false;
        bool unflip = false;
        int log_level = 2;
    } args;

    CLI::App app{argv[0]};
    app.option_defaults()->always_capture_default();
    app.add_option("input", args.input, "Input mesh.")->required()->check(CLI::ExistingFile);
    app.add_option("output", args.output, "Output mesh.");
    app.add_option("-m,--method", args.method, "Candidate detection method: sweep, bvh, hybrid.")
        ->check(CLI::IsMember({"sweep", "bvh", "hybrid"}));
    app.add_option("--uv", args.uv_attribute_name, "UV attribute name (default: first UV found).");
    app.add_flag("--gui", args.gui, "Launch the Polyscope GUI to visualize results.");
    app.add_flag("--uv-view", args.uv_view, "Start in the 2D UV layout view (implies --gui).");
    app.add_flag("--repack", args.repack, "Repack UV charts per overlap color layer.");
    app.add_flag(
        "--unflip",
        args.unflip,
        "Reverse winding of any UV chart with negative signed area.");
    app.add_option("-l,--level", args.log_level, "Log level (0 = most verbose, 6 = off).");
    CLI11_PARSE(app, argc, argv)

    if (args.uv_view) {
        args.gui = true;
    }

    args.log_level = std::max(0, std::min(6, args.log_level));
    spdlog::set_level(static_cast<spdlog::level::level_enum>(args.log_level));

    // Load and triangulate
    lagrange::logger().info("Loading input mesh: {}", args.input);
    lagrange::io::LoadOptions load_options;
    load_options.stitch_vertices = true;
    auto mesh = lagrange::io::load_mesh<SurfaceMesh>(args.input, load_options);
    lagrange::triangulate_polygonal_facets(mesh);
    lagrange::logger().info(
        "Mesh has {} vertices and {} facets.",
        mesh.get_num_vertices(),
        mesh.get_num_facets());

    // Compute UV overlap
    lagrange::bvh::UVOverlapOptions options;
    options.uv_attribute_name = args.uv_attribute_name;
    options.compute_overlap_coloring = true;
    if (args.method == "bvh") {
        options.method = lagrange::bvh::UVOverlapMethod::BVH;
    } else if (args.method == "hybrid") {
        options.method = lagrange::bvh::UVOverlapMethod::Hybrid;
    } else {
        options.method = lagrange::bvh::UVOverlapMethod::SweepAndPrune;
    }

    lagrange::logger().info("Computing UV overlap (method={})...", args.method);
    lagrange::VerboseTimer timer("compute_uv_overlap", nullptr, spdlog::level::info);
    timer.tick();
    auto result = lagrange::bvh::compute_uv_overlap(mesh, options);
    timer.tock();

    if (result.has_overlap) {
        if (result.overlap_area.has_value()) {
            lagrange::logger().info("Total overlap area: {}", result.overlap_area.value());
        } else {
            lagrange::logger().info("UV overlap detected.");
        }
    } else {
        lagrange::logger().info("No UV overlap detected.");
    }

    if (args.gui || args.unflip) {
        lagrange::UVOrientationOptions orient_options;
        orient_options.uv_attribute_name = args.uv_attribute_name;
        size_t num_flipped = lagrange::compute_uv_orientation(mesh, orient_options).negative;
        lagrange::logger().info("Flipped UV triangles: {}", num_flipped);

        if (args.unflip && num_flipped > 0) {
            size_t num_unflipped = apply_unflip(mesh, args.uv_attribute_name);
            lagrange::logger().info("Unflipped {} chart(s).", num_unflipped);
        }
    }

    // GUI or CLI output
    if (args.gui) {
        polyscope::options::configureImGuiStyleCallback = []() {
            ImGui::Spectrum::StyleColorsSpectrum();
            ImGui::Spectrum::LoadFont();
        };
        polyscope::init();

        DemoState state;
        state.mesh_original = std::move(mesh);
        state.mesh_display = state.mesh_original;
        prepare_mesh_for_display(state.mesh_display);
        state.coloring_id = result.overlap_coloring_id;
        state.uv_attribute_name = args.uv_attribute_name;
        state.mesh_name = lagrange::fs::path(args.input).stem().string();
        state.output_path = args.output;
        state.uv_view = args.uv_view;

        register_view(state);
        polyscope::state::userCallback = [&]() { user_callback(state); };
        polyscope::show();
    } else {
        if (args.repack && result.overlap_coloring_id != lagrange::invalid_attribute_id()) {
            repack_overlapping_charts(mesh, result.overlap_coloring_id, args.uv_attribute_name);
        }
        lagrange::logger().info("Saving result: {}", args.output);
        lagrange::io::save_mesh(args.output, mesh);
    }

    return 0;
}
