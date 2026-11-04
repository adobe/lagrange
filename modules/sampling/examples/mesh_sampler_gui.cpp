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

///
/// @file
/// Interactive polyscope demo for `sample_blue_noise`.
///
/// Lets the user tweak sampling parameters (number of samples, seed, bias, oversample ratio,
/// rmax factor) and switch between density modes (uniform, per-vertex attribute, callback)
/// in real time.
///

#include <lagrange/Attribute.h>
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMesh.h>
#include <lagrange/io/load_mesh.h>
#include <lagrange/polyscope/register_mesh.h>
#include <lagrange/polyscope/register_point_cloud.h>
#include <lagrange/sampling/sample_blue_noise.h>
#include <lagrange/utils/safe_cast.h>
#include <lagrange/views.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <imgui.h>
#include <polyscope/point_cloud.h>
#include <polyscope/polyscope.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <imgui_spectrum.h>

#include <CLI/CLI.hpp>

#include <cmath>
#include <string>

using Scalar = double;
using Index = uint32_t;
using SurfaceMesh = lagrange::SurfaceMesh<Scalar, Index>;

/// Application state shared between main() and the ImGui callback.
struct AppState
{
    using RowVector = Eigen::Matrix<Scalar, 1, Eigen::Dynamic>;

    // Input mesh (immutable after loading).
    SurfaceMesh mesh;

    // Density attribute id on input mesh (per-vertex, created once).
    lagrange::AttributeId density_attr_id = lagrange::invalid_attribute_id();

    // Longest bounding-box axis (0=x, 1=y, 2=z), its min value, and range.
    int longest_axis = 2;
    Scalar axis_min = 0;
    Scalar axis_range = 1;
    RowVector bbox_center = RowVector::Zero(3);

    // Sampling parameters (editable via UI).
    int num_samples = 4096;
    int seed = 0;
    float bias = 0.0f;
    float oversample_ratio = 8.0f;
    float rmax_factor = 4.0f;

    // Density mode: 0 = uniform, 1 = attribute, 2 = exp(x), 3 = exp(-r).
    int density_mode = 0;

    // True when parameters changed since the last sampling run.
    bool params_dirty = false;

    // Status message.
    std::string status_message = "Ready";
};

/// Run blue noise sampling with current parameters and update polyscope.
void run_sampling(AppState& state)
{
    lagrange::sampling::BlueNoiseSamplingOptions<Scalar, Index> options;
    options.num_samples = static_cast<size_t>(state.num_samples);
    options.seed = static_cast<size_t>(std::max(0, state.seed));
    options.bias = state.bias;
    options.oversample_ratio = state.oversample_ratio;
    options.rmax_factor = state.rmax_factor;

    switch (state.density_mode) {
    case 1: // Per-vertex attribute
        options.density = "density";
        break;
    case 2: // Callback exponential weighting along longest bbox axis
    {
        const size_t axis = static_cast<size_t>(state.longest_axis);
        const size_t dim = static_cast<size_t>(state.mesh.get_dimension());
        const Scalar a_min = state.axis_min;
        const Scalar a_range = state.axis_range;
        options.density = [axis, dim, a_min, a_range](
                              lagrange::span<const Scalar> positions,
                              lagrange::span<const Index> /*facet_indices*/,
                              lagrange::span<Scalar> output_densities) {
            for (size_t i = 0; i < output_densities.size(); ++i) {
                const Scalar coord = positions[i * dim + axis];
                const Scalar t = (coord - a_min) / a_range;
                output_densities[i] = std::exp(3.0 * t);
            }
        };
        break;
    }
    case 3: // Callback linear weighting wrt to distance to bbox center
    {
        // size_t axis = static_cast<size_t>(state.longest_axis);
        const size_t dim = static_cast<size_t>(state.mesh.get_dimension());
        const Scalar a_range = state.axis_range;
        const AppState::RowVector bbox_center = state.bbox_center;
        options.density = [dim, a_range, bbox_center](
                              lagrange::span<const Scalar> positions,
                              lagrange::span<const Index> /*facet_indices*/,
                              lagrange::span<Scalar> output_densities) {
            for (size_t i = 0; i < output_densities.size(); ++i) {
                AppState::RowVector coord(1, dim);
                std::copy(
                    positions.begin() + i * dim,
                    positions.begin() + (i + 1) * dim,
                    coord.data());
                const Scalar r = (coord - bbox_center).norm() / a_range;
                output_densities[i] = std::exp(-10.0 * r);
            }
        };
        break;
    }
    default: // Uniform (std::monostate, already the default)
        break;
    }

    try {
        auto result = lagrange::sampling::sample_blue_noise(state.mesh, options);

        const auto n = lagrange::safe_cast<Index>(result.facet_indices.size());

        // Build point cloud mesh.
        const auto dim = state.mesh.get_dimension();
        SurfaceMesh samples_mesh(dim);
        samples_mesh.add_vertices(n, [&](Index v, lagrange::span<Scalar> p) {
            for (Index d = 0; d < static_cast<Index>(dim); ++d) {
                p[d] = result.samples[v * dim + d];
            }
        });

        auto attr_id = samples_mesh.template create_attribute<Scalar>(
            "density",
            lagrange::AttributeElement::Vertex,
            lagrange::AttributeUsage::Scalar);
        auto& attr = samples_mesh.template ref_attribute<Scalar>(attr_id);
        for (Index i = 0; i < n; ++i) {
            attr.ref(i) = result.densities[i];
        }

        // Register with polyscope (replaces previous if it exists).
        auto* ps_cloud = lagrange::polyscope::register_point_cloud("samples", samples_mesh);
        ps_cloud->setPointRadius(0.0025);

        state.status_message = lagrange::format("Sampled {} points", n);
        state.params_dirty = false;
    } catch (const lagrange::Error& e) {
        state.status_message = lagrange::format("Sampling failed: {}", e.what());
        lagrange::logger().error(state.status_message);
        return;
    }
}

/// ImGui callback drawn every frame.
void draw_ui(AppState& state)
{
    bool changed = false;

    // --- Density mode ---
    if (ImGui::CollapsingHeader("Density Mode", ImGuiTreeNodeFlags_DefaultOpen)) {
        changed |= ImGui::RadioButton("Uniform", &state.density_mode, 0);
        ImGui::SameLine();
        changed |= ImGui::RadioButton("Attribute", &state.density_mode, 1);
        ImGui::SameLine();
        changed |= ImGui::RadioButton("Exponential(X)", &state.density_mode, 2);
        ImGui::SameLine();
        changed |= ImGui::RadioButton("Exponential(-R)", &state.density_mode, 3);
    }

    // --- Parameters ---
    if (ImGui::CollapsingHeader("Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
        changed |= ImGui::SliderInt("Num samples", &state.num_samples, 1, 10000);
        if (state.num_samples < 1) state.num_samples = 1;
        changed |= ImGui::InputInt("Seed", &state.seed);
        changed |= ImGui::SliderFloat("Bias", &state.bias, 0.0f, 1.0f, "%.2f");
        changed |=
            ImGui::SliderFloat("Oversample ratio", &state.oversample_ratio, 2.0f, 32.0f, "%.1f");
        changed |= ImGui::SliderFloat("Rmax factor", &state.rmax_factor, 1.0f, 16.0f, "%.1f");
    }

    if (changed) state.params_dirty = true;

    ImGui::Separator();

    // --- Run button (highlighted when parameters are dirty) ---
    bool was_dirty = state.params_dirty;
    if (was_dirty) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.4f, 0.1f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.5f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 0.6f, 0.3f, 1.0f));
    }
    if (ImGui::Button(was_dirty ? "Run Sampling *" : "Run Sampling", ImVec2(-1, 0))) {
        run_sampling(state);
    }
    if (was_dirty) {
        ImGui::PopStyleColor(3);
    }

    // --- Status ---
    ImGui::TextWrapped("%s", state.status_message.c_str());
}

int main(int argc, char* argv[])
{
    std::string input_path;

    CLI::App app("mesh_sampler — interactive blue noise sampling demo");
    app.option_defaults()->always_capture_default();
    app.add_option("input", input_path, "Input triangle mesh.")
        ->required()
        ->check(CLI::ExistingFile);
    CLI11_PARSE(app, argc, argv);

    AppState state;
    state.mesh = lagrange::io::load_mesh<SurfaceMesh>(input_path);
    lagrange::logger().info(
        "loaded mesh: {}v {}f",
        state.mesh.get_num_vertices(),
        state.mesh.get_num_facets());

    // Pre-compute a per-vertex density attribute for the attribute mode.
    // Uses exponential weighting along the longest bounding-box axis so the density contrast is
    // clearly visible on any mesh (including flat 2D meshes where z-range may be zero).
    {
        auto vertices = lagrange::vertex_view(state.mesh);

        // Find the longest bounding-box axis.
        const AppState::RowVector bb_min = vertices.colwise().minCoeff();
        const AppState::RowVector bb_max = vertices.colwise().maxCoeff();
        const AppState::RowVector bb_extent = bb_max - bb_min;
        Eigen::Index axis = 0;
        bb_extent.maxCoeff(&axis);

        Scalar a_min = bb_min(axis);
        Scalar a_range = bb_extent(axis);
        if (a_range == 0) a_range = 1; // degenerate: single point

        state.longest_axis = static_cast<int>(axis);
        state.axis_min = a_min;
        state.axis_range = a_range;
        state.bbox_center = 0.5 * (bb_min + bb_max);

        state.density_attr_id = state.mesh.template create_attribute<Scalar>(
            "density",
            lagrange::AttributeElement::Vertex,
            lagrange::AttributeUsage::Scalar);
        auto& density_attr = state.mesh.template ref_attribute<Scalar>(state.density_attr_id);
        for (Index v = 0; v < state.mesh.get_num_vertices(); ++v) {
            Scalar t = (vertices(v, axis) - a_min) / a_range;
            density_attr.ref(v) = std::exp(3.0 * t); // ~1 at min, ~20 at max
        }
    }

    // Initialize polyscope.
    polyscope::options::configureImGuiStyleCallback = []() {
        ImGui::Spectrum::StyleColorsSpectrum();
        ImGui::Spectrum::LoadFont();
    };
    polyscope::init();
    lagrange::polyscope::register_mesh("input", state.mesh);

    // Initial sampling.
    run_sampling(state);

    // Set up user callback for the UI panel.
    polyscope::state::userCallback = [&]() { draw_ui(state); };

    polyscope::show();

    return 0;
}
