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
#include "io_helpers.h"

#include <lagrange/Attribute.h>
#include <lagrange/AttributeTypes.h>
#include <lagrange/ExactPredicatesShewchuk.h>
#include <lagrange/IndexedAttribute.h>
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/cast_attribute.h>
#include <lagrange/find_matching_attributes.h>
#include <lagrange/foreach_attribute.h>
#include <lagrange/io/load_mesh.h>
#include <lagrange/io/save_mesh.h>
#include <lagrange/map_attribute.h>
#include <lagrange/polyscope/register_mesh.h>
#include <lagrange/texproc/texture_filtering.h>
#include <lagrange/texproc/texture_stitching.h>
#include <lagrange/triangulate_polygonal_facets.h>
#include <lagrange/utils/Error.h>
#include <lagrange/utils/fmt_eigen.h>
#include <lagrange/views.h>
#include <lagrange/weld_indexed_attribute.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <polyscope/polyscope.h>
#include <polyscope/curve_network.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <CLI/CLI.hpp>

using SurfaceMesh = lagrange::SurfaceMesh32d;
using Scalar = SurfaceMesh::Scalar;
using Index = SurfaceMesh::Index;


lagrange::AttributeId ensure_texcoord_id(SurfaceMesh& mesh)
{
    using namespace lagrange;

    // Get the texcoord id (and set the texcoords if they weren't already).
    AttributeId texcoord_id;

    // Check if the mesh comes with UVs.
    if (auto res = lagrange::find_matching_attribute(mesh, AttributeUsage::UV)) {
        texcoord_id = res.value();
    } else {
        la_runtime_assert(false, "Requires uv coordinates.");
    }

    // Make sure the UV coordinate type is the same as that of the vertices.
    if (!mesh.template is_attribute_type<Scalar>(texcoord_id)) {
        logger().warn(
            "Input uv coordinates do not have the same scalar type as the input points. "
            "Casting "
            "attribute.");
        texcoord_id = cast_attribute_in_place<Scalar>(mesh, texcoord_id);
    }

    // Make sure the UV coordinates are indexed.
    if (mesh.get_attribute_base(texcoord_id).get_element_type() != AttributeElement::Indexed) {
        logger().warn("UV coordinates are not indexed. Welding.");
        texcoord_id = map_attribute_in_place(mesh, texcoord_id, AttributeElement::Indexed);
        weld_indexed_attribute(mesh, texcoord_id);
    }

    // Check that the number of corners is equal to (K+1) times the number of simplices.
    // Check mesh is triangular.
    la_runtime_assert(
        mesh.get_num_corners() == mesh.get_num_facets() * (2 + 1),
        "Number of corners doesn't match the number of simplices");
    la_runtime_assert(mesh.is_triangle_mesh());

    return texcoord_id;
}

struct OrientReturn
{
    std::string texcoord_name;
    size_t num_negative_tris;
    size_t num_zero_tris;
    size_t num_positive_tris;

    bool all_ok() const
    {
        if (num_negative_tris != 0) return false;
        if (num_zero_tris != 0) return false;
        return num_positive_tris > 0;
    }
};

OrientReturn add_uv_orient_attribute(SurfaceMesh& mesh, std::string_view orient_name)
{
    using namespace lagrange;

    // Create uv facet orientation attribute.
    const auto orient_id = mesh.create_attribute<Scalar>(
        orient_name,
        lagrange::AttributeElement::Facet,
        1,
        AttributeUsage::Scalar);
    auto& orient_attr = mesh.ref_attribute<Scalar>(orient_id);
    auto orients = matrix_ref(orient_attr);
    la_runtime_assert(orients.rows() == mesh.get_num_facets());
    la_runtime_assert(orients.cols() == 1);

    // Retrieve parametrization.
    const auto texcoord_id = ensure_texcoord_id(mesh);
    la_runtime_assert(mesh.is_attribute_indexed(texcoord_id));
    const auto& texcoord_attr = mesh.get_indexed_attribute<Scalar>(texcoord_id);
    const auto texcoord_indices = reshaped_view(texcoord_attr.indices(), 3);
    const auto texcoord_values = matrix_view(texcoord_attr.values());
    lagrange::logger().debug(
        "texcoord_indices {}x{}",
        texcoord_indices.rows(),
        texcoord_indices.cols());
    lagrange::logger().debug(
        "texcoord_values {}x{}",
        texcoord_values.rows(),
        texcoord_values.cols());
    la_runtime_assert(texcoord_indices.cols() == 3);
    la_runtime_assert(texcoord_indices.maxCoeff() < static_cast<Index>(texcoord_values.rows()));
    la_runtime_assert(texcoord_values.cols() == 2);

    // Compute orientations.
    ExactPredicatesShewchuk predicates;
    std::array<size_t, 3> orient_to_counts;
    orient_to_counts.fill(0);
    for (Index f = 0; f < mesh.get_num_facets(); ++f) {
        la_debug_assert(texcoord_indices(f, 2) < static_cast<Index>(texcoord_values.rows()));
        Eigen::RowVector2d p0 = texcoord_values.row(texcoord_indices(f, 0)).template cast<double>();
        Eigen::RowVector2d p1 = texcoord_values.row(texcoord_indices(f, 1)).template cast<double>();
        Eigen::RowVector2d p2 = texcoord_values.row(texcoord_indices(f, 2)).template cast<double>();
        const auto rr = predicates.orient2D(p0.data(), p1.data(), p2.data());
        orients(f) = static_cast<Scalar>(rr);
        orient_to_counts[rr + 1] += 1;
    }

    // Dump stats.
    for (size_t rr = 0; rr < 3; ++rr) {
        logger().debug("orient {} count {}", rr - 1, orient_to_counts[rr]);
    }

    std::string name{mesh.get_attribute_name(texcoord_id)};

    return OrientReturn{
        name,
        orient_to_counts[0],
        orient_to_counts[1],
        orient_to_counts[2],
    };
}

using Array3Df = lagrange::image::experimental::Array3D<float>;
using ConstView3Df = lagrange::image::experimental::View3D<const float>;

auto register_color_texture(
    polyscope::SurfaceMesh* ps_mesh,
    polyscope::SurfaceCornerParameterizationQuantity* ps_tex,
    std::string label,
    ConstView3Df texture) -> polyscope::SurfaceTextureColorQuantity*
{
    la_runtime_assert(ps_mesh);
    la_runtime_assert(ps_tex);
    la_runtime_assert(texture.extent(2) >= 3);
    la_runtime_assert(texture.size() > 0);

    std::vector<glm::vec3> colors;
    colors.reserve(texture.extent(0) * texture.extent(1));
    for (const auto jj : lagrange::range(texture.extent(1))) {
        for (const auto ii : lagrange::range(texture.extent(0))) {
            const auto rr = texture(ii, jj, 0);
            const auto gg = texture(ii, jj, 1);
            const auto bb = texture(ii, jj, 2);
            const auto color = glm::vec3(rr, gg, bb);
            colors.emplace_back(color);
        }
    }

    la_runtime_assert(ps_mesh);
    la_runtime_assert(ps_tex);
    polyscope::SurfaceTextureColorQuantity* colors_ = ps_mesh->addTextureColorQuantity(
        label,
        *ps_tex,
        texture.extent(1),
        texture.extent(0),
        colors,
        polyscope::ImageOrigin::UpperLeft);
    la_runtime_assert(colors_);
    colors_->setFilterMode(polyscope::FilterMode::Nearest);
    colors_->setEnabled(true);

    return colors_;
}

struct UiState
{
    OrientReturn orient_ret;
    lagrange::texproc::StitchingOptions stitching_options;
    lagrange::texproc::FilteringOptions filtering_options;
    SurfaceMesh mesh;
    Array3Df input_texture;
    Array3Df stitched_texture;
    Array3Df filtered_texture;
    polyscope::SurfaceMesh* ps_mesh;
    polyscope::SurfaceCornerParameterizationQuantity* ps_tex;

    bool has_valid_inputs() const;
    void main_panel();
};

bool UiState::has_valid_inputs() const
{
    bool can_stitch = true;
    can_stitch &= mesh.get_num_facets() > 0;
    can_stitch &= orient_ret.all_ok();
    can_stitch &= input_texture.size() > 0;
    can_stitch &= static_cast<bool>(ps_mesh);
    can_stitch &= static_cast<bool>(ps_tex);
    return can_stitch;
}

template <typename... Args>
void ImGui_FmtText(fmt::format_string<Args...> text, Args&&... args)
{
    ImGui::Text("%s", fmt::format(text, std::forward<Args>(args)...).c_str());
}

void UiState::main_panel()
{
    // mesh stat
    ImGui_FmtText("mesh: {}v {}f", mesh.get_num_vertices(), mesh.get_num_facets());

    // uv face orientations
    ImGui_FmtText("parametrization: {}", orient_ret.texcoord_name);
    ImGui_FmtText(
        "uv orientation: #neg:{}, #zero:{}, #pos:{}",
        orient_ret.num_negative_tris,
        orient_ret.num_zero_tris,
        orient_ret.num_positive_tris);

    // texture stat
    ImGui_FmtText(
        "texture: {}x{}x{}",
        input_texture.extent(0),
        input_texture.extent(1),
        input_texture.extent(2));

    const auto valid_inputs = has_valid_inputs();
    ImGui_FmtText("{} inputs", valid_inputs ? "valid" : "invalid / missing");

    ImGui::Separator();

    // trigger stitching
    if (ImGui::Button("run texture stitching") && valid_inputs) {
        lagrange::logger().info("Running texture stitching");
        auto result_texture = input_texture;
        lagrange::texproc::texture_stitching(mesh, result_texture.to_mdspan(), stitching_options);
        stitched_texture = result_texture;
        register_color_texture(ps_mesh, ps_tex, "stitched", result_texture.to_mdspan());
        lagrange::logger().info("Done");
    }

    // stitching options
    if (ImGui::TreeNode("texture stitching options")) {
        auto& options = stitching_options;

        {
            const std::array<const char*, 5> labels = {
                "1",
                "3",
                "6 (default)",
                "12",
                "24",
            };
            const std::array<const unsigned int, 5> values = {
                1,
                3,
                6,
                12,
                24,
            };
            la_runtime_assert(labels.size() == values.size());
            auto index = 0;
            for (const auto& value : values) {
                if (value == options.quadrature_samples) break;
                index += 1;
            }
            la_runtime_assert(static_cast<size_t>(index) < values.size());
            ImGui::Combo("quadrature", &index, labels.data(), static_cast<int>(labels.size()));
            options.quadrature_samples = values.at(index);
        }

        {
            options.stiffness_regularization_weight =
                std::max(options.stiffness_regularization_weight, 1e-9);
            la_runtime_assert(options.stiffness_regularization_weight > 0);
            float value_log = std::log(options.stiffness_regularization_weight) / std::log(10.0f);
            ImGui::SliderFloat("log stiffness regularization", &value_log, -9.0f, 0.0f);
            options.stiffness_regularization_weight = std::pow(10.0f, value_log);
        }

        ImGui::Checkbox("exterior only", &options.exterior_only);

        {
            bool has_jitter = options.jitter_epsilon > 0;
            ImGui::Checkbox("jitter", &has_jitter);
            options.jitter_epsilon = has_jitter ? 1e-4 : 0.0;
        }

        {
            bool is_clamped = options.clamp_to_range.has_value();
            ImGui::Checkbox("clamp", &is_clamped);
            options.clamp_to_range =
                is_clamped ? std::make_optional(std::pair<double, double>{0.0, 1.0}) : std::nullopt;
        }

        if (ImGui::Button("Reset")) options = {};

        ImGui::TreePop();
    }


    ImGui::Separator();

    // trigger filtering
    if (ImGui::Button("run texture filtering") && valid_inputs) {
        lagrange::logger().info("Running texture filtering");
        auto result_texture = input_texture;
        lagrange::texproc::texture_filtering(mesh, result_texture.to_mdspan(), filtering_options);
        filtered_texture = result_texture;
        register_color_texture(ps_mesh, ps_tex, "filtered", filtered_texture.to_mdspan());
        lagrange::logger().info("Done");
    }

    // filtering options
    if (ImGui::TreeNode("texture filtering options")) {
        auto& options = filtering_options;

        {
            const std::array<const char*, 5> labels = {
                "1",
                "3",
                "6 (default)",
                "12",
                "24",
            };
            const std::array<const unsigned int, 5> values = {
                1,
                3,
                6,
                12,
                24,
            };
            la_runtime_assert(labels.size() == values.size());
            auto index = 0;
            for (const auto& value : values) {
                if (value == options.quadrature_samples) break;
                index += 1;
            }
            la_runtime_assert(static_cast<size_t>(index) < values.size());
            ImGui::Combo("quadrature", &index, labels.data(), static_cast<int>(labels.size()));
            options.quadrature_samples = values.at(index);
        }

        {
            float weight = options.gradient_scale;
            ImGui::SliderFloat("scale", &weight, 0.0f, 10.0);
            options.gradient_scale = weight;
        }

        {
            options.value_weight = std::max(options.value_weight, 1e0);
            la_runtime_assert(options.value_weight > 0);
            float weight_log = std::log(options.value_weight) / std::log(10.0f);
            ImGui::SliderFloat("log value weight", &weight_log, 0.0f, 5.0f);
            options.value_weight = std::pow(10.0f, weight_log);
        }

        {
            options.gradient_weight = std::max(options.gradient_weight, 1e0);
            la_runtime_assert(options.gradient_weight > 0);
            float weight_log = std::log(options.gradient_weight) / std::log(10.0f);
            ImGui::SliderFloat("log grad weight", &weight_log, 0.0f, 5.0f);
            options.gradient_weight = std::pow(10.0f, weight_log);
        }

        {
            bool has_jitter = options.jitter_epsilon > 0;
            ImGui::Checkbox("jitter", &has_jitter);
            options.jitter_epsilon = has_jitter ? 1e-4 : 0.0;
        }

        {
            bool is_clamped = options.clamp_to_range.has_value();
            ImGui::Checkbox("clamp", &is_clamped);
            options.clamp_to_range =
                is_clamped ? std::make_optional(std::pair<double, double>{0.0, 1.0}) : std::nullopt;
        }

        if (ImGui::Button("Reset")) options = {};

        ImGui::TreePop();
    }
}

int main(int argc, char** argv)
{
    struct
    {
        lagrange::fs::path mesh_path;
        lagrange::fs::path texture_path;
        std::string orient_name = "texcoord_orient";
        int log_level = 2; // normal
    } args;

    CLI::App app{argv[0]};
    app.option_defaults()->always_capture_default();
    app.add_option("mesh", args.mesh_path, "Input mesh.")->required()->check(CLI::ExistingFile);
    app.add_option("texture", args.texture_path, "Input texture.")->check(CLI::ExistingFile);
    app.add_option("--orient-name", args.orient_name, "UV orientation attribute.");
    app.add_option("-l,--level", args.log_level, "Log level (0 = most verbose, 6 = off).");
    CLI11_PARSE(app, argc, argv)

    spdlog::set_level(static_cast<spdlog::level::level_enum>(args.log_level));

    polyscope::options::configureImGuiStyleCallback = []() {
        ImGui::Spectrum::StyleColorsSpectrum();
        ImGui::Spectrum::LoadFont();
    };
    polyscope::init();

    UiState ui_state;
    polyscope::state::userCallback = [&]() { ui_state.main_panel(); };

    const auto& mesh_path = args.mesh_path;
    lagrange::logger().info("Loading input mesh: {}", mesh_path.string());
    lagrange::io::LoadOptions options;
    options.stitch_vertices = true;
    auto mesh = lagrange::io::load_mesh<::SurfaceMesh>(mesh_path, options);

    lagrange::triangulate_polygonal_facets(mesh);
    ui_state.orient_ret = add_uv_orient_attribute(mesh, args.orient_name);

    // list mesh attributes imported by polyscope module
    if (lagrange::logger().should_log(spdlog::level::debug))
        seq_foreach_named_attribute_read(mesh, [&](auto name, auto&& attr) {
            using AttributeType = std::decay_t<decltype(attr)>;
            const bool is_indexed = AttributeType::IsIndexed;
            const bool is_reserved = mesh.attr_name_is_reserved(name);
            const bool all_ok = !is_reserved && !is_indexed;
            lagrange::logger().debug(
                "attr \"{}\" {}{}{}",
                name,
                is_reserved ? "reserved " : "",
                is_indexed ? "indexed " : "",
                all_ok ? "IMPORT" : "SKIP");
        });

    lagrange::logger().debug(
        "mesh {}v {}f {}c",
        mesh.get_num_vertices(),
        mesh.get_num_facets(),
        mesh.get_num_corners());
    la_runtime_assert(mesh.is_triangle_mesh());
    la_runtime_assert(mesh.get_num_facets() * 3 == mesh.get_num_corners());
    ui_state.mesh = mesh;

    const auto name = mesh_path.stem().string();
    polyscope::SurfaceMesh* ps_mesh = lagrange::polyscope::register_mesh(name, mesh);
    la_runtime_assert(ps_mesh);

    ps_mesh->setBackFacePolicy(::polyscope::BackFacePolicy::Custom);
    ps_mesh->setBackFaceColor(glm::vec3(1.0, 0.0, 0.0));
    ps_mesh->setSmoothShade(true);

    polyscope::SurfaceCornerParameterizationQuantity* ps_tex = nullptr;
    {
        // register texcoord parametrization
        la_runtime_assert(mesh.is_triangle_mesh());
        auto texcoord_id = ensure_texcoord_id(mesh);
        lagrange::logger().info(
            "Registering corner parametrization: {}",
            mesh.get_attribute_name(texcoord_id));
        la_runtime_assert(mesh.is_attribute_indexed(texcoord_id));

        const auto& texcoord_attr = mesh.get_indexed_attribute<Scalar>(texcoord_id);
        const auto texcoord_indices = reshaped_view(texcoord_attr.indices(), 3);
        const auto texcoord_values = matrix_view(texcoord_attr.values());
        lagrange::logger().debug(
            "texcoord_indices {}x{}",
            texcoord_indices.rows(),
            texcoord_indices.cols());
        lagrange::logger().debug(
            "texcoord_values {}x{}",
            texcoord_values.rows(),
            texcoord_values.cols());
        la_runtime_assert(texcoord_indices.cols() == 3);
        la_runtime_assert(texcoord_indices.maxCoeff() < static_cast<Index>(texcoord_values.rows()));
        la_runtime_assert(texcoord_values.cols() == 2);

        la_runtime_assert(ps_mesh);
        la_runtime_assert(mesh.get_num_vertices() == static_cast<Index>(ps_mesh->nVertices()));
        la_runtime_assert(mesh.get_num_facets() == static_cast<Index>(ps_mesh->nFaces()));
        la_runtime_assert(mesh.get_num_corners() == static_cast<Index>(ps_mesh->nCorners()));

        std::vector<glm::vec2> uv_values_;
        la_runtime_assert(ps_mesh->nCorners() == ps_mesh->nFaces() * 3);
        la_runtime_assert(static_cast<size_t>(texcoord_indices.size()) == ps_mesh->nCorners());
        uv_values_.reserve(ps_mesh->nCorners());
        for (const auto cc : lagrange::range(ps_mesh->nCorners())) {
            const auto index = texcoord_indices(cc / 3, cc % 3);
            la_runtime_assert(index < texcoord_values.rows());
            const auto uu = texcoord_values(index, 0);
            const auto vv = texcoord_values(index, 1);
            const auto uv = glm::vec2(uu, vv);
            uv_values_.emplace_back(uv);
        }

        ps_tex = ps_mesh->addParameterizationQuantity("texcoord", uv_values_);
        la_runtime_assert(ps_tex);
        ps_tex->setEnabled(true);
    }

    polyscope::CurveNetwork* ps_seam_network;
    {
        // texproc parametrization seams
        la_runtime_assert(ps_tex);
        ps_seam_network = ps_tex->createCurveNetworkFromSeams("texcoord_seams");
        la_runtime_assert(ps_seam_network);
        ps_seam_network->setEnabled(true);
    }

    polyscope::SurfaceTextureColorQuantity* ps_input_colors = nullptr;
    if (lagrange::fs::exists(args.texture_path)) {
        lagrange::logger().info("Loading input texture: {}", args.texture_path.string());
        const auto texture = load_image(args.texture_path);

        lagrange::logger()
            .debug("texture {}x{}x{}", texture.extent(0), texture.extent(1), texture.extent(2));
        la_runtime_assert(texture.extent(2) >= 3);
        la_runtime_assert(texture.size() > 0);
        ui_state.input_texture = texture;
        ps_input_colors = register_color_texture(ps_mesh, ps_tex, "input", texture.to_mdspan());
    }
    (void)ps_input_colors;

    la_runtime_assert(ps_mesh);
    la_runtime_assert(ps_tex);
    la_runtime_assert(ps_seam_network);
    ui_state.ps_mesh = ps_mesh;
    ui_state.ps_tex = ps_tex;

    lagrange::logger().info("Starting main loop");
    polyscope::show();

    return 0;
}
