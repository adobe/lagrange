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
#include <lagrange/subdivision/api.h>
#include <lagrange/subdivision/mesh_subdivision.h>

#include <lagrange/IndexedAttribute.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/foreach_attribute.h>
#include <lagrange/internal/attribute_string_utils.h>
#include <lagrange/internal/find_attribute_utils.h>
#include <lagrange/internal/visit_attribute.h>
#include <lagrange/utils/Error.h>
#include "MeshConverter.h"

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <opensubdiv/bfr/refinerSurfaceFactory.h>
#include <opensubdiv/bfr/surface.h>
#include <opensubdiv/bfr/tessellation.h>
#include <opensubdiv/far/topologyRefiner.h>
#include <opensubdiv/far/topologyRefinerFactory.h>
#include <lagrange/utils/warnon.h>
// clang-format on

//------------------------------------------------------------------------------

// This is ugly but what else can we do? Macros to the rescue!
// This needs to be in the same .cpp where these specialization are used, otherwise the compiler
// will pick the generic implementation!!!

#define ConverterType lagrange::subdivision::MeshConverter<lagrange::SurfaceMesh32f>
#include "TopologyRefinerFactory.h"
#undef ConverterType

#define ConverterType lagrange::subdivision::MeshConverter<lagrange::SurfaceMesh32d>
#include "TopologyRefinerFactory.h"
#undef ConverterType

#define ConverterType lagrange::subdivision::MeshConverter<lagrange::SurfaceMesh64f>
#include "TopologyRefinerFactory.h"
#undef ConverterType

#define ConverterType lagrange::subdivision::MeshConverter<lagrange::SurfaceMesh64d>
#include "TopologyRefinerFactory.h"
#undef ConverterType

//------------------------------------------------------------------------------

namespace lagrange::subdivision {

namespace {

// Convert from Lagrange's SchemeType to OpenSubdiv's SchemeType
template <typename Scalar, typename Index>
OpenSubdiv::Sdc::SchemeType get_subdivision_scheme(
    std::optional<SchemeType> input_scheme,
    const SurfaceMesh<Scalar, Index>& mesh)
{
    OpenSubdiv::Sdc::SchemeType output_scheme;
    if (input_scheme.has_value()) {
        switch (input_scheme.value()) {
        case SchemeType::CatmullClark: output_scheme = OpenSubdiv::Sdc::SCHEME_CATMARK; break;
        case SchemeType::Loop: output_scheme = OpenSubdiv::Sdc::SCHEME_LOOP; break;
        case SchemeType::Bilinear: output_scheme = OpenSubdiv::Sdc::SCHEME_BILINEAR; break;
        default: throw Error("Unknown subdivision scheme"); break;
        }
    } else {
        if (mesh.is_triangle_mesh()) {
            output_scheme = OpenSubdiv::Sdc::SCHEME_LOOP;
        } else {
            output_scheme = OpenSubdiv::Sdc::SCHEME_CATMARK;
        }
    }
    if (output_scheme == OpenSubdiv::Sdc::SCHEME_LOOP) {
        la_runtime_assert(
            mesh.is_triangle_mesh(),
            "Loop Subdivision only supports triangle meshes");
    }
    return output_scheme;
}

// Convert from Lagrange's SubdivisionOptions to OpenSubdiv's Sdc::Options
OpenSubdiv::Sdc::Options get_subdivision_options(const SubdivisionOptions& options)
{
    OpenSubdiv::Sdc::Options out;

    switch (options.vertex_boundary_interpolation) {
    case VertexBoundaryInterpolation::None:
        out.SetVtxBoundaryInterpolation(OpenSubdiv::Sdc::Options::VTX_BOUNDARY_NONE);
        break;
    case VertexBoundaryInterpolation::EdgeOnly:
        out.SetVtxBoundaryInterpolation(OpenSubdiv::Sdc::Options::VTX_BOUNDARY_EDGE_ONLY);
        break;
    case VertexBoundaryInterpolation::EdgeAndCorner:
        out.SetVtxBoundaryInterpolation(OpenSubdiv::Sdc::Options::VTX_BOUNDARY_EDGE_AND_CORNER);
        break;
    default: throw Error("Unknown vertex boundary interpolation"); break;
    }

    switch (options.face_varying_interpolation) {
    case FaceVaryingInterpolation::None:
        out.SetFVarLinearInterpolation(OpenSubdiv::Sdc::Options::FVAR_LINEAR_NONE);
        break;
    case FaceVaryingInterpolation::CornersOnly:
        out.SetFVarLinearInterpolation(OpenSubdiv::Sdc::Options::FVAR_LINEAR_CORNERS_ONLY);
        break;
    case FaceVaryingInterpolation::CornersPlus1:
        out.SetFVarLinearInterpolation(OpenSubdiv::Sdc::Options::FVAR_LINEAR_CORNERS_PLUS1);
        break;
    case FaceVaryingInterpolation::CornersPlus2:
        out.SetFVarLinearInterpolation(OpenSubdiv::Sdc::Options::FVAR_LINEAR_CORNERS_PLUS2);
        break;
    case FaceVaryingInterpolation::Boundaries:
        out.SetFVarLinearInterpolation(OpenSubdiv::Sdc::Options::FVAR_LINEAR_BOUNDARIES);
        break;
    case FaceVaryingInterpolation::All:
        out.SetFVarLinearInterpolation(OpenSubdiv::Sdc::Options::FVAR_LINEAR_ALL);
        break;
    default: throw Error("Unknown face varying interpolation"); break;
    }

    return out;
}

template <typename T>
bool contains(const std::vector<T>& v, const T& x)
{
    return std::find(v.begin(), v.end(), x) != v.end();
}

template <typename Scalar, typename Index>
InterpolatedAttributeIds prepare_interpolated_attribute_ids(
    const SurfaceMesh<Scalar, Index>& mesh,
    const InterpolatedAttributes& interpolation)
{
    InterpolatedAttributeIds result;

    result.smooth_vertex_attributes.push_back(mesh.attr_id_vertex_to_position());

    if (interpolation.selection_type != InterpolatedAttributes::SelectionType::Selected &&
        !interpolation.smooth_attributes.empty()) {
        logger().warn("Ignoring smooth_attributes list because selection_type is not 'Selected'.");
    }
    if (interpolation.selection_type == InterpolatedAttributes::SelectionType::None &&
        !interpolation.linear_attributes.empty()) {
        logger().warn("Ignoring linear_attributes list because selection_type is 'None'.");
    }

    bool all_smooth = (interpolation.selection_type == InterpolatedAttributes::SelectionType::All);
    switch (interpolation.selection_type) {
    case InterpolatedAttributes::SelectionType::None: break;
    case InterpolatedAttributes::SelectionType::Selected: [[fallthrough]];
    case InterpolatedAttributes::SelectionType::All: {
        seq_foreach_named_attribute_read(mesh, [&](std::string_view name, auto&& attr) {
            using AttributeType = std::decay_t<decltype(attr)>;
            using ValueType = typename AttributeType::ValueType;
            if (mesh.attr_name_is_reserved(name)) {
                return;
            }
            auto id = mesh.get_attribute_id(name);
            bool is_smooth = contains(interpolation.smooth_attributes, id);
            bool is_linear = contains(interpolation.linear_attributes, id);
            bool check_attribute = (is_smooth || is_linear);
            if (!is_smooth && !is_linear && all_smooth) {
                is_smooth = true;
            }
            if (!(is_smooth || is_linear)) {
                return;
            }
            if (is_smooth && is_linear) {
                logger().warn(
                    "Attribute '{}' is both smooth and linear. Defaulting to smooth.",
                    name);
                is_linear = false;
            }
            if constexpr (!(std::is_same_v<ValueType, float> ||
                            std::is_same_v<ValueType, double>)) {
                if (check_attribute) {
                    throw Error(
                        fmt::format(
                            "Interpolated attribute '{}' (id: {}) type must be float or double. "
                            "Received: {}",
                            name,
                            id,
                            lagrange::internal::value_type_name<ValueType>()));
                } else {
                    logger().debug(
                        "Skipping attribute '{}' (id: {}) with incompatible value type: {}",
                        name,
                        id,
                        lagrange::internal::value_type_name<ValueType>());
                    return;
                }
            }
            if (attr.get_element_type() == AttributeElement::Vertex) {
                if (is_smooth) {
                    logger().debug("Interpolating smooth vertex attribute '{}'.", name);
                    result.smooth_vertex_attributes.push_back(id);
                } else {
                    logger().debug("Interpolating linear vertex attribute '{}'", name);
                    result.linear_vertex_attributes.push_back(id);
                }
            } else if (attr.get_element_type() == AttributeElement::Indexed) {
                logger().debug("Interpolating indexed attribute '{}'.", name);
                result.face_varying_attributes.push_back(id);
            } else {
                if (check_attribute) {
                    throw Error(
                        fmt::format(
                            "Requested interpolation of a attribute '{}' (id: {}), which has "
                            "unsupported element type '{}'.",
                            name,
                            id,
                            lagrange::internal::to_string(attr.get_element_type())));
                } else {
                    logger().debug(
                        "Skipping attribute '{}' (id: {}) with unsupported element type: {}",
                        name,
                        id,
                        lagrange::internal::to_string(attr.get_element_type()));
                    return;
                }
            }
        });
        break;
    }
    default: la_debug_assert(false); break;
    }

    return result;
}

} // namespace

template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> subdivide_uniform(
    const SurfaceMesh<Scalar, Index>& input_mesh,
    OpenSubdiv::Far::TopologyRefiner& topology_refiner,
    const InterpolatedAttributeIds& interpolated_attr,
    const SubdivisionOptions& options);

template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> subdivide_edge_adaptive(
    const SurfaceMesh<Scalar, Index>& input_mesh,
    OpenSubdiv::Far::TopologyRefiner& topology_refiner,
    const InterpolatedAttributeIds& interpolated_attr,
    const SubdivisionOptions& options);

template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> subdivide_mesh(
    const SurfaceMesh<Scalar, Index>& input_mesh,
    const SubdivisionOptions& options)
{
    // Prepare list of attribute ids to interpolate
    auto interpolated_attr =
        prepare_interpolated_attribute_ids(input_mesh, options.interpolated_attributes);

    // Create a topology refiner from the input mesh
    std::unique_ptr<OpenSubdiv::Far::TopologyRefiner> topology_refiner([&] {
        using TopologyRefinerFactory =
            OpenSubdiv::Far::TopologyRefinerFactory<MeshConverter<SurfaceMesh<Scalar, Index>>>;

        MeshConverter<SurfaceMesh<Scalar, Index>> converter{
            input_mesh,
            options,
            interpolated_attr.face_varying_attributes};

        // Convert user options
        auto osd_scheme = get_subdivision_scheme(options.scheme, input_mesh);
        auto osd_options = get_subdivision_options(options);

        auto refiner_options = typename TopologyRefinerFactory::Options(osd_scheme, osd_options);
        refiner_options.validateFullTopology = true; // uncomment for debugging

        return TopologyRefinerFactory::Create(converter, refiner_options);
    }());

    if (options.validate_topology) {
        la_runtime_assert(topology_refiner->GetLevel(0).ValidateTopology());
    }

    if (options.refinement == RefinementType::Uniform) {
        return subdivide_uniform(input_mesh, *topology_refiner, interpolated_attr, options);
    } else {
        return subdivide_edge_adaptive(input_mesh, *topology_refiner, interpolated_attr, options);
    }
}

#define LA_X_subdivide_mesh(_, Scalar, Index)                              \
    template LA_SUBDIVISION_API SurfaceMesh<Scalar, Index> subdivide_mesh( \
        const SurfaceMesh<Scalar, Index>& mesh,                            \
        const SubdivisionOptions& options);
LA_SURFACE_MESH_X(subdivide_mesh, 0)

// TODOs for a second PR:
// - Nonmanifold inputs
// - Repeated evaluation using PatchTable? (both with iterative refinement and bfr limit surface)

} // namespace lagrange::subdivision
