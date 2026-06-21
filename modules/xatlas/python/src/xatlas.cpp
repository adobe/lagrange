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
#include <lagrange/python/binding.h>

#include <lagrange/xatlas/repack_mesh.h>
#include <lagrange/xatlas/repack_scene.h>
#include <lagrange/xatlas/unwrap_mesh.h>
#include <lagrange/xatlas/unwrap_scene.h>

#include <string_view>

namespace lagrange::python {

namespace nb = nanobind;

namespace {

using namespace lagrange::xatlas;

UnwrapOptions make_unwrap_options(
    // ChartOptions
    float max_chart_area,
    float max_boundary_length,
    float normal_deviation_weight,
    float roundness_weight,
    float straightness_weight,
    float normal_seam_weight,
    float texture_seam_weight,
    float max_cost,
    uint32_t max_iterations,
    bool use_input_mesh_uvs,
    bool fix_winding,
    // PackOptions
    uint32_t max_chart_size,
    uint32_t padding,
    float texels_per_unit,
    uint32_t resolution,
    bool bilinear,
    bool block_align,
    bool brute_force,
    bool rotate_charts_to_axis,
    bool rotate_charts,
    // UnwrapOptions
    std::string_view input_normal_attribute_name,
    std::string_view input_uv_attribute_name,
    std::string_view output_uv_attribute_name,
    std::string_view output_atlas_attribute_name,
    std::string_view output_chart_attribute_name,
    MultiAtlasPolicy multi_atlas_policy,
    bool enable_sharing_uvs_between_instances)
{
    UnwrapOptions opts;
    opts.chart.max_chart_area = max_chart_area;
    opts.chart.max_boundary_length = max_boundary_length;
    opts.chart.normal_deviation_weight = normal_deviation_weight;
    opts.chart.roundness_weight = roundness_weight;
    opts.chart.straightness_weight = straightness_weight;
    opts.chart.normal_seam_weight = normal_seam_weight;
    opts.chart.texture_seam_weight = texture_seam_weight;
    opts.chart.max_cost = max_cost;
    opts.chart.max_iterations = max_iterations;
    opts.chart.use_input_mesh_uvs = use_input_mesh_uvs;
    opts.chart.fix_winding = fix_winding;
    opts.packing.max_chart_size = max_chart_size;
    opts.packing.padding = padding;
    opts.packing.texels_per_unit = texels_per_unit;
    opts.packing.resolution = resolution;
    opts.packing.bilinear = bilinear;
    opts.packing.block_align = block_align;
    opts.packing.brute_force = brute_force;
    opts.packing.rotate_charts_to_axis = rotate_charts_to_axis;
    opts.packing.rotate_charts = rotate_charts;
    opts.input_normal_attribute_name = input_normal_attribute_name;
    opts.input_uv_attribute_name = input_uv_attribute_name;
    opts.output_uv_attribute_name = output_uv_attribute_name;
    opts.output_atlas_attribute_name = output_atlas_attribute_name;
    opts.output_chart_attribute_name = output_chart_attribute_name;
    opts.multi_atlas_policy = multi_atlas_policy;
    opts.enable_sharing_uvs_between_instances = enable_sharing_uvs_between_instances;
    return opts;
}

RepackOptions make_repack_options(
    // PackOptions
    uint32_t max_chart_size,
    uint32_t padding,
    float texels_per_unit,
    uint32_t resolution,
    bool bilinear,
    bool block_align,
    bool brute_force,
    bool rotate_charts_to_axis,
    bool rotate_charts,
    // RepackOptions
    std::string_view input_uv_attribute_name,
    std::string_view input_chart_attribute_name,
    std::string_view output_uv_attribute_name,
    std::string_view output_atlas_attribute_name,
    std::string_view output_chart_attribute_name,
    MultiAtlasPolicy multi_atlas_policy,
    bool enable_sharing_uvs_between_instances)
{
    RepackOptions opts;
    opts.packing.max_chart_size = max_chart_size;
    opts.packing.padding = padding;
    opts.packing.texels_per_unit = texels_per_unit;
    opts.packing.resolution = resolution;
    opts.packing.bilinear = bilinear;
    opts.packing.block_align = block_align;
    opts.packing.brute_force = brute_force;
    opts.packing.rotate_charts_to_axis = rotate_charts_to_axis;
    opts.packing.rotate_charts = rotate_charts;
    opts.input_uv_attribute_name = input_uv_attribute_name;
    opts.input_chart_attribute_name = input_chart_attribute_name;
    opts.output_uv_attribute_name = output_uv_attribute_name;
    opts.output_atlas_attribute_name = output_atlas_attribute_name;
    opts.output_chart_attribute_name = output_chart_attribute_name;
    opts.multi_atlas_policy = multi_atlas_policy;
    opts.enable_sharing_uvs_between_instances = enable_sharing_uvs_between_instances;
    return opts;
}

} // namespace

void populate_xatlas_module(nb::module_& m)
{
    using Scalar = double;
    using Index = uint32_t;
    using namespace nb::literals;

    nb::enum_<MultiAtlasPolicy>(m, "MultiAtlasPolicy", "How to encode multi-atlas output.")
        .value(
            "NormalizePerTile",
            MultiAtlasPolicy::NormalizePerTile,
            "UVs are normalized inside each atlas tile; tile id is stored separately.")
        .value(
            "Udim",
            MultiAtlasPolicy::Udim,
            "UVs include integer U offset = atlasIndex; V stays normalized in [0, 1].")
        .value(
            "ErrorIfMultiple",
            MultiAtlasPolicy::ErrorIfMultiple,
            "Throw if xatlas produces more than one atlas tile.")
        .export_values();

    static const ChartOptions chart_defaults{};
    static const PackOptions pack_defaults{};
    static const UnwrapOptions unwrap_defaults{};
    static const RepackOptions repack_defaults{};

    m.def(
        "unwrap_mesh",
        [](const SurfaceMesh<Scalar, Index>& mesh,
           // ChartOptions
           float max_chart_area,
           float max_boundary_length,
           float normal_deviation_weight,
           float roundness_weight,
           float straightness_weight,
           float normal_seam_weight,
           float texture_seam_weight,
           float max_cost,
           uint32_t max_iterations,
           bool use_input_mesh_uvs,
           bool fix_winding,
           // PackOptions
           uint32_t max_chart_size,
           uint32_t padding,
           float texels_per_unit,
           uint32_t resolution,
           bool bilinear,
           bool block_align,
           bool brute_force,
           bool rotate_charts_to_axis,
           bool rotate_charts,
           // UnwrapOptions
           std::string_view input_normal_attribute_name,
           std::string_view input_uv_attribute_name,
           std::string_view output_uv_attribute_name,
           std::string_view output_atlas_attribute_name,
           std::string_view output_chart_attribute_name,
           MultiAtlasPolicy multi_atlas_policy,
           bool enable_sharing_uvs_between_instances) {
            auto opts = make_unwrap_options(
                max_chart_area,
                max_boundary_length,
                normal_deviation_weight,
                roundness_weight,
                straightness_weight,
                normal_seam_weight,
                texture_seam_weight,
                max_cost,
                max_iterations,
                use_input_mesh_uvs,
                fix_winding,
                max_chart_size,
                padding,
                texels_per_unit,
                resolution,
                bilinear,
                block_align,
                brute_force,
                rotate_charts_to_axis,
                rotate_charts,
                input_normal_attribute_name,
                input_uv_attribute_name,
                output_uv_attribute_name,
                output_atlas_attribute_name,
                output_chart_attribute_name,
                multi_atlas_policy,
                enable_sharing_uvs_between_instances);
            return unwrap_mesh(mesh, opts);
        },
        "mesh"_a,
        nb::kw_only(),
        "max_chart_area"_a = chart_defaults.max_chart_area,
        "max_boundary_length"_a = chart_defaults.max_boundary_length,
        "normal_deviation_weight"_a = chart_defaults.normal_deviation_weight,
        "roundness_weight"_a = chart_defaults.roundness_weight,
        "straightness_weight"_a = chart_defaults.straightness_weight,
        "normal_seam_weight"_a = chart_defaults.normal_seam_weight,
        "texture_seam_weight"_a = chart_defaults.texture_seam_weight,
        "max_cost"_a = chart_defaults.max_cost,
        "max_iterations"_a = chart_defaults.max_iterations,
        "use_input_mesh_uvs"_a = chart_defaults.use_input_mesh_uvs,
        "fix_winding"_a = chart_defaults.fix_winding,
        "max_chart_size"_a = pack_defaults.max_chart_size,
        "padding"_a = pack_defaults.padding,
        "texels_per_unit"_a = pack_defaults.texels_per_unit,
        "resolution"_a = pack_defaults.resolution,
        "bilinear"_a = pack_defaults.bilinear,
        "block_align"_a = pack_defaults.block_align,
        "brute_force"_a = pack_defaults.brute_force,
        "rotate_charts_to_axis"_a = pack_defaults.rotate_charts_to_axis,
        "rotate_charts"_a = pack_defaults.rotate_charts,
        "input_normal_attribute_name"_a = "",
        "input_uv_attribute_name"_a = "",
        "output_uv_attribute_name"_a = unwrap_defaults.output_uv_attribute_name,
        "output_atlas_attribute_name"_a = unwrap_defaults.output_atlas_attribute_name,
        "output_chart_attribute_name"_a = unwrap_defaults.output_chart_attribute_name,
        "multi_atlas_policy"_a = unwrap_defaults.multi_atlas_policy,
        "enable_sharing_uvs_between_instances"_a =
            unwrap_defaults.enable_sharing_uvs_between_instances,
        R"(Unwrap a single mesh using xatlas (segmentation + parameterization + packing).

The mesh must be triangle-only. All option fields are passed as keyword arguments; defaults match
those of the underlying xatlas C library.

:param mesh: Mesh to unwrap.

Chart options (segmentation):

:param max_chart_area: Maximum chart area in input units squared. 0 = no limit.
:param max_boundary_length: Maximum chart boundary length. 0 = no limit.
:param normal_deviation_weight: Cost weight on normal deviation; higher = flatter charts.
:param roundness_weight: Cost weight on roundness; higher = more disk-like charts.
:param straightness_weight: Cost weight on boundary straightness.
:param normal_seam_weight: Cost weight on crossing normal seams. >1000 forbids crossing.
:param texture_seam_weight: Cost weight on crossing input UV seams.
:param max_cost: Maximum cost; faces beyond this are not merged.
:param max_iterations: Number of segmentation iterations.
:param use_input_mesh_uvs: If True, treat the input UV attribute as the chart layout.
:param fix_winding: If True, ensure consistent triangle winding within charts.

Pack options:

:param max_chart_size: Maximum chart size in pixels. 0 = no limit.
:param padding: Empty pixels of padding around each chart.
:param texels_per_unit: Texel-to-world ratio. 0 = auto-estimate.
:param resolution: Atlas tile resolution in pixels. 0 = single tile sized by texels_per_unit.
:param bilinear: If True, leave a 1-pixel margin for safe bilinear filtering.
:param block_align: If True, align charts to 4-pixel blocks (BCn-friendly).
:param brute_force: If True, brute-force optimal placement (slow).
:param rotate_charts_to_axis: Rotate charts so longest axis aligns with U or V.
:param rotate_charts: Allow xatlas to rotate charts during packing.

Output options:

:param input_normal_attribute_name: Optional vertex normal attribute used as a chart hint.
:param input_uv_attribute_name: Optional UV attribute (vertex or indexed) used as a chart hint.
:param output_uv_attribute_name: Output indexed UV attribute name.
:param output_atlas_attribute_name: Corner-attribute name for the per-corner atlas tile id.
:param output_chart_attribute_name: Corner-attribute name for the per-corner chart id.
:param multi_atlas_policy: How to encode multi-atlas output (see MultiAtlasPolicy).
:param enable_sharing_uvs_between_instances: Scene-only; ignored for single-mesh unwrap.

:returns: A new mesh with the indexed UV attribute (and optional atlas/chart attributes).
)");

    m.def(
        "repack_mesh",
        [](const SurfaceMesh<Scalar, Index>& mesh,
           uint32_t max_chart_size,
           uint32_t padding,
           float texels_per_unit,
           uint32_t resolution,
           bool bilinear,
           bool block_align,
           bool brute_force,
           bool rotate_charts_to_axis,
           bool rotate_charts,
           std::string_view input_uv_attribute_name,
           std::string_view input_chart_attribute_name,
           std::string_view output_uv_attribute_name,
           std::string_view output_atlas_attribute_name,
           std::string_view output_chart_attribute_name,
           MultiAtlasPolicy multi_atlas_policy,
           bool enable_sharing_uvs_between_instances) {
            auto opts = make_repack_options(
                max_chart_size,
                padding,
                texels_per_unit,
                resolution,
                bilinear,
                block_align,
                brute_force,
                rotate_charts_to_axis,
                rotate_charts,
                input_uv_attribute_name,
                input_chart_attribute_name,
                output_uv_attribute_name,
                output_atlas_attribute_name,
                output_chart_attribute_name,
                multi_atlas_policy,
                enable_sharing_uvs_between_instances);
            return repack_mesh(mesh, opts);
        },
        "mesh"_a,
        nb::kw_only(),
        "max_chart_size"_a = pack_defaults.max_chart_size,
        "padding"_a = pack_defaults.padding,
        "texels_per_unit"_a = pack_defaults.texels_per_unit,
        "resolution"_a = pack_defaults.resolution,
        "bilinear"_a = pack_defaults.bilinear,
        "block_align"_a = pack_defaults.block_align,
        "brute_force"_a = pack_defaults.brute_force,
        "rotate_charts_to_axis"_a = pack_defaults.rotate_charts_to_axis,
        "rotate_charts"_a = pack_defaults.rotate_charts,
        "input_uv_attribute_name"_a = repack_defaults.input_uv_attribute_name,
        "input_chart_attribute_name"_a = repack_defaults.input_chart_attribute_name,
        "output_uv_attribute_name"_a = repack_defaults.output_uv_attribute_name,
        "output_atlas_attribute_name"_a = repack_defaults.output_atlas_attribute_name,
        "output_chart_attribute_name"_a = repack_defaults.output_chart_attribute_name,
        "multi_atlas_policy"_a = repack_defaults.multi_atlas_policy,
        "enable_sharing_uvs_between_instances"_a =
            repack_defaults.enable_sharing_uvs_between_instances,
        R"(Repack the existing UV islands of a mesh using xatlas.

The mesh must be triangle-only and have an existing indexed UV attribute. All option fields are
passed as keyword arguments.

:param mesh: Triangle mesh with an existing indexed UV attribute.
:param max_chart_size: See PackOptions.max_chart_size.
:param padding: See PackOptions.padding.
:param texels_per_unit: See PackOptions.texels_per_unit.
:param resolution: See PackOptions.resolution.
:param bilinear: See PackOptions.bilinear.
:param block_align: See PackOptions.block_align.
:param brute_force: See PackOptions.brute_force.
:param rotate_charts_to_axis: See PackOptions.rotate_charts_to_axis.
:param rotate_charts: See PackOptions.rotate_charts.
:param input_uv_attribute_name: Input indexed UV attribute name.
:param input_chart_attribute_name: Optional per-facet chart id attribute. When set, each input UV island
    is pinned to its own chart (forwarded to xatlas as faceMaterialData). If empty (default),
    xatlas re-derives charts by flood-filling faces across shared UV edges, which can merge
    logically-distinct islands that touch/overlap in UV space and cause the packer to overlap or
    flip them. Set this to make repack preserve the existing island decomposition.
:param output_uv_attribute_name: Output indexed UV attribute name. If the attribute already exists,
    its value type is preserved; otherwise, a new attribute is created using the mesh Scalar type.
:param output_atlas_attribute_name: Corner-attribute name for the per-corner atlas tile id.
:param output_chart_attribute_name: Corner-attribute name for the per-corner chart id.
:param multi_atlas_policy: How to encode multi-atlas output.
:param enable_sharing_uvs_between_instances: Scene-only; ignored for single-mesh repack.

:returns: A new mesh with the repacked UV attribute.
)");

    m.def(
        "unwrap_scene",
        [](const scene::SimpleScene<Scalar, Index, 3>& scene,
           // chart
           float max_chart_area,
           float max_boundary_length,
           float normal_deviation_weight,
           float roundness_weight,
           float straightness_weight,
           float normal_seam_weight,
           float texture_seam_weight,
           float max_cost,
           uint32_t max_iterations,
           bool use_input_mesh_uvs,
           bool fix_winding,
           // pack
           uint32_t max_chart_size,
           uint32_t padding,
           float texels_per_unit,
           uint32_t resolution,
           bool bilinear,
           bool block_align,
           bool brute_force,
           bool rotate_charts_to_axis,
           bool rotate_charts,
           // unwrap
           std::string_view input_normal_attribute_name,
           std::string_view input_uv_attribute_name,
           std::string_view output_uv_attribute_name,
           std::string_view output_atlas_attribute_name,
           std::string_view output_chart_attribute_name,
           MultiAtlasPolicy multi_atlas_policy,
           bool enable_sharing_uvs_between_instances,
           // scene
           std::vector<float> per_instance_importance) {
            auto opts = make_unwrap_options(
                max_chart_area,
                max_boundary_length,
                normal_deviation_weight,
                roundness_weight,
                straightness_weight,
                normal_seam_weight,
                texture_seam_weight,
                max_cost,
                max_iterations,
                use_input_mesh_uvs,
                fix_winding,
                max_chart_size,
                padding,
                texels_per_unit,
                resolution,
                bilinear,
                block_align,
                brute_force,
                rotate_charts_to_axis,
                rotate_charts,
                input_normal_attribute_name,
                input_uv_attribute_name,
                output_uv_attribute_name,
                output_atlas_attribute_name,
                output_chart_attribute_name,
                multi_atlas_policy,
                enable_sharing_uvs_between_instances);
            SceneOptions sopts;
            sopts.per_instance_importance = std::move(per_instance_importance);
            return unwrap_scene(scene, opts, sopts);
        },
        "scene"_a,
        nb::kw_only(),
        "max_chart_area"_a = chart_defaults.max_chart_area,
        "max_boundary_length"_a = chart_defaults.max_boundary_length,
        "normal_deviation_weight"_a = chart_defaults.normal_deviation_weight,
        "roundness_weight"_a = chart_defaults.roundness_weight,
        "straightness_weight"_a = chart_defaults.straightness_weight,
        "normal_seam_weight"_a = chart_defaults.normal_seam_weight,
        "texture_seam_weight"_a = chart_defaults.texture_seam_weight,
        "max_cost"_a = chart_defaults.max_cost,
        "max_iterations"_a = chart_defaults.max_iterations,
        "use_input_mesh_uvs"_a = chart_defaults.use_input_mesh_uvs,
        "fix_winding"_a = chart_defaults.fix_winding,
        "max_chart_size"_a = pack_defaults.max_chart_size,
        "padding"_a = pack_defaults.padding,
        "texels_per_unit"_a = pack_defaults.texels_per_unit,
        "resolution"_a = pack_defaults.resolution,
        "bilinear"_a = pack_defaults.bilinear,
        "block_align"_a = pack_defaults.block_align,
        "brute_force"_a = pack_defaults.brute_force,
        "rotate_charts_to_axis"_a = pack_defaults.rotate_charts_to_axis,
        "rotate_charts"_a = pack_defaults.rotate_charts,
        "input_normal_attribute_name"_a = "",
        "input_uv_attribute_name"_a = "",
        "output_uv_attribute_name"_a = unwrap_defaults.output_uv_attribute_name,
        "output_atlas_attribute_name"_a = unwrap_defaults.output_atlas_attribute_name,
        "output_chart_attribute_name"_a = unwrap_defaults.output_chart_attribute_name,
        "multi_atlas_policy"_a = unwrap_defaults.multi_atlas_policy,
        "enable_sharing_uvs_between_instances"_a =
            unwrap_defaults.enable_sharing_uvs_between_instances,
        "per_instance_importance"_a = std::vector<float>(),
        R"(Unwrap all meshes in a scene using xatlas. See unwrap_mesh() for parameter docs.

Scene-specific kwargs:

:param per_instance_importance: Optional list of per-instance importance weights (>0, finite).
)");

    m.def(
        "repack_scene",
        [](const scene::SimpleScene<Scalar, Index, 3>& scene,
           uint32_t max_chart_size,
           uint32_t padding,
           float texels_per_unit,
           uint32_t resolution,
           bool bilinear,
           bool block_align,
           bool brute_force,
           bool rotate_charts_to_axis,
           bool rotate_charts,
           std::string_view input_uv_attribute_name,
           std::string_view input_chart_attribute_name,
           std::string_view output_uv_attribute_name,
           std::string_view output_atlas_attribute_name,
           std::string_view output_chart_attribute_name,
           MultiAtlasPolicy multi_atlas_policy,
           bool enable_sharing_uvs_between_instances,
           std::vector<float> per_instance_importance) {
            auto opts = make_repack_options(
                max_chart_size,
                padding,
                texels_per_unit,
                resolution,
                bilinear,
                block_align,
                brute_force,
                rotate_charts_to_axis,
                rotate_charts,
                input_uv_attribute_name,
                input_chart_attribute_name,
                output_uv_attribute_name,
                output_atlas_attribute_name,
                output_chart_attribute_name,
                multi_atlas_policy,
                enable_sharing_uvs_between_instances);
            SceneOptions sopts;
            sopts.per_instance_importance = std::move(per_instance_importance);
            return repack_scene(scene, opts, sopts);
        },
        "scene"_a,
        nb::kw_only(),
        "max_chart_size"_a = pack_defaults.max_chart_size,
        "padding"_a = pack_defaults.padding,
        "texels_per_unit"_a = pack_defaults.texels_per_unit,
        "resolution"_a = pack_defaults.resolution,
        "bilinear"_a = pack_defaults.bilinear,
        "block_align"_a = pack_defaults.block_align,
        "brute_force"_a = pack_defaults.brute_force,
        "rotate_charts_to_axis"_a = pack_defaults.rotate_charts_to_axis,
        "rotate_charts"_a = pack_defaults.rotate_charts,
        "input_uv_attribute_name"_a = repack_defaults.input_uv_attribute_name,
        "input_chart_attribute_name"_a = repack_defaults.input_chart_attribute_name,
        "output_uv_attribute_name"_a = repack_defaults.output_uv_attribute_name,
        "output_atlas_attribute_name"_a = repack_defaults.output_atlas_attribute_name,
        "output_chart_attribute_name"_a = repack_defaults.output_chart_attribute_name,
        "multi_atlas_policy"_a = repack_defaults.multi_atlas_policy,
        "enable_sharing_uvs_between_instances"_a =
            repack_defaults.enable_sharing_uvs_between_instances,
        "per_instance_importance"_a = std::vector<float>(),
        R"(Repack all UV islands in a scene using xatlas. See repack_mesh() for parameter docs.

Scene-specific kwargs:

:param per_instance_importance: Optional list of per-instance importance weights (>0, finite).
)");
}

} // namespace lagrange::python
