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
#include <lagrange/utils/warnon.h>
// clang-format on

using Scalar = float;
using Index = uint32_t;
using SceneType = lagrange::scene::SimpleScene<Scalar, Index, 3>;
using InstanceSampler = lagrange::raycasting::OccludedInstanceSampler<Scalar, Index>;
using FacetSampler = lagrange::raycasting::OccludedFacetSampler<Scalar, Index>;

enum class Mode { Meshes, Facets, FacetsBruteforce };

struct Args
{
    static constexpr auto default_options = lagrange::raycasting::RemoveOccludedFacetsOptions{};

    std::string input;
    std::string output;
    uint64_t num_rays = 0LLU;
    uint64_t batch_size = 5000000;
    uint64_t num_adaptive_per_normal = default_options.estimate_options.num_adaptive_per_normal;
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

/// Green -> yellow -> red colormap for t in [0, 1].
std::array<float, 3> green_red_colormap(float t)
{
    float r = (t < 0.5f) ? (2.f * t) : 1.f;
    float g = (t < 0.5f) ? 1.f : (2.f * (1.f - t));
    return {r, g, 0.1f};
}

// ---------------------------------------------------------------------------
// Meshes (instance-level) mode
// ---------------------------------------------------------------------------

void visualize_instances(const SceneType& scene, const InstanceSampler& sampler)
{
    polyscope::init();

    auto* visible_group = polyscope::createGroup("Visible");
    auto* occluded_group = polyscope::createGroup("Occluded");

    uint64_t min_rays = std::numeric_limits<uint64_t>::max();
    uint64_t max_rays = 0;
    for (auto i : lagrange::range(sampler.num_instances())) {
        if (sampler.is_visible(i)) {
            min_rays = std::min(min_rays, sampler.num_rays_cast(i));
            max_rays = std::max(max_rays, sampler.num_rays_cast(i));
        }
    }

    Index global = 0;
    for (auto mi : lagrange::range(scene.get_num_meshes())) {
        const auto& mesh = scene.get_mesh(mi);
        for (auto ii : lagrange::range(scene.get_num_instances(mi))) {
            const auto& instance = scene.get_instance(mi, ii);
            std::string name = "mesh_" + std::to_string(mi) + "_" + std::to_string(ii);
            auto* ps_mesh = lagrange::polyscope::register_mesh(name, mesh);
            lagrange::polyscope::set_transform(*ps_mesh, instance.transform);
            ps_mesh->setBackFacePolicy(::polyscope::BackFacePolicy::Identical);
            if (!sampler.is_visible(global)) {
                ps_mesh->setSurfaceColor({0.5, 0.5, 0.5});
                ps_mesh->setTransparency(0.5);
                ps_mesh->addToGroup(*occluded_group);
            } else {
                float log_val =
                    std::log1p(static_cast<float>(sampler.num_rays_cast(global) - min_rays));
                float log_max = std::log1p(static_cast<float>(max_rays - min_rays));
                float t = (log_max > 0.f) ? (log_val / log_max) : 0.f;
                auto color = green_red_colormap(t);
                ps_mesh->setSurfaceColor({color[0], color[1], color[2]});
                ps_mesh->addToGroup(*visible_group);
            }
            ++global;
        }
    }

    polyscope::show();
}

int run_meshes_mode(const Args& args)
{
    lagrange::logger().info("Loading input scene: {}", args.input);
    auto scene = lagrange::io::load_simple_scene<SceneType>(args.input);

    InstanceSampler sampler(scene);

    lagrange::raycasting::OccludedInstanceEstimateOptions opts;
    opts.num_rays = args.num_rays;
    opts.batch_size = args.batch_size;
    opts.until_converged = args.until_converged;

    lagrange::ProgressCallback progress;
    std::signal(SIGINT, signal_handler);
    if (args.num_rays == 0) lagrange::logger().info("Progressive mode (Ctrl+C to stop)");
    lagrange::raycasting::estimate_occluded_instances(sampler, opts, progress, &g_cancel);
    std::signal(SIGINT, SIG_DFL);

    Index num_visible = 0;
    for (auto i : lagrange::range(sampler.num_instances())) {
        if (sampler.is_visible(i)) ++num_visible;
    }
    const Index num_occluded = sampler.num_instances() - num_visible;
    lagrange::logger().info(
        "Found {} occluded instances out of {}",
        num_occluded,
        sampler.num_instances());

    if (args.vis) {
        visualize_instances(scene, sampler);
    }

    const std::string output = args.output.empty() ? "output.obj" : args.output;
    lagrange::logger().info("Removing occluded instances and saving: {}", output);
    auto result = lagrange::scene::filter_instances(scene, [&](Index mi, Index ii) {
        const auto r = lagrange::range(mi);
        const Index global = std::accumulate(
                                 r.begin(),
                                 r.end(),
                                 Index{0},
                                 [&](Index s, Index m) { return s + scene.get_num_instances(m); }) +
                             ii;
        return sampler.is_visible(global);
    });
    lagrange::io::save_simple_scene(output, result);

    return 0;
}

// ---------------------------------------------------------------------------
// Facets (facet-level) mode — either normal+adaptive cycles or brute-force
// ---------------------------------------------------------------------------

void visualize_facets(const SceneType& scene, const FacetSampler& sampler)
{
    polyscope::init();

    // Heat scale: normalize across all visible facets.
    uint64_t min_rays = std::numeric_limits<uint64_t>::max();
    uint64_t max_rays = 0;
    for (const auto& info : sampler.instances()) {
        for (auto lf : lagrange::range(info.num_facets)) {
            const uint64_t gf = info.facet_offset + lf;
            if (sampler.is_visible(gf)) {
                min_rays = std::min(min_rays, sampler.num_rays_cast(gf));
                max_rays = std::max(max_rays, sampler.num_rays_cast(gf));
            }
        }
    }
    const float log_max = std::log1p(static_cast<float>(max_rays - min_rays));

    for (const auto& info : sampler.instances()) {
        const auto& mesh = scene.get_mesh(info.mesh_index);
        const auto& instance = scene.get_instance(info.mesh_index, info.instance_index);
        std::string name =
            "mesh_" + std::to_string(info.mesh_index) + "_" + std::to_string(info.instance_index);
        auto* ps_mesh = lagrange::polyscope::register_mesh(name, mesh);
        lagrange::polyscope::set_transform(*ps_mesh, instance.transform);
        ps_mesh->setBackFacePolicy(::polyscope::BackFacePolicy::Identical);

        std::vector<std::array<double, 3>> colors(info.num_facets);
        for (auto lf : lagrange::range(info.num_facets)) {
            const uint64_t gf = info.facet_offset + lf;
            if (!sampler.is_visible(gf)) {
                colors[lf] = {0.5, 0.5, 0.5};
            } else {
                const float log_val =
                    std::log1p(static_cast<float>(sampler.num_rays_cast(gf) - min_rays));
                const float t = (log_max > 0.f) ? (log_val / log_max) : 0.f;
                const auto c = green_red_colormap(t);
                colors[lf] = {c[0], c[1], c[2]};
            }
        }
        ps_mesh->addFaceColorQuantity("visibility", colors)->setEnabled(true);
    }

    polyscope::show();
}

int run_facets_mode(const Args& args)
{
    lagrange::logger().info("Loading input scene: {}", args.input);
    auto scene = lagrange::io::load_simple_scene<SceneType>(args.input);

    FacetSampler sampler(scene);
    const uint64_t total_facets = sampler.num_facets();

    lagrange::raycasting::OccludedFacetEstimateOptions opts;
    opts.num_rays = args.num_rays;
    opts.batch_size = args.batch_size;
    opts.num_adaptive_per_normal = args.num_adaptive_per_normal;
    opts.brute_force = args.mode == Mode::FacetsBruteforce;
    opts.until_converged = args.until_converged;

    lagrange::ProgressCallback progress;
    std::signal(SIGINT, signal_handler);
    if (args.num_rays == 0) lagrange::logger().info("Progressive mode (Ctrl+C to stop)");
    lagrange::raycasting::estimate_occluded_facets(sampler, opts, progress, &g_cancel);
    std::signal(SIGINT, SIG_DFL);

    uint64_t num_visible = 0;
    for (auto f : lagrange::range(total_facets)) {
        if (sampler.is_visible(f)) ++num_visible;
    }
    const uint64_t num_occluded = total_facets - num_visible;
    lagrange::logger().info("Found {} occluded facets out of {}", num_occluded, total_facets);

    if (args.vis) {
        visualize_facets(scene, sampler);
    }

    // One output mesh per input instance — see remove_occluded_facets().
    const std::string output = args.output.empty() ? "output.obj" : args.output;
    lagrange::logger().info("Removing occluded facets and saving: {}", output);
    SceneType result;
    for (const auto& info : sampler.instances()) {
        auto filtered = scene.get_mesh(info.mesh_index);
        filtered.remove_facets(
            [&](Index local_f) { return !sampler.is_visible(info.facet_offset + local_f); });
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
        args.num_adaptive_per_normal,
        "Adaptive batches per normal batch (facets mode only).");
    app.add_option("--log-level,-l", args.log_level, "Log level.");
    app.add_flag("--visualize,-v", args.vis, "Launch polyscope visualization.");
    app.add_flag(
        "--until-converged,-u",
        args.until_converged,
        "Stop early when a batch (meshes) or cycle (facets) finds no new visibilities. "
        "Composes with --num-rays as a soft early-exit.");

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
