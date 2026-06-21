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
#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace lagrange::xatlas {

///
/// Chart computation options. Maps 1-to-1 onto `xatlas::ChartOptions`.
///
/// Charts are extracted by greedily growing seed faces while a cost function is below
/// `max_cost`. The cost is a weighted sum of normal deviation, roundness, straightness, and seam
/// terms; tuning the weights tunes the chart shapes.
///
struct ChartOptions
{
    /// Maximum chart area, in input units squared. Set to 0 (default) to disable the limit.
    float max_chart_area = 0.f;

    /// Maximum chart boundary length, in input units. Set to 0 (default) to disable the limit.
    float max_boundary_length = 0.f;

    /// Weight applied to normal deviation when growing charts. Higher values produce flatter
    /// charts.
    float normal_deviation_weight = 2.f;

    /// Weight applied to chart roundness. Higher values prefer compact (disk-like) charts.
    float roundness_weight = 0.01f;

    /// Weight applied to chart boundary straightness. Higher values prefer straight cuts.
    float straightness_weight = 6.f;

    /// Weight applied to crossing input normal seams. Values > 1000 effectively forbid crossing
    /// normal seams.
    float normal_seam_weight = 4.f;

    /// Weight applied to crossing input UV seams (when `use_input_mesh_uvs` is true).
    float texture_seam_weight = 0.5f;

    /// Maximum allowed chart cost. Faces whose addition would exceed this cost are not merged.
    float max_cost = 2.f;

    /// Maximum number of segmentation iterations. Higher values may produce better charts but cost
    /// more time.
    uint32_t max_iterations = 1;

    /// If true, treat the input UV attribute as the chart layout (faces with the same UV island
    /// belong to the same chart).
    bool use_input_mesh_uvs = false;

    /// If true, fix triangle winding so all charts have a consistent orientation.
    bool fix_winding = true;
};

///
/// Packing options. Maps 1-to-1 onto `xatlas::PackOptions`.
///
struct PackOptions
{
    /// Maximum chart size, in pixels. Charts larger than this are scaled down. 0 (default)
    /// means no limit.
    uint32_t max_chart_size = 0;

    /// Empty pixels of padding to leave around each chart. Useful for mip-map filtering.
    uint32_t padding = 0;

    /// Texel-to-world-unit ratio. If 0, xatlas estimates a value such that the entire atlas
    /// fits within `resolution` x `resolution` (or a single tile of any size if `resolution` is
    /// also 0).
    float texels_per_unit = 0.f;

    /// Atlas tile resolution, in pixels (square). 0 (default) means a single atlas sized by
    /// `texels_per_unit`. If both are non-zero, charts that don't fit produce additional tiles.
    uint32_t resolution = 0;

    /// If true, leave a 1-pixel safety margin so bilinear filtering doesn't sample neighbouring
    /// charts.
    bool bilinear = true;

    /// If true, align charts to 4-pixel blocks (useful for compressed texture formats like BCn).
    bool block_align = false;

    /// If true, use brute-force search to find optimal chart placement (significantly slower).
    bool brute_force = false;

    /// If true, rotate each chart so that its longest principal axis aligns with the U or V axis.
    bool rotate_charts_to_axis = true;

    /// If true, allow xatlas to rotate charts during packing (in addition to axis alignment).
    bool rotate_charts = true;
};

///
/// How to encode multi-atlas output in the returned UV attribute.
///
/// xatlas may produce more than one atlas tile when charts don't fit at the requested
/// `texels_per_unit`. This enum controls how the per-corner UVs encode the tile id.
///
enum class MultiAtlasPolicy {
    /// UVs are normalized inside each atlas tile (`[0, 1] x [0, 1]`). Tile id is stored
    /// separately as a corner attribute.
    NormalizePerTile,

    /// UVs include integer U offset = `atlasIndex`; V stays normalized in `[0, 1]`. Useful for
    /// downstream UDIM-style pipelines.
    Udim,

    /// Throw `lagrange::Error` if xatlas produces more than one atlas tile.
    ErrorIfMultiple,
};

///
/// Options controlling a full unwrap (segment + parameterize + pack).
///
struct UnwrapOptions
{
    /// Chart computation options.
    ChartOptions chart;

    /// Packing options.
    PackOptions packing;

    /// Optional input UV attribute name. When non-empty, the UV values are passed to xatlas as a
    /// per-vertex hint (regardless of `chart.use_input_mesh_uvs`). When
    /// `chart.use_input_mesh_uvs = true`, xatlas uses the UV islands directly as charts.
    /// May be a per-vertex or indexed UV attribute. Indexed UVs are handled internally via
    /// `lagrange::unify_index_buffer()` so seams are preserved.
    std::string_view input_uv_attribute_name;

    /// Optional input per-vertex normal attribute name. When set, xatlas uses these normals as a
    /// hint for chart segmentation. Must be a non-indexed vertex attribute (float or double).
    std::string_view input_normal_attribute_name;

    /// Multi-atlas encoding policy for the output UV attribute.
    MultiAtlasPolicy multi_atlas_policy = MultiAtlasPolicy::NormalizePerTile;

    /// Output indexed UV attribute name. If the attribute already exists and is `float` or
    /// `double`, its value type is preserved; otherwise it defaults to the mesh `Scalar` type.
    std::string_view output_uv_attribute_name = "@uv";

    /// Output corner attribute name for the per-corner xatlas atlas tile id (only written when attribute name is non-empty).
    std::string_view output_atlas_attribute_name = "";

    /// Output corner attribute name for the per-corner xatlas chart id (only written when attribute name is non-empty).
    std::string_view output_chart_attribute_name = "";

    /// Whether to share generated UVs between instances of the same scene mesh. When false, each
    /// scene instance is unwrapped independently with its instance transform baked in.
    bool enable_sharing_uvs_between_instances = false;
};

///
/// Options controlling a repack (existing UVs → new packing, no chart computation).
///
struct RepackOptions
{
    /// Packing options.
    PackOptions packing;

    /// Input indexed UV attribute name. Must be an indexed `float` or `double` UV attribute.
    std::string_view input_uv_attribute_name;

    /// Optional per-facet chart id attribute name. When set, each input UV island is pinned to its
    /// own chart; otherwise xatlas re-derives charts from shared UV edges.
    std::string_view input_chart_attribute_name;

    /// Multi-atlas encoding policy for the output UV attribute.
    MultiAtlasPolicy multi_atlas_policy = MultiAtlasPolicy::NormalizePerTile;

    /// Output indexed UV attribute name. If the attribute already exists, its value type is
    /// preserved; otherwise a new attribute is created using the mesh `Scalar` type.
    std::string_view output_uv_attribute_name = "@uv";

    /// Output corner attribute name for the per-corner xatlas atlas tile id (only written when attribute name is non-empty).
    std::string_view output_atlas_attribute_name = "";

    /// Output corner attribute name for the per-corner xatlas chart id (only written when attribute name is non-empty).
    std::string_view output_chart_attribute_name = "";

    /// Whether to share generated UVs between instances of the same scene mesh. When false, each
    /// scene instance is repacked independently.
    bool enable_sharing_uvs_between_instances = false;
};

///
/// Scene-specific options for `unwrap_scene` / `repack_scene`.
///
struct SceneOptions
{
    /// Optional per-instance importance weights. Size must be 0 or equal to the number of scene
    /// instances; values must be `> 0` and finite. Affects `unwrap_scene` only.
    std::vector<float> per_instance_importance;
};

} // namespace lagrange::xatlas
