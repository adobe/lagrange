/*
 * Copyright 2025 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */

/*

    ./examples/Release/texture_rasterization \
              --scene-in ../data/corp/texproc/prepared/pumpkin.glb \
              --renders-in ../data/corp/texproc/prepared/view_*.png \
              --width 1024 --height 1024 --base-confidence 0
    ./examples/Release/texture_compositing \
              --mesh-in ../data/corp/texproc/prepared/pumpkin.glb \
              --textures-in output_textures_*.exr \
              --weights-in output_weights_*.exr \
              --output composed.exr

*/
#include "../shared/shared_utils.h"
#include "io_helpers.h"

#include <lagrange/find_matching_attributes.h>
#include <lagrange/io/load_mesh.h>
#include <lagrange/io/save_scene.h>
#include <lagrange/map_attribute.h>
#include <lagrange/polyscope/register_mesh.h>
#include <lagrange/scene/scene_convert.h>
#include <lagrange/texproc/texture_compositing.h>
#include <lagrange/unify_index_buffer.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/fmt/format.h>
#include <lagrange/utils/range.h>
#include <lagrange/utils/timing.h>
#include <lagrange/views.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <polyscope/polyscope.h>
#include <polyscope/surface_mesh.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <imgui.h>
#include <imgui_spectrum.h>

#include <CLI/CLI.hpp>

#include <limits>
#include <map>

namespace fs = lagrange::fs;

template <typename ValueType>
lagrange::image::experimental::View3D<ValueType> extract_channel(
    lagrange::image::experimental::View3D<ValueType> image,
    size_t channel)
{
    return submdspan(
        image,
        lagrange::image::experimental::full_extent_t(),
        lagrange::image::experimental::full_extent_t(),
        std::tuple{channel, 1});
}

// ============================================================================
// GUI helpers
// ============================================================================

namespace {

/// Convert a 3-channel Array3Df image to polyscope scan-line color data.
/// Layout: extent(0)=W, extent(1)=H, extent(2)>=3.
/// Polyscope expects data in row-major order: outer loop over H (extent(1)),
/// inner loop over W (extent(0)), matching addTextureColorQuantity(H, W, ...).
std::vector<glm::vec3> texture_to_colors(const Array3Df& tex)
{
    const size_t W = tex.extent(0);
    const size_t H = tex.extent(1);
    std::vector<glm::vec3> colors;
    colors.reserve(W * H);
    for (size_t jj = 0; jj < H; ++jj) {
        for (size_t ii = 0; ii < W; ++ii) {
            colors.emplace_back(tex(ii, jj, 0), tex(ii, jj, 1), tex(ii, jj, 2));
        }
    }
    return colors;
}

/// Prepare a mesh for Polyscope display:
/// - Unify non-UV indexed attributes into a shared index buffer.
/// - Convert indexed UV attributes to per-corner attributes.
void prepare_mesh_for_display(lagrange::SurfaceMesh32f& mesh)
{
    lagrange::AttributeMatcher matcher;
    matcher.element_types = lagrange::AttributeElement::Indexed;

    matcher.usages = ~lagrange::BitField(lagrange::AttributeUsage::UV);
    auto ids = lagrange::find_matching_attributes(mesh, matcher);
    if (!ids.empty()) {
        mesh = lagrange::unify_index_buffer(mesh, ids);
    }

    matcher.usages = lagrange::AttributeUsage::UV;
    ids = lagrange::find_matching_attributes(mesh, matcher);
    for (auto id : ids) {
        lagrange::map_attribute_in_place(mesh, id, lagrange::AttributeElement::Corner);
    }
}

struct ImgSize
{
    size_t W, H;
};

struct DemoState
{
    lagrange::SurfaceMesh32f mesh; // display mesh (unified index buffer, corner UVs)
    std::string mesh_name;
    std::vector<Array3Df> textures; // input texture images (3-channel float)
    std::vector<Array3Df> weights; // input weight images (1+ channel float)
    Array3Df result; // composited output (3-channel float)

    // Pre-computed pixel buffers kept in memory for fast combo-box switching.
    // Indices [0..N-1] = input textures, [N] = result.
    std::vector<std::vector<glm::vec3>> all_colors;
    std::vector<ImgSize> color_sizes;
    // Indices [0..M-1] = weight maps.
    std::vector<std::vector<float>> all_scalars;
    std::vector<ImgSize> scalar_sizes;

    // Current combo-box selection.
    int color_idx = 0;
    int scalar_idx = 0;

    // Polyscope handles (set during register_view, valid for the lifetime of the GUI).
    polyscope::SurfaceMesh* ps_mesh = nullptr;
    polyscope::SurfaceParameterizationQuantity* ps_uv = nullptr;
};

/// Push new color data into the single "color_view" texture quantity.
void update_color_quantity(DemoState& state)
{
    if (state.color_idx < 0) return;
    const auto color_idx = static_cast<size_t>(state.color_idx);
    if (color_idx >= state.color_sizes.size()) return;
    const auto [W, H] = state.color_sizes[color_idx];
    if (state.ps_mesh->getQuantity("color_view")) state.ps_mesh->removeQuantity("color_view");
    auto* q = state.ps_mesh->addTextureColorQuantity(
        "color_view",
        *state.ps_uv,
        H,
        W,
        state.all_colors[color_idx],
        polyscope::ImageOrigin::UpperLeft);
    q->setFilterMode(polyscope::FilterMode::Nearest);
    q->setEnabled(true);
    if (auto* sq = state.ps_mesh->getQuantity("scalar_view")) sq->setEnabled(false);
}

/// Push new scalar data into the single "scalar_view" texture quantity.
void update_scalar_quantity(DemoState& state)
{
    if (state.scalar_idx < 0) return;
    const auto scalar_idx = static_cast<size_t>(state.scalar_idx);
    if (scalar_idx >= state.scalar_sizes.size()) return;
    const auto [W, H] = state.scalar_sizes[scalar_idx];
    if (state.ps_mesh->getQuantity("scalar_view")) state.ps_mesh->removeQuantity("scalar_view");
    auto* q = state.ps_mesh->addTextureScalarQuantity(
        "scalar_view",
        *state.ps_uv,
        H,
        W,
        state.all_scalars[scalar_idx],
        polyscope::ImageOrigin::UpperLeft);
    q->setColorMap("viridis");
    q->setEnabled(true);
    if (auto* cq = state.ps_mesh->getQuantity("color_view")) cq->setEnabled(false);
}

/// Register the mesh and exactly two texture quantities (one color, one scalar).
/// All pixel arrays are pre-computed here for instant combo-box switching later.
void register_view(DemoState& state)
{
    polyscope::removeAllStructures();

    state.ps_mesh = lagrange::polyscope::register_mesh(state.mesh_name, state.mesh);
    la_runtime_assert(state.ps_mesh != nullptr, "Failed to register mesh with Polyscope");

    // Extract corner UV values for the parameterization quantity.
    // After prepare_mesh_for_display, the UV attribute is a per-corner attribute.
    auto uv_id = lagrange::find_matching_attribute(state.mesh, lagrange::AttributeUsage::UV);
    la_runtime_assert(uv_id.has_value(), "No UV attribute found — mesh must have UVs for GUI");
    auto& uv_attr = state.mesh.get_attribute<float>(*uv_id);
    auto uv_mat = lagrange::matrix_view(uv_attr);
    std::vector<glm::vec2> uv_vec;
    uv_vec.reserve(state.mesh.get_num_corners());
    for (size_t c = 0; c < state.mesh.get_num_corners(); ++c) {
        uv_vec.push_back({uv_mat(c, 0), uv_mat(c, 1)});
    }
    state.ps_uv = state.ps_mesh->addParameterizationQuantity("uv", uv_vec);
    la_runtime_assert(state.ps_uv != nullptr, "Failed to register UV parameterization");
    state.ps_uv->setEnabled(true);

    // Pre-compute color pixel buffers: input textures first, then result.
    state.all_colors.clear();
    state.color_sizes.clear();
    for (const auto& tex : state.textures) {
        la_runtime_assert(tex.extent(2) >= 3, "Input texture must have at least 3 channels");
        state.all_colors.push_back(texture_to_colors(tex));
        state.color_sizes.push_back({tex.extent(0), tex.extent(1)});
    }
    la_runtime_assert(state.result.extent(2) >= 3, "Result must have at least 3 channels");
    state.all_colors.push_back(texture_to_colors(state.result));
    state.color_sizes.push_back({state.result.extent(0), state.result.extent(1)});

    // Pre-compute scalar pixel buffers for each weight map.
    state.all_scalars.clear();
    state.scalar_sizes.clear();
    for (const auto& wt : state.weights) {
        std::vector<float> scalars;
        scalars.reserve(wt.extent(0) * wt.extent(1));
        for (size_t jj = 0; jj < wt.extent(1); ++jj) {
            for (size_t ii = 0; ii < wt.extent(0); ++ii) {
                scalars.push_back(wt(ii, jj, 0));
            }
        }
        state.all_scalars.push_back(std::move(scalars));
        state.scalar_sizes.push_back({wt.extent(0), wt.extent(1)});
    }

    // Default: show the composited result. Register the scalar quantity (disabled) so the combo
    // can toggle to it without recreating the quantity, then enable the color view last so the
    // colored result is visible on open.
    state.color_idx = static_cast<int>(state.textures.size()); // last entry = result
    state.scalar_idx = 0;

    update_scalar_quantity(state); // registers scalar, then gets disabled below
    update_color_quantity(state); // enables color, disables scalar
}

void user_callback(DemoState& state)
{
    ImGui::Text(
        "Mesh: %d vertices, %d facets",
        static_cast<int>(state.mesh.get_num_vertices()),
        static_cast<int>(state.mesh.get_num_facets()));

    const auto num_textures = state.textures.size();
    la_runtime_assert(num_textures == state.weights.size());
    const auto texture_width = num_textures > 0 ? state.textures.front().extent(0) : 0;
    const auto texture_height = num_textures > 0 ? state.textures.front().extent(1) : 0;
    ImGui::Text(
        "Inputs: %d pairs (%dx%d)",
        static_cast<int>(num_textures),
        static_cast<int>(texture_width),
        static_cast<int>(texture_height));

    ImGui::Separator();

    for (const auto kk : lagrange::range(num_textures)) {
        const auto& weight = state.weights[kk];

        float weight_min = std::numeric_limits<float>::max();
        float weight_max = std::numeric_limits<float>::lowest();
        float weight_mean = 0.0f;
        for (const auto ii : lagrange::range(weight.extent(0))) {
            for (const auto jj : lagrange::range(weight.extent(1))) {
                const float v = weight(ii, jj, 0);
                weight_min = std::min(weight_min, v);
                weight_max = std::max(weight_max, v);
                weight_mean += v;
            }
        }
        weight_mean /= static_cast<float>(weight.extent(0) * weight.extent(1));

        ImGui::PushID(static_cast<int>(kk));

        ImGui::Text(
            "[%02zu]: min %.3f mean %.3f max %.3f",
            kk,
            weight_min,
            weight_mean,
            weight_max);
        ImGui::SameLine();

        if (ImGui::Button("texture")) {
            lagrange::logger().info("Switching to texture {}", kk);
            state.color_idx = static_cast<int>(kk);
            update_color_quantity(state);
        }
        ImGui::SameLine();

        if (ImGui::Button("weight")) {
            lagrange::logger().info("Switching to weight {}", kk);
            state.scalar_idx = static_cast<int>(kk);
            update_scalar_quantity(state);
        }

        ImGui::PopID();
    }

    ImGui::Separator();

    ImGui::Text(
        "Result: %dx%d",
        static_cast<int>(state.result.extent(0)),
        static_cast<int>(state.result.extent(1)));
    ImGui::SameLine();

    if (ImGui::Button("texture")) {
        lagrange::logger().info("Switching to output texture");
        state.color_idx = static_cast<int>(num_textures);
        update_color_quantity(state);
    }
}

} // namespace

int main(int argc, char** argv)
{
    struct
    {
        fs::path input_mesh;
        std::vector<fs::path> input_textures;
        std::vector<fs::path> input_weights;
        fs::path output_path = "output.exr";
        bool gui = false;
        bool sanity_check = false;
        int log_level = 2;
    } args;
    lagrange::texproc::CompositingOptions compositing_options;

    const auto named_gradient_normalizations =
        std::map<std::string, lagrange::texproc::GradientNormalization>{
            {"per-edge", lagrange::texproc::GradientNormalization::PerEdge},
            {"per-texel-sqrt", lagrange::texproc::GradientNormalization::PerTexelSqrt},
        };

    CLI::App app{argv[0]};
    app.option_defaults()->always_capture_default();
    app.add_option("--mesh-in", args.input_mesh, "Input mesh with UVs.")
        ->required()
        ->check(CLI::ExistingFile);
    app.add_option("--textures-in", args.input_textures, "Input textures images.")
        ->required()
        ->check(CLI::ExistingFile);
    app.add_option("--weights-in", args.input_weights, "Input weights images.")
        ->required()
        ->check(CLI::ExistingFile);
    app.add_option("--output", args.output_path, "Output file (texture or .glb/.gltf scene).");
    app.add_option(
        "--value-weight",
        compositing_options.value_weight,
        "Value interpolation weight.");
    app.add_option(
        "--quadrature",
        compositing_options.quadrature_samples,
        "Number of quadrature samples (in {1, 3, 6, 12, 24, 32}).");
    app.add_option(
        "--jitter-epsilon",
        compositing_options.jitter_epsilon,
        "Random jitter amount (0 if no jittering).");
    app.add_option(
           "--gradient-normalization",
           compositing_options.gradient_normalization,
           "How to normalize per-view gradient contributions (per-edge | per-texel-sqrt).")
        ->transform(CLI::CheckedTransformer(named_gradient_normalizations, CLI::ignore_case));
    app.add_flag(
        "--direct-solver,!--no-direct-solver",
        compositing_options.use_direct_solver,
        "Use direct LDLT solver instead of multigrid v-cycle.");
    app.add_option(
        "--regularization",
        compositing_options.stiffness_regularization_weight,
        "Laplacian regularization weight for the stiffness matrix.");
    app.add_flag(
        "--smooth-low-weight-areas",
        compositing_options.smooth_low_weight_areas,
        "Smooth pixels with low total weight (< 1) when using PerTexelSqrt normalization.");
    app.add_option(
        "--clamp",
        compositing_options.clamp_to_range,
        "Clamp out-of-range texels to the given range.");
    app.add_option(
        "--num-multigrid-levels",
        compositing_options.solver.num_multigrid_levels,
        "Number of multigrid levels (ignored when using the direct solver).");
    app.add_option(
        "--num-gauss-seidel-iterations",
        compositing_options.solver.num_gauss_seidel_iterations,
        "Number of Gauss-Seidel iterations per multigrid level, must be even (ignored when "
        "using the direct solver).");
    app.add_option(
        "--num-v-cycles",
        compositing_options.solver.num_v_cycles,
        "Number of V-cycles to perform (ignored when using the direct solver).");
    app.add_flag("--gui", args.gui, "Launch Polyscope GUI to visualize inputs and result.");
    app.add_flag("--sanity-check", args.sanity_check, "Enable solver sanity checks.");
    app.add_option("-l,--level", args.log_level, "Log level (0 = most verbose, 6 = off).");

    CLI11_PARSE(app, argc, argv)
    spdlog::set_level(static_cast<spdlog::level::level_enum>(args.log_level));
    if (args.sanity_check) {
        compositing_options.sanity_check = true;
    }

    // Sort input textures and weights
    sort_paths(args.input_textures);
    sort_paths(args.input_weights);

    lagrange::logger().info("Loading input mesh: {}", args.input_mesh.string());
    lagrange::io::LoadOptions load_options;
    load_options.stitch_vertices = true;
    auto mesh = lagrange::io::load_mesh<lagrange::SurfaceMesh32f>(args.input_mesh, load_options);

    lagrange::logger().info("Compositing {} textures", args.input_textures.size());
    std::vector<Array3Df> textures;
    std::vector<Array3Df> weights;
    std::vector<lagrange::texproc::ConstWeightedTextureView<float>> weighted_textures;
    la_runtime_assert(args.input_textures.size() == args.input_weights.size());
    for (const auto ii : lagrange::range(args.input_textures.size())) {
        textures.emplace_back(load_image(args.input_textures[ii]));
        weights.emplace_back(load_image(args.input_weights[ii]));
        weighted_textures.push_back({
            textures.back().to_mdspan(),
            extract_channel(weights.back().to_mdspan(), 0),
        });
    }

    lagrange::VerboseTimer timer("Texture compositing", nullptr, spdlog::level::level_enum::info);

    {
        const auto gradient_normalization_name = [&]() {
            for (const auto& [name, value] : named_gradient_normalizations) {
                if (value == compositing_options.gradient_normalization) {
                    return name;
                }
            }
            return std::string{"unknown"};
        }();

        const auto clamp_to_range_name = [&]() {
            if (compositing_options.clamp_to_range.has_value()) {
                const auto [min, max] = *compositing_options.clamp_to_range;
                return lagrange::format("[{:1.3f}, {:1.3f}]", min, max);
            } else {
                return std::string{"disabled"};
            }
        }();

        auto& logger = lagrange::logger();
        logger.debug("value_weight {:1.3e}", compositing_options.value_weight);
        logger.debug("quadrature_samples {}", compositing_options.quadrature_samples);
        logger.debug("jitter_epsilon {:1.3e}", compositing_options.jitter_epsilon);
        logger.debug("clamp_to_range {}", clamp_to_range_name);
        logger.debug("gradient_normalization {}", gradient_normalization_name);
        logger.debug("use_direct_solver {}", compositing_options.use_direct_solver);
        logger.debug(
            "stiffness_regularization_weight {:1.3e}",
            compositing_options.stiffness_regularization_weight);
        logger.debug("smooth_low_weight_areas {}", compositing_options.smooth_low_weight_areas);
        if (!compositing_options.use_direct_solver) {
            logger.debug(
                "num_multigrid_levels {}",
                compositing_options.solver.num_multigrid_levels);
            logger.debug(
                "num_gauss_seidel_iterations {}",
                compositing_options.solver.num_gauss_seidel_iterations);
            logger.debug("num_v_cycles {}", compositing_options.solver.num_v_cycles);
        }
    }

    timer.tick();
    auto output_texture =
        lagrange::texproc::texture_compositing(mesh, weighted_textures, compositing_options);
    timer.tock();

    if (!args.output_path.empty()) {
        auto extension = args.output_path.extension().string();
        if (extension == ".glb" || extension == ".gltf") {
            lagrange::logger().info("Saving scene: {}", args.output_path.string());
            auto scene =
                lagrange::scene::internal::single_mesh_to_scene(mesh, output_texture.to_mdspan());
            lagrange::io::SaveOptions save_options;
            save_options.encoding = lagrange::io::FileEncoding::Binary;
            save_options.embed_images = true;
            save_options.attribute_conversion_policy =
                lagrange::io::SaveOptions::AttributeConversionPolicy::ConvertAsNeeded;
            lagrange::io::save_scene(args.output_path, scene, save_options);
        } else {
            lagrange::logger().info("Saving texture: {}", args.output_path.string());
            save_image(args.output_path, output_texture.to_mdspan());
        }
    }

    if (args.gui) {
        polyscope::init();

        DemoState state;
        state.mesh = std::move(mesh);
        state.mesh_name = args.input_mesh.stem().string();
        state.textures = std::move(textures);
        state.weights = std::move(weights);
        state.result = std::move(output_texture);
        prepare_mesh_for_display(state.mesh);
        register_view(state);

        polyscope::options::configureImGuiStyleCallback = []() {
            ImGui::Spectrum::StyleColorsSpectrum();
            ImGui::Spectrum::LoadFont();
        };
        polyscope::state::userCallback = [&state]() { user_callback(state); };

        polyscope::show();
    }

    return 0;
}
