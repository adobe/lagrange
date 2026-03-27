/*
 * Copyright 2024 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */

#include <lagrange/subdivision/compute_sharpness.h>
#include <lagrange/subdivision/mesh_subdivision.h>

#include <lagrange/python/binding.h>

namespace lagrange::python {

namespace nb = nanobind;

void populate_subdivision_module(nb::module_& m)
{
    using namespace nb::literals;

    using Scalar = double;
    using Index = uint32_t;
    using MeshType = SurfaceMesh<Scalar, Index>;

    nb::enum_<lagrange::subdivision::SchemeType>(m, "SchemeType", "Subdivision scheme type")
        .value(
            "Bilinear",
            lagrange::subdivision::SchemeType::Bilinear,
            "Bilinear scheme, useful before applying displacement.")
        .value(
            "CatmullClark",
            lagrange::subdivision::SchemeType::CatmullClark,
            "Catmull-Clark scheme for quad-dominant meshes.")
        .value("Loop", lagrange::subdivision::SchemeType::Loop, "Loop scheme for triangle meshes.");

    nb::enum_<lagrange::subdivision::VertexBoundaryInterpolation>(
        m,
        "VertexBoundaryInterpolation",
        "Vertex boundary interpolation rule")
        .value(
            "NoInterpolation",
            lagrange::subdivision::VertexBoundaryInterpolation::None,
            "Do not interpolate boundary edges.")
        .value(
            "EdgeOnly",
            lagrange::subdivision::VertexBoundaryInterpolation::EdgeOnly,
            "Interpolate boundary edges only.")
        .value(
            "EdgeAndCorner",
            lagrange::subdivision::VertexBoundaryInterpolation::EdgeAndCorner,
            "Interpolate boundary edges and corners.");

    nb::enum_<lagrange::subdivision::FaceVaryingInterpolation>(
        m,
        "FaceVaryingInterpolation",
        "Face-varying interpolation rule")
        .value(
            "Smooth",
            lagrange::subdivision::FaceVaryingInterpolation::None,
            "Smooth interpolation everywhere the mesh is smooth.")
        .value(
            "CornersOnly",
            lagrange::subdivision::FaceVaryingInterpolation::CornersOnly,
            "Linear interpolation at corners only.")
        .value(
            "CornersPlus1",
            lagrange::subdivision::FaceVaryingInterpolation::CornersPlus1,
            "CornersOnly plus sharpening at junctions of 3+ regions.")
        .value(
            "CornersPlus2",
            lagrange::subdivision::FaceVaryingInterpolation::CornersPlus2,
            "CornersPlus1 plus sharpening of darts and concave corners.")
        .value(
            "Boundaries",
            lagrange::subdivision::FaceVaryingInterpolation::Boundaries,
            "Linear interpolation along all boundaries.")
        .value(
            "All",
            lagrange::subdivision::FaceVaryingInterpolation::All,
            "Linear interpolation everywhere.");

    nb::enum_<lagrange::subdivision::InterpolatedAttributes::SelectionType>(
        m,
        "InterpolatedAttributesSelection",
        "Selection tag for interpolated attributes")
        .value(
            "All",
            lagrange::subdivision::InterpolatedAttributes::SelectionType::All,
            "Interpolate all compatible attributes.")
        .value(
            "Empty",
            lagrange::subdivision::InterpolatedAttributes::SelectionType::None,
            "Do not interpolate any attributes.")
        .value(
            "Selected",
            lagrange::subdivision::InterpolatedAttributes::SelectionType::Selected,
            "Interpolate only selected attributes.");

    m.def(
        "compute_sharpness",
        [](MeshType& mesh,
           std::optional<std::string_view> normal_attribute_name,
           std::optional<double> feature_angle_threshold) {
            lagrange::subdivision::SharpnessOptions options;
            options.normal_attribute_name = normal_attribute_name.value_or("");
            options.feature_angle_threshold = feature_angle_threshold;
            auto res = lagrange::subdivision::compute_sharpness(mesh, options);
            return std::make_tuple(
                res.vertex_sharpness_attr,
                res.edge_sharpness_attr,
                res.normal_attr);
        },
        "mesh"_a,
        "normal_attribute_name"_a = nb::none(),
        "feature_angle_threshold"_a = nb::none(),
        R"(Computes sharpness attributes for subdivision based on existing mesh normals.

:param mesh: The input mesh. Modified in place to add the necessary attributes.
:param normal_attribute_name: If provided, name of the normal attribute to use as indexed normals to
    define sharp edges. If not provided, the function will attempt to find an existing indexed normal
    attribute. If no such attribute is found, autosmooth normals will be computed based on the
    `feature_angle_threshold` parameter.
:param feature_angle_threshold: Feature angle threshold (in radians) to detect sharp edges when
    computing autosmooth normals. By default, if no indexed normal attribute is found, no autosmooth
    normals will be computed.

:returns: A tuple containing:
    - vertex_sharpness_attr: Attribute id to use for vertex sharpness in the `subdivide_mesh` function.
    - edge_sharpness_attr: Attribute id to use for edge sharpness in the `subdivide_mesh` function.
    - normal_attr: Attribute id of the indexed normal attribute used to compute sharpness information.
)");

    using SubdivOptions = lagrange::subdivision::SubdivisionOptions;
    m.def(
        "subdivide_mesh",
        [](const MeshType& mesh,
           unsigned num_levels,
           std::optional<lagrange::subdivision::SchemeType> scheme,
           bool adaptive,
           std::optional<float> max_edge_length,
           std::optional<float> max_chordal_deviation,
           lagrange::subdivision::VertexBoundaryInterpolation vertex_boundary_interpolation,
           lagrange::subdivision::FaceVaryingInterpolation face_varying_interpolation,
           bool use_limit_surface,
           lagrange::subdivision::InterpolatedAttributes::SelectionType
               interpolated_attributes_selection,
           std::optional<std::vector<AttributeId>> interpolated_smooth_attributes,
           std::optional<std::vector<AttributeId>> interpolated_linear_attributes,
           std::optional<AttributeId> edge_sharpness_attr,
           std::optional<AttributeId> vertex_sharpness_attr,
           std::optional<AttributeId> face_hole_attr,
           std::optional<std::string_view> output_limit_normals,
           std::optional<std::string_view> output_limit_tangents,
           std::optional<std::string_view> output_limit_bitangents) {
            lagrange::subdivision::SubdivisionOptions options;
            options.scheme = scheme;
            if (adaptive) {
                options.refinement = lagrange::subdivision::RefinementType::EdgeAdaptive;
                if (max_edge_length.has_value()) {
                    options.max_edge_length = max_edge_length.value();
                }
                if (max_chordal_deviation.has_value()) {
                    options.max_chordal_deviation = max_chordal_deviation.value();
                }
            }
            options.num_levels = num_levels;
            options.vertex_boundary_interpolation = vertex_boundary_interpolation;
            options.face_varying_interpolation = face_varying_interpolation;
            options.use_limit_surface = use_limit_surface;
            options.interpolated_attributes.selection_type = interpolated_attributes_selection;
            if (interpolated_smooth_attributes) {
                options.interpolated_attributes.smooth_attributes =
                    std::move(interpolated_smooth_attributes.value());
            }
            if (interpolated_linear_attributes) {
                options.interpolated_attributes.linear_attributes =
                    std::move(interpolated_linear_attributes.value());
            }
            options.edge_sharpness_attr = edge_sharpness_attr;
            options.vertex_sharpness_attr = vertex_sharpness_attr;
            options.face_hole_attr = face_hole_attr;
            if (output_limit_normals) {
                options.output_limit_normals = output_limit_normals.value();
            }
            if (output_limit_tangents) {
                options.output_limit_tangents = output_limit_tangents.value();
            }
            if (output_limit_bitangents) {
                options.output_limit_bitangents = output_limit_bitangents.value();
            }
            return lagrange::subdivision::subdivide_mesh(mesh, options);
        },
        "mesh"_a,
        "num_levels"_a = 1,
        "scheme"_a = nb::none(),
        "adaptive"_a = false,
        "max_edge_length"_a = nb::none(),
        "max_chordal_deviation"_a = nb::none(),
        "vertex_boundary_interpolation"_a = SubdivOptions{}.vertex_boundary_interpolation,
        "face_varying_interpolation"_a = SubdivOptions{}.face_varying_interpolation,
        "use_limit_surface"_a = SubdivOptions{}.use_limit_surface,
        "interpolated_attributes_selection"_a =
            SubdivOptions{}.interpolated_attributes.selection_type,
        "interpolated_smooth_attributes"_a = nb::none(),
        "interpolated_linear_attributes"_a = nb::none(),
        "edge_sharpness_attr"_a = nb::none(),
        "vertex_sharpness_attr"_a = nb::none(),
        "face_hole_attr"_a = nb::none(),
        "output_limit_normals"_a = nb::none(),
        "output_limit_tangents"_a = nb::none(),
        "output_limit_bitangents"_a = nb::none(),
        R"(Evaluates the subdivision surface of a polygonal mesh.

:param mesh: The source mesh.
:param num_levels: The number of levels of subdivision to apply.
:param scheme: Subdivision scheme. If None, uses Loop for triangle meshes and CatmullClark for quad-dominant meshes.
:param adaptive: Whether to use edge-adaptive refinement.
:param max_edge_length: Maximum edge length for adaptive refinement. If None, uses longest edge / num_levels. Ignored when adaptive is False. Mutually exclusive with max_chordal_deviation.
:param max_chordal_deviation: Maximum chordal deviation for adaptive refinement. This controls the maximum distance between the limit surface and its piecewise-linear approximation. For each edge, the peak deviation is found using Newton's method on the squared-distance function, and the tessellation rate is chosen so that all sub-segment deviations stay below this threshold. Only supported for meshes with 3D vertex positions. The value must be strictly positive. Ignored when adaptive is False. Mutually exclusive with max_edge_length.
:param vertex_boundary_interpolation: Vertex boundary interpolation rule.
:param face_varying_interpolation: Face-varying interpolation rule.
:param use_limit_surface: Interpolate all data to the limit surface.
:param interpolated_attributes_selection: Whether to interpolate all, none, or selected attributes.
:param interpolated_smooth_attributes: Attribute ids to smoothly interpolate (per-vertex or indexed).
:param interpolated_linear_attributes: Attribute ids to linearly interpolate (per-vertex only).
:param edge_sharpness_attr: Per-edge scalar attribute denoting edge sharpness. Sharpness values must be in [0, 1] (0 means smooth, 1 means sharp).
:param vertex_sharpness_attr: Per-vertex scalar attribute denoting vertex sharpness (e.g. for boundary corners). Sharpness values must be in [0, 1] (0 means smooth, 1 means sharp).
:param face_hole_attr: Per-face integer attribute denoting face holes. A non-zero value means the facet is a hole. If a face is tagged as a hole, the limit surface will not be generated for that face.
:param output_limit_normals: Output name for a newly computed per-vertex attribute containing the normals to the limit surface. Skipped if left empty.
:param output_limit_tangents: Output name for a newly computed per-vertex attribute containing the tangents (first derivatives) to the limit surface. Skipped if left empty.
:param output_limit_bitangents: Output name for a newly computed per-vertex attribute containing the bitangents (second derivative) to the limit surface. Skipped if left empty.

:return: The subdivided mesh.)");
}

} // namespace lagrange::python
