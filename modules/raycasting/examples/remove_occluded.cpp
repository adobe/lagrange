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

#include <lagrange/Attribute.h>
#include <lagrange/Logger.h>
#include <lagrange/io/load_simple_scene.h>
#include <lagrange/io/save_simple_scene.h>
#include <lagrange/polyscope/register_mesh.h>
#include <lagrange/polyscope/set_transform.h>
#include <lagrange/raycasting/remove_occluded_facets.h>
#include <lagrange/raycasting/remove_occluded_instances.h>
#include <lagrange/scene/filter_instances.h>
#include <lagrange/utils/ProgressCallback.h>
#include <lagrange/utils/range.h>
#include <CLI/CLI.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <csignal>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <polyscope/polyscope.h>
#include <polyscope/surface_mesh.h>
#include <polyscope/types.h>
#include <imgui.h>
#include <lagrange/utils/warnon.h>
// clang-format on

using Scalar = float;
using Index = uint32_t;
using SceneType = lagrange::scene::SimpleScene<Scalar, Index, 3>;
using FacetFilter = lagrange::raycasting::OccludedFacetsFilter<Scalar, Index>;

struct Args
{
    std::string input;
    std::string output;
    uint64_t num_rays = 0LLU;
    uint64_t batch_size = lagrange::raycasting::OccludedFacetSamplerOptions{}.batch_size;
    size_t num_adaptive_per_cosine =
        lagrange::raycasting::AdaptiveOptions{}.num_adaptive_per_cosine;
    double vmf_kappa = lagrange::raycasting::AdaptiveOptions{}.vmf_kappa;
    double max_threshold =
        lagrange::raycasting::OccludedFacetSamplerOptions{}.max_visibility_threshold;
    double size_influence = lagrange::raycasting::OccludedInstanceSamplerOptions{}.size_influence;
    std::optional<double> graphcut_smoothness =
        lagrange::raycasting::RemoveOccludedFacetsOptions{}.graphcut_smoothness;
    int log_level = 2;
    bool vis = false;
    bool until_converged = false;
    bool no_adaptive = false;
};

static std::atomic_bool g_cancel{false};

extern "C" void signal_handler(int)
{
    g_cancel.store(true);
}

template <typename T>
[[nodiscard]] constexpr std::array<T, 3> green_red_colormap(T t)
{
    return {std::min(2 * t, T(1)), std::min((2 * (1 - t)), T(1)), T(0.1)};
}

// Meshes (instance-level) mode

/// A registered instance plus its frozen visibility measure.
struct RegisteredInstance
{
    ::polyscope::SurfaceMesh* ps;
    double visibility_measure;
    /// whether the instance is kept even at the strictest slider threshold (max_threshold == threshold)
    bool never_occluded;
};

/// Recolor occluded instances at given threshold. The never-occluded group is left alone.
void update_instances(
    const std::vector<RegisteredInstance>& meshes,
    double threshold,
    double max_measure)
{
    for (const auto& rm : meshes) {
        if (rm.never_occluded) continue;
        if (rm.visibility_measure >= threshold) {
            const double denom = max_measure - threshold;
            const double u =
                (denom > 0.0)
                    ? std::clamp(1.0 - (rm.visibility_measure - threshold) / denom, 0.0, 1.0)
                    : 0.0;
            const auto c = green_red_colormap(static_cast<float>(u));
            rm.ps->setSurfaceColor({c[0], c[1], c[2]});
            rm.ps->setTransparency(1.f);
        } else {
            rm.ps->setSurfaceColor({0.5, 0.5, 0.5});
            rm.ps->setTransparency(0.5f);
        }
    }
}

/// Register nonempty instance geometry with a live threshold slider initialized to max_threshold.
/// The tuned threshold is returned.
double visualize_instances(
    const SceneType& scene,
    const std::vector<std::vector<double>>& measures,
    const double max_threshold)
{
    polyscope::init();

    double threshold = max_threshold;
    auto* visible_group = polyscope::createGroup("Visible (all thresholds)");
    auto* occluded_group = polyscope::createGroup("Occluded (some thresholds)");

    double max_measure = std::numeric_limits<double>::epsilon();
    for (auto mi : lagrange::range(scene.get_num_meshes())) {
        for (auto ii : lagrange::range(scene.get_num_instances(mi))) {
            max_measure = std::max(max_measure, measures[mi][ii]);
        }
    }

    std::vector<RegisteredInstance> meshes;
    for (auto mi : lagrange::range(scene.get_num_meshes())) {
        const auto& mesh = scene.get_mesh(mi);
        if (mesh.get_num_facets() == 0) continue;
        for (auto ii : lagrange::range(scene.get_num_instances(mi))) {
            const auto& instance = scene.get_instance(mi, ii);
            const std::string name = "mesh_" + std::to_string(mi) + "_" + std::to_string(ii);
            auto* ps_mesh = lagrange::polyscope::register_mesh(name, mesh);
            lagrange::polyscope::set_transform(*ps_mesh, instance.transform);
            ps_mesh->setBackFacePolicy(::polyscope::BackFacePolicy::Identical);

            RegisteredInstance rm{ps_mesh, measures[mi][ii], false};
            rm.never_occluded = rm.visibility_measure >= max_threshold;
            if (rm.never_occluded) {
                const double u = std::clamp(1.0 - rm.visibility_measure / max_measure, 0.0, 1.0);
                const auto c = green_red_colormap(static_cast<float>(u));
                ps_mesh->setSurfaceColor({c[0], c[1], c[2]});
                ps_mesh->addToGroup(*visible_group);
            } else {
                ps_mesh->addToGroup(*occluded_group);
            }
            meshes.push_back(rm);
        }
    }
    update_instances(meshes, threshold, max_measure);

    const double zero = 0;
    ::polyscope::state::userCallback = [&]() {
        if (ImGui::SliderScalar(
                "threshold",
                ImGuiDataType_Double,
                &threshold,
                &zero,
                &max_threshold,
                "%.8g",
                ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_NoRoundToFormat)) {
            update_instances(meshes, threshold, max_measure);
        }
        ImGui::Text("instance keep threshold = %.8g", threshold);
    };

    polyscope::show();
    ::polyscope::state::userCallback = nullptr;
    return threshold;
}

int run_meshes_mode(const Args& args)
{
    lagrange::logger().info("Loading input scene: {}", args.input);
    const auto scene = lagrange::io::load_simple_scene<SceneType>(args.input);

    lagrange::raycasting::RemoveOccludedInstancesOptions options;
    options.sampler_options.threshold = args.max_threshold;
    options.sampler_options.size_influence = args.size_influence;
    options.estimate_options.num_rays = args.num_rays;
    options.estimate_options.batch_size = args.batch_size;
    options.estimate_options.until_converged = args.until_converged;

    lagrange::ProgressCallback progress;
    std::signal(SIGINT, signal_handler);
    if (args.num_rays == 0) lagrange::logger().info("Progressive mode (Ctrl+C to stop)");
    const auto measures = lagrange::raycasting::estimate_occluded_instance_measures<Scalar, Index>(
        scene,
        options,
        progress,
        [](Index, Index) { return true; },
        &g_cancel);
    std::signal(SIGINT, SIG_DFL);

    Index num_visible = 0;
    for (const auto& mesh_measures : measures) {
        for (double measure : mesh_measures) {
            if (measure >= args.max_threshold) ++num_visible;
        }
    }
    const Index num_instances = scene.compute_num_instances();
    lagrange::logger().info(
        "Found {} occluded instances out of {}",
        num_instances - num_visible,
        num_instances);

    // Threshold applied to the saved result — tunable live in the viewer, defaults to
    // max_threshold.
    double threshold = args.max_threshold;
    if (args.vis) {
        threshold = visualize_instances(scene, measures, args.max_threshold);
    }
    lagrange::logger().info("Using threshold = {} for output", threshold);

    const std::string output = args.output.empty() ? "output.obj" : args.output;
    lagrange::logger().info("Removing occluded instances and saving: {}", output);
    auto result = lagrange::scene::filter_instances(scene, [&](Index mi, Index ii) {
        return measures[mi][ii] >= threshold;
    });
    lagrange::io::save_simple_scene(output, result);

    return 0;
}

// Facets (facet-level) mode — either adaptive or plain-cosine sampling

/// Choose an output attribute name that does not overwrite any source mesh attribute.
std::string unused_attribute_name(const SceneType& scene, const std::string& base)
{
    std::string name = base;
    for (size_t suffix = 1;; ++suffix) {
        bool used = false;
        for (auto mi : lagrange::range(scene.get_num_meshes())) {
            if (scene.get_mesh(mi).has_attribute(name)) {
                used = true;
                break;
            }
        }
        if (!used) return name;
        name = base + "_" + std::to_string(suffix);
    }
}

/// Replace preview geometry with the pruned public result and color its surviving facets.
void update_facets(
    const SceneType& preview,
    std::vector<std::string>& meshes,
    const lagrange::raycasting::OccludedFacetFilterOptions& options)
{
    for (const auto& name : meshes) polyscope::removeSurfaceMesh(name);
    meshes.clear();
    for (auto mi : lagrange::range(preview.get_num_meshes())) {
        const auto& mesh = preview.get_mesh(mi);
        if (mesh.get_num_facets() == 0) continue;

        for (auto ii : lagrange::range(preview.get_num_instances(mi))) {
            const auto& instance = preview.get_instance(mi, ii);
            const std::string name = "Filtered_" + std::to_string(mi) + "_" + std::to_string(ii);
            auto* ps_mesh = lagrange::polyscope::register_mesh(name, mesh);
            lagrange::polyscope::set_transform(*ps_mesh, instance.transform);
            ps_mesh->setBackFacePolicy(::polyscope::BackFacePolicy::Identical);
            ps_mesh->getQuantity(*options.visibility_attribute_name)->setEnabled(true);
            meshes.push_back(name);
        }
    }
}

/// Switch between cached original and filtered geometry without recomputing the cut.
void set_facet_view(
    const std::vector<std::string>& original_meshes,
    const std::vector<std::string>& filtered_meshes,
    bool show_original)
{
    for (const auto& name : original_meshes)
        polyscope::getSurfaceMesh(name)->setEnabled(show_original);
    for (const auto& name : filtered_meshes)
        polyscope::getSurfaceMesh(name)->setEnabled(!show_original);
}

/// Reapply cached estimates as sliders move, with an original/filtered comparison toggle.
void visualize_facets(
    const FacetFilter& filter,
    const SceneType& source,
    const SceneType& classified,
    lagrange::raycasting::OccludedFacetFilterOptions& options,
    const double max_threshold)
{
    polyscope::init();

    double max_measure = 0.0;
    for (auto mi : lagrange::range(classified.get_num_meshes())) {
        const auto scores = classified.get_mesh(mi)
                                .get_attribute<Scalar>(*options.visibility_attribute_name)
                                .get_all();
        for (Scalar score : scores) max_measure = std::max(max_measure, double(score));
    }
    if (max_measure <= 0.0) max_measure = 1.0;

    bool show_original = false;
    std::vector<std::string> original_meshes;
    for (auto mi : lagrange::range(source.get_num_meshes())) {
        const auto& mesh = source.get_mesh(mi);
        if (mesh.get_num_facets() == 0) continue;
        for (auto ii : lagrange::range(source.get_num_instances(mi))) {
            const auto& instance = source.get_instance(mi, ii);
            const std::string name = "Original_" + std::to_string(mi) + "_" + std::to_string(ii);
            auto* ps_mesh = lagrange::polyscope::register_mesh(name, mesh);
            lagrange::polyscope::set_transform(*ps_mesh, instance.transform);
            ps_mesh->setBackFacePolicy(::polyscope::BackFacePolicy::Identical);
            ps_mesh->setEnabled(false);
            original_meshes.push_back(name);
        }
    }
    std::vector<std::string> meshes;
    auto refresh = [&] {
        auto preview_options = options;
        preview_options.keep_attribute_name.reset();
        const auto preview = filter.apply(preview_options);
        update_facets(preview, meshes, preview_options);
        set_facet_view(original_meshes, meshes, show_original);
    };
    refresh();

    const double zero = 0;
    const double max_smoothness = std::max(max_measure, options.graphcut_smoothness.value_or(0.0));
    double smoothness = options.graphcut_smoothness.value_or(0.0);
    bool live_updates = false;
    ::polyscope::state::userCallback = [&]() {
        if (ImGui::Checkbox("Show original (unfiltered)", &show_original))
            set_facet_view(original_meshes, meshes, show_original);
        ImGui::Checkbox("Live updates", &live_updates);
        ImGui::SetItemTooltip(
            "Recompute graph cut filtering while dragging sliders instead of only on release.");
        const auto flags = ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_NoRoundToFormat;
        const bool threshold_edited = ImGui::SliderScalar(
            "threshold",
            ImGuiDataType_Double,
            &options.visibility_threshold,
            &zero,
            &max_threshold,
            "%.8g",
            flags);
        const bool threshold_released = ImGui::IsItemDeactivatedAfterEdit();
        const bool smoothness_edited = ImGui::SliderScalar(
            "graphcut smoothness",
            ImGuiDataType_Double,
            &smoothness,
            &zero,
            &max_smoothness,
            "%.8g",
            flags);
        const bool smoothness_released = ImGui::IsItemDeactivatedAfterEdit();
        // Without live updates, graph cuts (positive smoothness) only rerun once a slider is
        // released; plain thresholding is cheap enough to preview while dragging.
        const bool smoothness_changed = live_updates ? smoothness_edited : smoothness_released;
        if (smoothness_changed) options.graphcut_smoothness = smoothness;
        const bool defer = !live_updates && options.graphcut_smoothness.value_or(0.0) > 0.0;
        if (smoothness_changed || (defer ? threshold_released : threshold_edited)) refresh();
        ImGui::Text(
            "keep threshold = %.8g, smoothness = %.8g",
            options.visibility_threshold,
            options.graphcut_smoothness.value_or(0.0));
    };

    polyscope::show();
    ::polyscope::state::userCallback = nullptr;
}

int run_facets_mode(const Args& args)
{
    if (args.until_converged) {
        lagrange::logger().warn("--until-converged is ignored in facet mode");
    }
    lagrange::logger().info("Loading input scene: {}", args.input);
    const auto source_scene = lagrange::io::load_simple_scene<SceneType>(args.input);

    lagrange::raycasting::OccludedFacetSamplerOptions sampler_opts;
    sampler_opts.num_rays = args.num_rays;
    sampler_opts.batch_size = args.batch_size;
    sampler_opts.max_visibility_threshold = args.max_threshold;
    sampler_opts.size_influence = args.size_influence;
    if (args.no_adaptive) {
        sampler_opts.adaptive.reset();
    } else {
        sampler_opts.adaptive =
            lagrange::raycasting::AdaptiveOptions{args.num_adaptive_per_cosine, args.vmf_kappa};
    }

    lagrange::ProgressCallback progress;
    std::signal(SIGINT, signal_handler);
    if (args.num_rays == 0) lagrange::logger().info("Progressive mode (Ctrl+C to stop)");
    FacetFilter filter(
        source_scene,
        sampler_opts,
        progress,
        [](Index, Index) { return true; },
        &g_cancel);
    std::signal(SIGINT, SIG_DFL);

    lagrange::raycasting::OccludedFacetFilterOptions options;
    options.visibility_threshold = args.max_threshold;
    options.graphcut_smoothness = args.graphcut_smoothness;
    options.instancing = lagrange::raycasting::InstancingPolicy::FlattenInstances;
    options.visibility_attribute_name = unused_attribute_name(source_scene, "occlusion_visibility");
    options.keep_attribute_name = unused_attribute_name(source_scene, "occlusion_keep");
    auto classified = filter.apply(options);

    uint64_t total_facets = 0;
    uint64_t num_visible = 0;
    for (auto mi : lagrange::range(classified.get_num_meshes())) {
        const auto keep =
            classified.get_mesh(mi).get_attribute<uint8_t>(*options.keep_attribute_name).get_all();
        total_facets += keep.size();
        for (uint8_t value : keep) num_visible += value != 0;
    }
    lagrange::logger().info(
        "Found {} occluded facets out of {}",
        total_facets - num_visible,
        total_facets);

    if (args.vis) visualize_facets(filter, source_scene, classified, options, args.max_threshold);
    lagrange::logger().info(
        "Using threshold = {}, graphcut_smoothness = {} for output",
        options.visibility_threshold,
        options.graphcut_smoothness ? std::to_string(*options.graphcut_smoothness) : "disabled");

    const std::string output = args.output.empty() ? "output.obj" : args.output;
    lagrange::logger().info("Removing occluded facets and saving: {}", output);
    options.visibility_attribute_name.reset();
    options.keep_attribute_name.reset();
    const auto result = filter.apply(options);
    lagrange::io::save_simple_scene(output, result);

    return 0;
}

// Entry point

int main(int argc, char** argv)
{
    Args args;

    CLI::App app{argv[0]};
    app.option_defaults()->always_capture_default();
    app.add_option("input", args.input, "Input scene or mesh.")
        ->required()
        ->check(CLI::ExistingFile);
    app.add_option("output", args.output, "Output scene or mesh. Default: output.obj");
    app.add_option(
        "--num-rays,-n",
        args.num_rays,
        "Total number of rays. 0 = progressive mode (Ctrl+C to stop).");
    app.add_option("--batch-size,-b", args.batch_size, "Rays per batch.");
    app.add_option(
        "--num-adaptive,-a",
        args.num_adaptive_per_cosine,
        "Adaptive mode: seed-guided rays per cosine ray (1:K mixture ratio, K >= 1).");
    app.add_option(
        "--kappa,-k",
        args.vmf_kappa,
        "Adaptive mode: vMF proposal concentration (> 0; lobe width ~ 1/sqrt(kappa)).");
    app.add_option(
        "--max-threshold,-s",
        args.max_threshold,
        "Max keep-threshold in [0, 0.5] (and the slider's upper bound). "
        "Meshes and facets: on their respective visibility measures.");
    app.add_option(
        "--size-influence",
        args.size_influence,
        "Exponent on the area ratio. Meshes: area/scene-AABB-area, facets: area_f/mean-face-area "
        "(0 = mean visibility, 1 = area-weighted visibility).");
    app.add_option(
           "--graphcut-smoothness,-g",
           args.graphcut_smoothness,
           "Facets: optional graph-cut smoothness (>= 0). Omit to disable graph cutting; 0 gives "
           "per-facet thresholding, while positive values favor coherent keep/cull regions.")
        ->check(CLI::NonNegativeNumber);
    app.add_option("--log-level,-l", args.log_level, "Log level.");
    app.add_flag("--visualize,-v", args.vis, "Launch polyscope visualization.");
    app.add_flag(
        "--no-adaptive",
        args.no_adaptive,
        "Facets: use plain cosine sampling instead of the adaptive seed-guided mixture.");
    app.add_flag(
        "--until-converged,-u",
        args.until_converged,
        "Meshes only: after the initial batch, stop when a batch confidently retires no new "
        "instances. Composes with --num-rays as a soft early-exit.");

    std::string mode_str = "meshes";
    app.add_option("--mode,-m", mode_str, "Granularity: meshes | facets.")
        ->check(CLI::IsMember({"meshes", "facets"}));

    CLI11_PARSE(app, argc, argv);
    lagrange::logger().set_level(static_cast<spdlog::level::level_enum>(args.log_level));

    if (mode_str == "meshes") {
        return run_meshes_mode(args);
    } else if (mode_str == "facets") {
        return run_facets_mode(args);
    }
    return 0;
}
