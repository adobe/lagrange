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

namespace lagrange::subdivision {

namespace {

template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> extract_uniform_mesh_topology(
    const OpenSubdiv::Far::TopologyLevel& level,
    Index dimension)
{
    SurfaceMesh<Scalar, Index> mesh(dimension);
    mesh.add_vertices(level.GetNumVertices());
    mesh.add_hybrid(
        level.GetNumFaces(),
        [&](Index f) { return static_cast<Index>(level.GetFaceVertices(f).size()); },
        [&](Index f, lagrange::span<Index> t) {
            const auto& face = level.GetFaceVertices(f);
            std::transform(face.begin(), face.end(), t.begin(), [](auto&& x) {
                return static_cast<Index>(x);
            });
        });
    return mesh;
};

template <typename Index>
void set_indexed_attribute_indices(
    const OpenSubdiv::Far::TopologyLevel& level,
    Attribute<Index>& attr_indices,
    int fvar_index)
{
    auto target_indices = attr_indices.ref_all();
    size_t offset = 0;
    for (int face = 0; face < level.GetNumFaces(); ++face) {
        OpenSubdiv::Far::ConstIndexArray source = level.GetFaceFVarValues(face, fvar_index);
        auto target = target_indices.subspan(offset, source.size());
        std::transform(source.begin(), source.end(), target.begin(), [](auto&& x) {
            return static_cast<Index>(x);
        });
        offset += source.size();
    }
    la_debug_assert(offset == target_indices.size());
};

template <typename Scalar>
struct Vertex
{
    Vertex() {}

    void Clear(void* = 0) { std::fill(values.begin(), values.end(), Scalar(0)); }

    void AddWithWeight(const Vertex& src, Scalar weight)
    {
        for (size_t i = 0; i < values.size(); ++i) {
            values[i] += weight * src.values[i];
        }
    }

    // TODO: Specialize with compile-time size for N=1, 2, 3 and 4?
    lagrange::span<Scalar> values;
};

template <typename Scalar>
struct Vertex3
{
    Vertex3() { Clear(); }

    void Clear() { position[0] = position[1] = position[2] = Scalar(0); }

    void AddWithWeight(const Vertex<Scalar>& src, Scalar weight)
    {
        position[0] += weight * src.values[0];
        position[1] += weight * src.values[1];
        position[2] += weight * src.values[2];
    }

    void AddWithWeight(const Vertex3& src, Scalar weight)
    {
        position[0] += weight * src.position[0];
        position[1] += weight * src.position[1];
        position[2] += weight * src.position[2];
    }

    Eigen::Vector3<Scalar> GetPosition() const { return {position[0], position[1], position[2]}; }

    float position[3];
};

enum class InterpolationType {
    Smooth, ///< Smooth interpolation to the current subdivision level
    Limit, ///< Smooth interpolation to the limit surface
    Linear, ///< Linear interpolation
};

template <typename Scalar>
void interpolate_vertex_attribute(
    const OpenSubdiv::Far::TopologyRefiner& topology_refiner,
    const OpenSubdiv::Far::PrimvarRefinerReal<Scalar>& primvar_refiner,
    int num_refined_levels,
    const Attribute<Scalar>& source_attr,
    Attribute<Scalar>& target_attr,
    InterpolationType interpolation_type,
    Attribute<Scalar>* limit_normals = nullptr,
    Attribute<Scalar>* limit_tangents = nullptr,
    Attribute<Scalar>* limit_bitangents = nullptr)
{
    // Sanity check
    const bool need_limit_btn = (limit_normals || limit_tangents || limit_bitangents);
    if (need_limit_btn) {
        // If limit normals/tangents/bitangents are requested, we should be interpolating the vertex
        // positions, which can only be smoothly interpolated.
        la_debug_assert(interpolation_type != InterpolationType::Linear);
    }

    // Initialize intermediate data and buffers
    const bool need_limit = (interpolation_type == InterpolationType::Limit || need_limit_btn);
    const size_t num_channels = source_attr.get_num_channels();
    const size_t num_extra_vertices = (need_limit ? target_attr.get_num_elements() : size_t(0));
    const size_t num_intermediate_vertices = topology_refiner.GetNumVerticesTotal() +
                                             num_extra_vertices - target_attr.get_num_elements();
    // TODO: Cache those two std::vector in a thread-local tmp object? Maybe not worth it.
    std::vector<Scalar> all_values(num_intermediate_vertices * num_channels);
    std::vector<Vertex<Scalar>> verts(topology_refiner.GetNumVerticesTotal() + num_extra_vertices);
    for (size_t i = 0; i < verts.size(); ++i) {
        if (i < num_intermediate_vertices) {
            verts[i].values = {all_values.data() + i * num_channels, num_channels};
            if (i < source_attr.get_num_elements()) {
                const auto& src = source_attr.get_row(i);
                std::copy(src.begin(), src.end(), verts[i].values.begin());
            }
        } else {
            verts[i].values = target_attr.ref_row(i - num_intermediate_vertices);
        }
    }

    // Iterative interpolation
    Vertex<Scalar>* src = verts.data();
    for (int level = 1; level < num_refined_levels; ++level) {
        Vertex<Scalar>* dst = src + topology_refiner.GetLevel(level - 1).GetNumVertices();
        switch (interpolation_type) {
        case InterpolationType::Smooth: [[fallthrough]];
        case InterpolationType::Limit: primvar_refiner.Interpolate(level, src, dst); break;
        case InterpolationType::Linear: primvar_refiner.InterpolateVarying(level, src, dst); break;
        default: break;
        }
        src = dst;
    }
    if (need_limit_btn) {
        // Project the vertex positions to the limit surface and compute derivatives
        const auto& last_level = topology_refiner.GetLevel(num_refined_levels - 1);
        int num_vertices = last_level.GetNumVertices();

        Vertex<Scalar>* dst = src + num_vertices;
        std::vector<Vertex3<Scalar>> fine_tangent(num_vertices);
        std::vector<Vertex3<Scalar>> fine_bitangent(num_vertices);
        primvar_refiner.Limit(src, dst, fine_tangent, fine_bitangent);

        // Compute & copy limit normals
        for (int v = 0; limit_normals && v < num_vertices; ++v) {
            auto du = fine_tangent[v].GetPosition();
            auto dv = fine_bitangent[v].GetPosition();
            Eigen::Vector3<Scalar> normal = du.cross(dv).stableNormalized();
            std::copy_n(normal.data(), 3, limit_normals->ref_row(v).begin());
        }

        // Copy limit tangent & bitangent
        for (int v = 0; v < num_vertices; ++v) {
            if (limit_tangents) {
                std::copy_n(fine_tangent[v].position, 3, limit_tangents->ref_row(v).begin());
            }
            if (limit_bitangents) {
                std::copy_n(fine_bitangent[v].position, 3, limit_bitangents->ref_row(v).begin());
            }
        }

        // Overwrite limit positions with the last level interpolated data, but issue a warning
        // about using inconsistent positions/normals in the output mesh.
        if (interpolation_type == InterpolationType::Smooth) {
            logger().warn(
                "Limit normals/tangents/bitangents were requested, but refined vertex positions "
                "are not computed on the limit surface. Please set "
                "SubdivisionOptions::use_limit_surface=true to "
                "interpolate vertex positions to the limit surface and remove this warning.");
            for (int i = 0; i < num_vertices; ++i) {
                std::copy(src[i].values.begin(), src[i].values.end(), dst[i].values.begin());
            }
        }
    } else if (interpolation_type == InterpolationType::Limit) {
        // Project the last level interpolated data to the limit surface
        Vertex<Scalar>* dst =
            src + topology_refiner.GetLevel(num_refined_levels - 1).GetNumVertices();
        primvar_refiner.Limit(src, dst);
    }
}

template <typename Scalar>
void interpolate_indexed_attribute_values(
    const OpenSubdiv::Far::TopologyRefiner& topology_refiner,
    const OpenSubdiv::Far::PrimvarRefinerReal<Scalar>& primvar_refiner,
    int num_refined_levels,
    const Attribute<Scalar>& source_values,
    Attribute<Scalar>& target_values,
    int fvar_index,
    bool limit)
{
    // Allocate target attribute value buffer
    {
        const auto& last_level = topology_refiner.GetLevel(num_refined_levels - 1);
        target_values.resize_elements(last_level.GetNumFVarValues(fvar_index));
    }

    // Allocate and initialize the requested channel of 'face-varying' primvar data
    const size_t num_channels = source_values.get_num_channels();
    const size_t num_extra_values = (limit ? target_values.get_num_elements() : size_t(0));
    const size_t num_intermediate_values = topology_refiner.GetNumFVarValuesTotal(fvar_index) +
                                           num_extra_values - target_values.get_num_elements();
    std::vector<Scalar> all_values(num_intermediate_values * num_channels);
    std::vector<Vertex<Scalar>> values(
        topology_refiner.GetNumFVarValuesTotal(fvar_index) + num_extra_values);
    for (size_t i = 0; i < values.size(); ++i) {
        if (i < num_intermediate_values) {
            values[i].values = {all_values.data() + i * num_channels, num_channels};
            if (i < source_values.get_num_elements()) {
                const auto& src = source_values.get_row(i);
                std::copy(src.begin(), src.end(), values[i].values.begin());
            }
        } else {
            values[i].values = target_values.ref_row(i - num_intermediate_values);
        }
    }

    // Iterative interpolation
    Vertex<Scalar>* src = values.data();
    for (int level = 1; level < num_refined_levels; ++level) {
        Vertex<Scalar>* dst =
            src + topology_refiner.GetLevel(level - 1).GetNumFVarValues(fvar_index);
        primvar_refiner.InterpolateFaceVarying(level, src, dst, fvar_index);
        src = dst;
    }
    if (limit) {
        // Project the last level interpolated data to the limit surface
        Vertex<Scalar>* dst =
            src + topology_refiner.GetLevel(num_refined_levels - 1).GetNumFVarValues(fvar_index);
        primvar_refiner.LimitFaceVarying(src, dst, fvar_index);
    }
}

} // namespace

//------------------------------------------------------------------------------

template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> subdivide_uniform(
    const SurfaceMesh<Scalar, Index>& input_mesh,
    OpenSubdiv::Far::TopologyRefiner& topology_refiner,
    const InterpolatedAttributeIds& interpolated_attr,
    const SubdivisionOptions& options)
{
    if (options.preserve_shared_indices) {
        logger().warn(
            "Preserving shared indices is not supported with uniform subdivision. "
            "Ignoring the option. To silence this warning, set 'preserve_shared_indices' "
            "to false.");
    }

    // Uniformly refine the topology up to 'num_levels'
    {
        // note: fullTopologyInLastLevel must be true to work with face-varying data
        // TODO: Verify this is true
        OpenSubdiv::Far::TopologyRefiner::UniformOptions uniform_options(options.num_levels);
        uniform_options.fullTopologyInLastLevel = true;
        topology_refiner.RefineUniform(uniform_options);
    }

    // Adaptive refinement may result in fewer levels than the max specified.
    int num_refined_levels = topology_refiner.GetNumLevels();

    // Extract mesh facet topology
    SurfaceMesh<Scalar, Index> output_mesh = extract_uniform_mesh_topology<Scalar, Index>(
        topology_refiner.GetLevel(num_refined_levels - 1),
        input_mesh.get_dimension());

    // Prepare output BTN attributes
    Attribute<Scalar>* output_limit_normals = nullptr;
    Attribute<Scalar>* output_limit_tangents = nullptr;
    Attribute<Scalar>* output_limit_bitangents = nullptr;
    if (!options.output_limit_normals.empty()) {
        AttributeId out_id = lagrange::internal::find_or_create_attribute<Scalar>(
            output_mesh,
            options.output_limit_normals,
            AttributeElement::Vertex,
            AttributeUsage::Normal,
            3,
            lagrange::internal::ResetToDefault::No);
        output_limit_normals = &output_mesh.template ref_attribute<Scalar>(out_id);
    }
    if (!options.output_limit_tangents.empty()) {
        AttributeId out_id = lagrange::internal::find_or_create_attribute<Scalar>(
            output_mesh,
            options.output_limit_tangents,
            AttributeElement::Vertex,
            AttributeUsage::Tangent,
            3,
            lagrange::internal::ResetToDefault::No);
        output_limit_tangents = &output_mesh.template ref_attribute<Scalar>(out_id);
    }
    if (!options.output_limit_bitangents.empty()) {
        AttributeId out_id = lagrange::internal::find_or_create_attribute<Scalar>(
            output_mesh,
            options.output_limit_bitangents,
            AttributeElement::Vertex,
            AttributeUsage::Bitangent,
            3,
            lagrange::internal::ResetToDefault::No);
        output_limit_bitangents = &output_mesh.template ref_attribute<Scalar>(out_id);
    }

    // Interpolate per-vertex data (including vertex positions)
    auto interpolate_attribute = [&](AttributeId id, bool smooth) {
        lagrange::internal::visit_attribute_read(input_mesh, id, [&](auto&& attr) {
            using AttributeType = std::decay_t<decltype(attr)>;
            using ValueType = typename AttributeType::ValueType;
            if constexpr (!(std::is_same_v<ValueType, float> ||
                            std::is_same_v<ValueType, double>)) {
                la_debug_assert(false);
            } else {
                la_debug_assert(attr.get_element_type() == AttributeElement::Vertex);
                if constexpr (!AttributeType::IsIndexed) {
                    AttributeId out_id = lagrange::internal::find_or_create_attribute<ValueType>(
                        output_mesh,
                        input_mesh.get_attribute_name(id),
                        AttributeElement::Vertex,
                        attr.get_usage(),
                        attr.get_num_channels(),
                        lagrange::internal::ResetToDefault::No);
                    AttributeType& out_attr = output_mesh.template ref_attribute<ValueType>(out_id);

                    OpenSubdiv::Far::PrimvarRefinerReal<ValueType> primvar_refiner(
                        topology_refiner);
                    InterpolationType interpolation_type =
                        smooth ? InterpolationType::Smooth : InterpolationType::Linear;
                    if (smooth && options.use_limit_surface) {
                        interpolation_type = InterpolationType::Limit;
                    }
                    if (id == input_mesh.attr_id_vertex_to_position()) {
                        if constexpr (std::is_same_v<ValueType, Scalar>) {
                            interpolate_vertex_attribute(
                                topology_refiner,
                                primvar_refiner,
                                num_refined_levels,
                                attr,
                                out_attr,
                                interpolation_type,
                                output_limit_normals,
                                output_limit_tangents,
                                output_limit_bitangents);
                        } else {
                            // Technically impossible, but we need the if constexpr to prevent
                            // complaining about mismatched Attribute<> types.
                            la_debug_assert(false);
                        }
                    } else {
                        interpolate_vertex_attribute(
                            topology_refiner,
                            primvar_refiner,
                            num_refined_levels,
                            attr,
                            out_attr,
                            interpolation_type);
                    }
                }
            }
        });
    };
    for (auto id : interpolated_attr.smooth_vertex_attributes) {
        interpolate_attribute(id, true);
    }
    for (auto id : interpolated_attr.linear_vertex_attributes) {
        interpolate_attribute(id, false);
    }

    // Interpolate face-varying data (such as UVs)
    int fvar_index = 0;
    for (auto id : interpolated_attr.face_varying_attributes) {
        lagrange::internal::visit_attribute_read(input_mesh, id, [&](auto&& attr) {
            using AttributeType = std::decay_t<decltype(attr)>;
            using ValueType = typename AttributeType::ValueType;
            if constexpr (!(std::is_same_v<ValueType, float> ||
                            std::is_same_v<ValueType, double>)) {
                la_debug_assert(false);
            } else {
                if constexpr (!AttributeType::IsIndexed) {
                    // Technically impossible, we need the if constexpr because the lambda is
                    // instantiated for all attr types.
                    la_debug_assert(false);
                } else {
                    AttributeId out_id = lagrange::internal::find_or_create_attribute<ValueType>(
                        output_mesh,
                        input_mesh.get_attribute_name(id),
                        AttributeElement::Indexed,
                        attr.get_usage(),
                        attr.get_num_channels(),
                        lagrange::internal::ResetToDefault::No);
                    AttributeType& out_attr =
                        output_mesh.template ref_indexed_attribute<ValueType>(out_id);

                    // Set face-varying indices
                    set_indexed_attribute_indices(
                        topology_refiner.GetLevel(num_refined_levels - 1),
                        out_attr.indices(),
                        fvar_index);

                    // Interpolate face-varying values
                    OpenSubdiv::Far::PrimvarRefinerReal<ValueType> primvar_refiner(
                        topology_refiner);
                    interpolate_indexed_attribute_values(
                        topology_refiner,
                        primvar_refiner,
                        num_refined_levels,
                        attr.values(),
                        out_attr.values(),
                        fvar_index,
                        options.use_limit_surface);

                    fvar_index++;
                }
            }
        });
    }

    // If subdiv mesh has holes, we need to remove them from the output mesh
    if (topology_refiner.HasHoles()) {
        logger().debug("Removing facets tagged as holes");
        const auto& last_level = topology_refiner.GetLevel(num_refined_levels - 1);
        output_mesh.remove_facets([&](Index f) -> bool {
            return last_level.IsFaceHole(static_cast<OpenSubdiv::Far::Index>(f));
        });
    }

    return output_mesh;
}

#define LA_X_subdivide_uniform(_, Scalar, Index)                              \
    template LA_SUBDIVISION_API SurfaceMesh<Scalar, Index> subdivide_uniform( \
        const SurfaceMesh<Scalar, Index>& input_mesh,                         \
        OpenSubdiv::Far::TopologyRefiner& topology_refiner,                   \
        const InterpolatedAttributeIds& interpolated_attr,                    \
        const SubdivisionOptions& options);
LA_SURFACE_MESH_X(subdivide_uniform, 0)

} // namespace lagrange::subdivision
