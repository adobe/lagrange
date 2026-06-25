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
#include "atlas_to_mesh.h"

#include <lagrange/Attribute.h>
#include <lagrange/IndexedAttribute.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/internal/find_attribute_utils.h>
#include <lagrange/utils/Error.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/fmt/format.h>

namespace lagrange::xatlas::internal {

namespace {

template <typename UVScalar>
inline void normalize_uv(
    float u_pixel,
    float v_pixel,
    int32_t atlas_index,
    float atlas_w,
    float atlas_h,
    MultiAtlasPolicy policy,
    UVScalar& u_out,
    UVScalar& v_out)
{
    const float inv_w = atlas_w > 0.f ? 1.f / atlas_w : 0.f;
    const float inv_h = atlas_h > 0.f ? 1.f / atlas_h : 0.f;
    float u_norm = u_pixel * inv_w;
    float v_norm = v_pixel * inv_h;
    if (policy == MultiAtlasPolicy::Udim && atlas_index > 0) {
        u_norm += static_cast<float>(atlas_index);
    }
    u_out = static_cast<UVScalar>(u_norm);
    v_out = static_cast<UVScalar>(v_norm);
}

template <typename UVScalar, typename Scalar, typename Index>
void apply_atlas_uvs_to_mesh_typed(
    SurfaceMesh<Scalar, Index>& mesh,
    const ::xatlas::Mesh& atlas_mesh,
    const ::xatlas::Atlas& atlas,
    std::string_view output_uv_attribute_name,
    MultiAtlasPolicy policy)
{
    const Index num_corners = mesh.get_num_corners();
    if (atlas_mesh.indexCount != static_cast<uint32_t>(num_corners)) {
        throw Error(
            lagrange::format(
                "lagrange::xatlas: atlas index count ({}) does not match mesh corner count ({})",
                atlas_mesh.indexCount,
                static_cast<uint64_t>(num_corners)));
    }

    const auto uv_attr_id = lagrange::internal::find_or_create_attribute<UVScalar>(
        mesh,
        output_uv_attribute_name,
        AttributeElement::Indexed,
        AttributeUsage::UV,
        2,
        lagrange::internal::ResetToDefault::No);

    auto& uv_attr = mesh.template ref_indexed_attribute<UVScalar>(uv_attr_id);
    uv_attr.values().resize_elements(static_cast<size_t>(atlas_mesh.vertexCount));

    auto values = uv_attr.values().ref_all();
    const float atlas_w = static_cast<float>(atlas.width);
    const float atlas_h = static_cast<float>(atlas.height);
    for (uint32_t v = 0; v < atlas_mesh.vertexCount; ++v) {
        const auto& xv = atlas_mesh.vertexArray[v];
        UVScalar u, vv;
        normalize_uv<UVScalar>(xv.uv[0], xv.uv[1], xv.atlasIndex, atlas_w, atlas_h, policy, u, vv);
        values[2 * v + 0] = u;
        values[2 * v + 1] = vv;
    }

    auto indices = uv_attr.indices().ref_all();
    for (Index c = 0; c < num_corners; ++c) {
        indices[c] = static_cast<Index>(atlas_mesh.indexArray[c]);
    }
}

template <typename Scalar, typename Index>
void write_corner_int_attribute(
    SurfaceMesh<Scalar, Index>& mesh,
    const ::xatlas::Mesh& atlas_mesh,
    std::string_view name,
    bool use_chart_index)
{
    if (name.empty()) return;
    const Index num_corners = mesh.get_num_corners();

    const auto attr_id = lagrange::internal::find_or_create_attribute<int32_t>(
        mesh,
        name,
        AttributeElement::Corner,
        AttributeUsage::Scalar,
        1,
        lagrange::internal::ResetToDefault::Yes);

    auto& attr = mesh.template ref_attribute<int32_t>(attr_id);
    auto out = attr.ref_all();
    for (Index c = 0; c < num_corners; ++c) {
        const uint32_t outv = atlas_mesh.indexArray[c];
        const auto& xv = atlas_mesh.vertexArray[outv];
        out[c] = use_chart_index ? xv.chartIndex : xv.atlasIndex;
    }
}

} // namespace

template <typename Scalar, typename Index>
void apply_atlas_uvs_to_mesh(
    SurfaceMesh<Scalar, Index>& mesh,
    const ::xatlas::Atlas& atlas,
    uint32_t mesh_index,
    MultiAtlasPolicy policy,
    std::string_view output_uv_attribute_name,
    std::string_view output_atlas_attribute_name,
    std::string_view output_chart_attribute_name)
{
    if (mesh.get_num_facets() == 0) return;

    if (mesh_index >= atlas.meshCount) {
        throw Error(
            lagrange::format(
                "lagrange::xatlas: mesh_index {} out of range (atlas has {} meshes)",
                mesh_index,
                atlas.meshCount));
    }

    if (policy == MultiAtlasPolicy::ErrorIfMultiple && atlas.atlasCount > 1) {
        throw Error(
            lagrange::format(
                "lagrange::xatlas: atlas produced {} tiles but MultiAtlasPolicy::ErrorIfMultiple "
                "was requested",
                atlas.atlasCount));
    }

    const auto& atlas_mesh = atlas.meshes[mesh_index];

    // Determine output value type: if attribute exists and is float/double, preserve type;
    // else default to Scalar.
    if (mesh.has_attribute(output_uv_attribute_name)) {
        if (mesh.template is_attribute_type<float>(output_uv_attribute_name)) {
            apply_atlas_uvs_to_mesh_typed<float, Scalar, Index>(
                mesh,
                atlas_mesh,
                atlas,
                output_uv_attribute_name,
                policy);
        } else if (mesh.template is_attribute_type<double>(output_uv_attribute_name)) {
            apply_atlas_uvs_to_mesh_typed<double, Scalar, Index>(
                mesh,
                atlas_mesh,
                atlas,
                output_uv_attribute_name,
                policy);
        } else {
            throw Error(
                lagrange::format(
                    "lagrange::xatlas: existing output attribute '{}' must be float or double",
                    output_uv_attribute_name));
        }
    } else {
        apply_atlas_uvs_to_mesh_typed<Scalar, Scalar, Index>(
            mesh,
            atlas_mesh,
            atlas,
            output_uv_attribute_name,
            policy);
    }

    if (!output_atlas_attribute_name.empty()) {
        write_corner_int_attribute(
            mesh,
            atlas_mesh,
            output_atlas_attribute_name,
            /*use_chart_index=*/false);
    }

    if (!output_chart_attribute_name.empty()) {
        la_runtime_assert(!output_chart_attribute_name.empty());
        write_corner_int_attribute(
            mesh,
            atlas_mesh,
            output_chart_attribute_name,
            /*use_chart_index=*/true);
    }
}

#define LA_X_atlas_to_mesh(_, S, I)              \
    template void apply_atlas_uvs_to_mesh<S, I>( \
        SurfaceMesh<S, I>&,                      \
        const ::xatlas::Atlas&,                  \
        uint32_t,                                \
        MultiAtlasPolicy,                        \
        std::string_view,                        \
        std::string_view,                        \
        std::string_view);
LA_SURFACE_MESH_X(atlas_to_mesh, 0)
#undef LA_X_atlas_to_mesh

} // namespace lagrange::xatlas::internal
