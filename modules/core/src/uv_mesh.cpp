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
#include <lagrange/Attribute.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/uv_mesh.h>
#include <lagrange/internal/get_uv_attribute.h>
#include <lagrange/utils/assert.h>

namespace lagrange {

template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> uv_mesh_ref(
    SurfaceMesh<Scalar, Index>& mesh,
    const UVMeshOptions& options)
{
    auto [uv_values, uv_indices] = internal::ref_uv_attribute(mesh, options.uv_attribute_name);

    SurfaceMesh<Scalar, Index> uv_mesh(2);
    uv_mesh.wrap_as_vertices(
        {uv_values.data(), static_cast<size_t>(uv_values.size())},
        static_cast<Index>(uv_values.rows()));

    if (mesh.is_regular()) {
        uv_mesh.wrap_as_facets(
            {uv_indices.data(), static_cast<size_t>(uv_indices.size())},
            mesh.get_num_facets(),
            mesh.get_vertex_per_facet());
    } else {
        AttributeId facet_offset_id = mesh.attr_id_facet_to_first_corner();
        auto& facet_offset = mesh.template ref_attribute<Index>(facet_offset_id);
        uv_mesh.wrap_as_facets(
            facet_offset.ref_all(),
            mesh.get_num_facets(),
            {uv_indices.data(), static_cast<size_t>(uv_indices.size())},
            mesh.get_num_corners());
    }

    return uv_mesh;
}

template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> uv_mesh_view(
    const SurfaceMesh<Scalar, Index>& mesh,
    const UVMeshOptions& options)
{
    auto [uv_values, uv_indices] = internal::get_uv_attribute(mesh, options.uv_attribute_name);

    SurfaceMesh<Scalar, Index> uv_mesh(2);
    uv_mesh.wrap_as_const_vertices(
        {uv_values.data(), static_cast<size_t>(uv_values.size())},
        static_cast<Index>(uv_values.rows()));

    if (mesh.is_regular()) {
        uv_mesh.wrap_as_const_facets(
            {uv_indices.data(), static_cast<size_t>(uv_indices.size())},
            mesh.get_num_facets(),
            mesh.get_vertex_per_facet());
    } else {
        AttributeId facet_offset_id = mesh.attr_id_facet_to_first_corner();
        const auto& facet_offset = mesh.template get_attribute<Index>(facet_offset_id);
        uv_mesh.wrap_as_const_facets(
            facet_offset.get_all(),
            mesh.get_num_facets(),
            {uv_indices.data(), static_cast<size_t>(uv_indices.size())},
            mesh.get_num_corners());
    }

    return uv_mesh;
}

#define LA_X_uv_mesh_view(_, Scalar, Index)                                      \
    template LA_CORE_API SurfaceMesh<Scalar, Index> uv_mesh_ref<Scalar, Index>(  \
        SurfaceMesh<Scalar, Index>&,                                             \
        const UVMeshOptions&);                                                   \
    template LA_CORE_API SurfaceMesh<Scalar, Index> uv_mesh_view<Scalar, Index>( \
        const SurfaceMesh<Scalar, Index>&,                                       \
        const UVMeshOptions&);
LA_SURFACE_MESH_X(uv_mesh_view, 0)
} // namespace lagrange
