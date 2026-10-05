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

#include <lagrange/Logger.h>
#include <lagrange/io/load_simple_scene.h>
#include <lagrange/io/save_simple_scene.h>
#include <lagrange/polyscope/register_mesh.h>
#include <lagrange/polyscope/set_transform.h>
#include <lagrange/raycasting/remove_occluded_facets.h>
#include <lagrange/raycasting/remove_occluded_instances.h>
#include <lagrange/scene/filter_instances.h>
#include <lagrange/utils/ProgressCallback.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/range.h>
#include <CLI/CLI.hpp>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <csignal>
#include <numeric>

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
using InstanceSampler = lagrange::raycasting::internal::OccludedInstanceSampler<Scalar, Index>;
using FacetSampler = lagrange::raycasting::internal::OccludedFacetSampler<Scalar, Index>;

enum class Mode { Meshes, Facets, FacetsBruteforce };

struct Args
{
    std::string input;
    std::string output;
    uint64_t num_rays = 0LLU;
    uint64_t batch_size = lagrange::raycasting::OccludedFacetEstimateOptions{}.batch_size;
    size_t num_adaptive_per_cosine =
        lagrange::raycasting::AdaptiveOptions{}.num_adaptive_per_cosine;
    double vmf_kappa = lagrange::raycasting::AdaptiveOptions{}.vmf_kappa;
    double max_threshold = lagrange::raycasting::OccludedFacetSamplerOptions{}.threshold;
    double size_influence = lagrange::raycasting::OccludedInstanceSamplerOptions{}.size_influence;
    int log_level = 2;
    bool vis = false;
    bool until_converged = false;
    Mode mode = Mode::Meshes;
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

// ---------------------------------------------------------------------------
// Meshes (instance-level) mode
// ---------------------------------------------------------------------------

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

/// Register every instance and launch polyscope with a live slider for the threshold, set to max_threshold. User threshold value is returned.
double visualize_instances(
    const SceneType& scene,
    const InstanceSampler& sampler,
    const double max_threshold)
{
    polyscope::init();

    double threshold = max_threshold;
    auto* visible_group = polyscope::createGroup("Visible (all thresholds)");
    auto* occluded_group = polyscope::createGroup("Occluded (some thresholds)");

    double max_measure = std::numeric_limits<double>::epsilon();
    for (auto mi : lagrange::range(scene.get_num_meshes())) {
        for (auto ii : lagrange::range(scene.get_num_instances(mi))) {
            max_measure = std::max(max_measure, sampler.visibility_measure(mi, ii));
        }
    }

    std::vector<RegisteredInstance> meshes;
    for (auto mi : lagrange::range(scene.get_num_meshes())) {
        const auto& mesh = scene.get_mesh(mi);
        for (auto ii : lagrange::range(scene.get_num_instances(mi))) {
            const auto& instance = scene.get_instance(mi, ii);
            const std::string name = "mesh_" + std::to_string(mi) + "_" + std::to_string(ii);
            auto* ps_mesh = lagrange::polyscope::register_mesh(name, mesh);
            lagrange::polyscope::set_transform(*ps_mesh, instance.transform);
            ps_mesh->setBackFacePolicy(::polyscope::BackFacePolicy::Identical);

            RegisteredInstance rm{ps_mesh, sampler.visibility_measure(mi, ii), false};
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

    // Drive the slider on sqrt(threshold) so it feels perceptually uniform.
    const auto max_threshold_root = static_cast<float>(std::sqrt(max_threshold));
    auto threshold_slider = max_threshold_root;
    ::polyscope::state::userCallback = [&, max_threshold_root, max_measure]() {
        if (ImGui::SliderFloat(
                "threshold (sqrt scale)",
                &threshold_slider,
                0.0f,
                max_threshold_root,
                "%.4f")) {
            threshold =
                static_cast<double>(threshold_slider) * static_cast<double>(threshold_slider);
            update_instances(meshes, threshold, max_measure);
        }
        ImGui::Text("instance keep threshold = %.6f", threshold);
    };

    polyscope::show();
    ::polyscope::state::userCallback = nullptr;
    return threshold;
}

int run_meshes_mode(const Args& args)
{
    lagrange::logger().info("Loading input scene: {}", args.input);
    auto scene = lagrange::io::load_simple_scene<SceneType>(args.input);

    lagrange::raycasting::OccludedInstanceSamplerOptions sampler_opts;
    sampler_opts.threshold = args.max_threshold;
    sampler_opts.size_influence = args.size_influence;
    InstanceSampler sampler(scene, sampler_opts);

    lagrange::raycasting::OccludedInstanceEstimateOptions opts;
    opts.num_rays = args.num_rays;
    opts.batch_size = args.batch_size;
    opts.until_converged = args.until_converged;

    std::signal(SIGINT, signal_handler);
    if (args.num_rays == 0) lagrange::logger().info("Progressive mode (Ctrl+C to stop)");
    // Drive the sampler to completion; the shared loop lives in the internal library driver.
    uint64_t prev_total_rays = 0;
    Index prev_retired = 0;
    bool has_previous_batch = false;
    while (true) {
        sampler.run_batch(opts.batch_size);
        const uint64_t total_rays = sampler.progress().second;
        const Index num_retired = sampler.num_retired();
        if (num_retired == sampler.num_instances()) break;
        if (total_rays == prev_total_rays) break;
        if (opts.until_converged && has_previous_batch && num_retired == prev_retired) break;
        prev_retired = num_retired;
        prev_total_rays = total_rays;
        has_previous_batch = true;
        if (g_cancel.load()) break;
        if (opts.num_rays > 0 && total_rays >= opts.num_rays) break;
    }
    std::signal(SIGINT, SIG_DFL);

    const Index num_visible = sampler.progress().first;
    lagrange::logger().info(
        "Found {} occluded instances out of {}",
        sampler.num_instances() - num_visible,
        sampler.num_instances());

    // Threshold applied to the saved result — tunable live in the viewer, defaults to
    // max_threshold.
    double threshold = args.max_threshold;
    if (args.vis) {
        threshold = visualize_instances(scene, sampler, args.max_threshold);
    }
    lagrange::logger().info("Using threshold = {} for output", threshold);

    const std::string output = args.output.empty() ? "output.obj" : args.output;
    lagrange::logger().info("Removing occluded instances and saving: {}", output);
    auto result = lagrange::scene::filter_instances(scene, [&](Index mi, Index ii) {
        return sampler.visibility_measure(mi, ii) >= threshold;
    });
    lagrange::io::save_simple_scene(output, result);

    return 0;
}

// ---------------------------------------------------------------------------
// Facets (facet-level) mode — either adaptive or plain-cosine sampling
// ---------------------------------------------------------------------------

/// Example of driving the public sampler progressively, with reporting and cancellation between
/// batches. Applications can instead inspect or visualize the sampler after every run_batch().
void run_facet_sampler(
    FacetSampler& sampler,
    const lagrange::raycasting::OccludedFacetEstimateOptions& options,
    lagrange::ProgressCallback& progress,
    const std::atomic_bool* cancel)
{
    la_runtime_assert(options.batch_size > 0);
    la_runtime_assert(options.num_rays > 0 || cancel != nullptr);

    const uint64_t total_facets = sampler.num_facets();
    lagrange::logger().info("Searching for occluded facets");
    progress.set_section("Searching for occluded facets");

    uint64_t prev_total_rays = 0;
    while (true) {
        sampler.run_batch(options.batch_size);

        uint64_t num_visible = 0;
        uint64_t total_rays = 0;
        for (uint64_t f = 0; f < total_facets; ++f) {
            if (sampler.is_visible(f)) ++num_visible;
            total_rays += sampler.num_rays_cast(f);
        }
        const uint64_t num_retired = sampler.num_retired();
        lagrange::logger().info(
            "{}/{} facets visible, {} confidently retired ({} rays so far)",
            num_visible,
            total_facets,
            num_retired,
            total_rays);
        const float fraction =
            options.num_rays > 0
                ? std::min(
                      1.f,
                      static_cast<float>(total_rays) / static_cast<float>(options.num_rays))
                : (total_facets > 0
                       ? static_cast<float>(num_retired) / static_cast<float>(total_facets)
                       : 1.f);
        progress.update(fraction);

        if (num_retired == total_facets) {
            lagrange::logger().info("All facets confidently visible, stopping early");
            break;
        }
        if (total_rays == prev_total_rays) {
            lagrange::logger().info("No facet rays could be cast, stopping");
            break;
        }
        prev_total_rays = total_rays;

        if (cancel != nullptr && cancel->load()) {
            lagrange::logger().info("Cancelled, using results so far");
            break;
        }
        if (options.num_rays > 0 && total_rays >= options.num_rays) break;
    }
}

/// A registered polyscope mesh + its facet range in the sampler's flat arrays.
struct RegisteredFacetMesh
{
    ::polyscope::SurfaceMesh* ps;
    uint64_t facet_offset;
    Index num_facets;
};

/// Recolor faces at threshold: gray below, heat-colored above (green = far above, red = near).
void update_facets(
    const FacetSampler& sampler,
    const std::vector<RegisteredFacetMesh>& meshes,
    double threshold,
    double max_measure)
{
    for (const auto& rm : meshes) {
        std::vector<std::array<double, 3>> colors(rm.num_facets);
        for (auto lf : lagrange::range(rm.num_facets)) {
            const double m = sampler.visibility_measure(rm.facet_offset + lf);
            if (m < threshold) {
                colors[lf] = {0.5, 0.5, 0.5};
            } else {
                const double denom = max_measure - threshold;
                const double u =
                    (denom > 0.0) ? std::clamp(1.0 - (m - threshold) / denom, 0.0, 1.0) : 0.0;
                colors[lf] = green_red_colormap(u);
            }
        }
        rm.ps->addFaceColorQuantity("visibility", colors)->setEnabled(true);
    }
}

/// Register every instance and launch polyscope with a live slider for the facet threshold, set to
/// max_threshold. User threshold value is returned.
double
visualize_facets(const SceneType& scene, const FacetSampler& sampler, const double max_threshold)
{
    polyscope::init();
    double threshold = max_threshold;

    // Heat reference: the largest per-face measure across the scene.
    double max_measure = 0.0;
    for (const auto& info : sampler.instances()) {
        for (auto lf : lagrange::range(info.num_facets)) {
            max_measure = std::max(max_measure, sampler.visibility_measure(info.facet_offset + lf));
        }
    }
    if (max_measure <= 0.0) max_measure = 1.0;

    std::vector<RegisteredFacetMesh> meshes;
    for (const auto& info : sampler.instances()) {
        const auto& mesh = scene.get_mesh(info.mesh_index);
        const auto& instance = scene.get_instance(info.mesh_index, info.instance_index);
        const std::string name =
            "mesh_" + std::to_string(info.mesh_index) + "_" + std::to_string(info.instance_index);
        auto* ps_mesh = lagrange::polyscope::register_mesh(name, mesh);
        lagrange::polyscope::set_transform(*ps_mesh, instance.transform);
        ps_mesh->setBackFacePolicy(::polyscope::BackFacePolicy::Identical);
        meshes.push_back({ps_mesh, info.facet_offset, info.num_facets});
    }
    update_facets(sampler, meshes, threshold, max_measure);

    // Slider on sqrt(threshold) for perceptual uniformity; bounded above by max_threshold.
    const auto max_threshold_root = static_cast<float>(std::sqrt(max_threshold));
    auto threshold_slider = max_threshold_root;
    ::polyscope::state::userCallback = [&, max_threshold_root, max_measure]() {
        if (ImGui::SliderFloat(
                "threshold (sqrt scale)",
                &threshold_slider,
                0.0f,
                max_threshold_root,
                "%.4f")) {
            threshold =
                static_cast<double>(threshold_slider) * static_cast<double>(threshold_slider);
            update_facets(sampler, meshes, threshold, max_measure);
        }
        ImGui::Text("facet keep threshold = %.6f", threshold);
    };

    polyscope::show();
    ::polyscope::state::userCallback = nullptr;
    return threshold;
}

int run_facets_mode(const Args& args)
{
    if (args.until_converged) {
        lagrange::logger().warn("--until-converged is ignored in facet modes");
    }
    lagrange::logger().info("Loading input scene: {}", args.input);
    auto scene = lagrange::io::load_simple_scene<SceneType>(args.input);

    lagrange::raycasting::OccludedFacetSamplerOptions sampler_opts;
    sampler_opts.threshold = args.max_threshold;
    sampler_opts.size_influence = args.size_influence;
    if (args.mode == Mode::Facets) {
        sampler_opts.adaptive =
            lagrange::raycasting::AdaptiveOptions{args.num_adaptive_per_cosine, args.vmf_kappa};
    } else {
        sampler_opts.adaptive.reset();
    }
    FacetSampler sampler(scene, sampler_opts);
    const uint64_t total_facets = sampler.num_facets();

    lagrange::raycasting::OccludedFacetEstimateOptions opts;
    opts.num_rays = args.num_rays;
    opts.batch_size = args.batch_size;

    lagrange::ProgressCallback progress;
    std::signal(SIGINT, signal_handler);
    if (args.num_rays == 0) lagrange::logger().info("Progressive mode (Ctrl+C to stop)");
    run_facet_sampler(sampler, opts, progress, &g_cancel);
    std::signal(SIGINT, SIG_DFL);

    uint64_t num_visible = 0;
    for (auto f : lagrange::range(total_facets)) {
        if (sampler.is_visible(f)) ++num_visible;
    }
    lagrange::logger().info(
        "Found {} occluded facets out of {}",
        total_facets - num_visible,
        total_facets);

    // Threshold is tuned in the viewer and applied to the saved result.
    double threshold = args.max_threshold;
    if (args.vis) {
        threshold = visualize_facets(scene, sampler, args.max_threshold);
    }
    lagrange::logger().info("Using threshold = {} for output", threshold);

    // Facet decisions are instance-specific, so the output contains one mesh per input instance.
    const std::string output = args.output.empty() ? "output.obj" : args.output;
    lagrange::logger().info("Removing occluded facets and saving: {}", output);
    SceneType result;
    for (const auto& info : sampler.instances()) {
        auto filtered = scene.get_mesh(info.mesh_index);
        filtered.remove_facets([&](Index local_f) {
            return sampler.visibility_measure(info.facet_offset + local_f) < threshold;
        });
        if (filtered.get_num_facets() == 0) continue;
        auto instance = scene.get_instance(info.mesh_index, info.instance_index);
        result.add_mesh(std::move(filtered));
        instance.mesh_index = result.get_num_meshes() - 1;
        result.add_instance(std::move(instance));
    }
    lagrange::io::save_simple_scene(output, result);

    return 0;
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

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
    app.add_option("--log-level,-l", args.log_level, "Log level.");
    app.add_flag("--visualize,-v", args.vis, "Launch polyscope visualization.");
    app.add_flag(
        "--until-converged,-u",
        args.until_converged,
        "Meshes only: after the initial batch, stop when a batch confidently retires no new "
        "instances. Composes with --num-rays as a soft early-exit.");

    std::string mode_str = "meshes";
    app.add_option("--mode,-m", mode_str, "Granularity: meshes | facets | facets_bruteforce.")
        ->check(CLI::IsMember({"meshes", "facets", "facets_bruteforce"}));

    CLI11_PARSE(app, argc, argv);

    if (mode_str == "meshes") {
        args.mode = Mode::Meshes;
    } else if (mode_str == "facets") {
        args.mode = Mode::Facets;
    } else {
        args.mode = Mode::FacetsBruteforce;
    }

    lagrange::logger().set_level(static_cast<spdlog::level::level_enum>(args.log_level));

    if (args.mode == Mode::Meshes) {
        return run_meshes_mode(args);
    }
    return run_facets_mode(args);
}
