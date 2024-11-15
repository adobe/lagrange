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

#include <lagrange/subdivision/mesh_subdivision.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <nanobind/nanobind.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/vector.h>
#include <nanobind/stl/string_view.h>
#include <lagrange/utils/warnon.h>
// clang-format on

namespace lagrange::python {

namespace nb = nanobind;

void populate_subdivision_module(nb::module_& m)
{
    using namespace nb::literals;

    using Scalar = double;
    using Index = uint32_t;
    using MeshType = SurfaceMesh<Scalar, Index>;

    nb::enum_<lagrange::subdivision::SchemeType>(m, "SchemeType")
        .value("Bilinear", lagrange::subdivision::SchemeType::Bilinear)
        .value("CatmullClark", lagrange::subdivision::SchemeType::CatmullClark)
        .value("Loop", lagrange::subdivision::SchemeType::Loop);

    nb::enum_<lagrange::subdivision::VertexBoundaryInterpolation>(m, "VertexBoundaryInterpolation")
        .value("NoInterpolation", lagrange::subdivision::VertexBoundaryInterpolation::None)
        .value("EdgeOnly", lagrange::subdivision::VertexBoundaryInterpolation::EdgeOnly)
        .value("EdgeAndCorner", lagrange::subdivision::VertexBoundaryInterpolation::EdgeAndCorner);

    nb::enum_<lagrange::subdivision::FaceVaryingInterpolation>(m, "FaceVaryingInterpolation")
        .value("Smooth", lagrange::subdivision::FaceVaryingInterpolation::None)
        .value("CornersOnly", lagrange::subdivision::FaceVaryingInterpolation::CornersOnly)
        .value("CornersPlus1", lagrange::subdivision::FaceVaryingInterpolation::CornersPlus1)
        .value("CornersPlus2", lagrange::subdivision::FaceVaryingInterpolation::CornersPlus2)
        .value("Boundaries", lagrange::subdivision::FaceVaryingInterpolation::Boundaries)
        .value("All", lagrange::subdivision::FaceVaryingInterpolation::All);

    nb::enum_<lagrange::subdivision::InterpolatedAttributes::SelectionType>(
        m,
        "InterpolatedAttributesSelection")
        .value("All", lagrange::subdivision::InterpolatedAttributes::SelectionType::All)
        .value("Empty", lagrange::subdivision::InterpolatedAttributes::SelectionType::None)
        .value("Selected", lagrange::subdivision::InterpolatedAttributes::SelectionType::Selected);

    using Options = lagrange::subdivision::SubdivisionOptions;
    m.def(
        "subdivide_mesh",
        [](const MeshType& mesh,
           unsigned num_levels,
           std::optional<lagrange::subdivision::SchemeType> scheme,
           bool adaptive,
           std::optional<float> max_edge_length,
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
        "num_levels"_a,
        "scheme"_a = nb::none(),
        "adaptive"_a = false,
        "max_edge_length"_a = nb::none(),
        "vertex_boundary_interpolation"_a = Options{}.vertex_boundary_interpolation,
        "face_varying_interpolation"_a = Options{}.face_varying_interpolation,
        "use_limit_surface"_a = Options{}.use_limit_surface,
        "interpolated_attributes_selection"_a = Options{}.interpolated_attributes.selection_type,
        "interpolated_smooth_attributes"_a = nb::none(),
        "interpolated_linear_attributes"_a = nb::none(),
        "edge_sharpness_attr"_a = nb::none(),
        "vertex_sharpness_attr"_a = nb::none(),
        "face_hole_attr"_a = nb::none(),
        "output_limit_normals"_a = nb::none(),
        "output_limit_tangents"_a = nb::none(),
        "output_limit_bitangents"_a = nb::none(),
        R"(Evaluates the subdivision surface of a polygonal mesh.

:param mesh:                  The source mesh.
:param num_levels:            The number of levels of subdivision to apply.
:param scheme:                The subdivision scheme to use.
:param adaptive:              Whether to use adaptive subdivision.
:param max_edge_length:       The maximum edge length for adaptive subdivision.
:param vertex_boundary_interpolation:  Vertex boundary interpolation rule.
:param face_varying_interpolation:     Face-varying interpolation rule.
:param use_limit_surface:      Interpolate all data to the limit surface.
:param edge_sharpness_attr:    Per-edge scalar attribute denoting edge sharpness. Sharpness values must be in [0, 1] (0 means smooth, 1 means sharp).
:param vertex_sharpness_attr:  Per-vertex scalar attribute denoting vertex sharpness (e.g. for boundary corners). Sharpness values must be in [0, 1] (0 means smooth, 1 means sharp).
:param face_hole_attr:         Per-face integer attribute denoting face holes. A non-zero value means the facet is a hole. If a face is tagged as a hole, the limit surface will not be generated for that face.
:param output_limit_normals:   Output name for a newly computed per-vertex attribute containing the normals to the limit surface. Skipped if left empty.
:param output_limit_tangents:  Output name for a newly computed per-vertex attribute containing the tangents (first derivatives) to the limit surface. Skipped if left empty.
:param output_limit_bitangents: Output name for a newly computed per-vertex attribute containing the bitangents (second derivative) to the limit surface. Skipped if left empty.

:return: The subdivided mesh.)");
}

} // namespace lagrange::python
