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
#include "mesh_to_xatlas.h"

#include <lagrange/Attribute.h>
#include <lagrange/IndexedAttribute.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/unify_index_buffer.h>
#include <lagrange/utils/Error.h>
#include <lagrange/utils/fmt/format.h>

#include <cstring>
#include <limits>
#include <type_traits>

namespace lagrange::xatlas::internal {

namespace {

template <typename Scalar, typename Index>
void copy_positions_as_float(const SurfaceMesh<Scalar, Index>& mesh, std::vector<float>& out)
{
    const Index n = mesh.get_num_vertices();
    out.resize(static_cast<size_t>(n) * 3);
    for (Index v = 0; v < n; ++v) {
        auto p = mesh.get_position(v);
        out[3 * v + 0] = static_cast<float>(p[0]);
        out[3 * v + 1] = static_cast<float>(p[1]);
        out[3 * v + 2] = static_cast<float>(p[2]);
    }
}

template <typename Scalar, typename Index>
void check_triangle_only(const SurfaceMesh<Scalar, Index>& mesh)
{
    if (!mesh.is_triangle_mesh()) {
        throw Error("lagrange::xatlas: input mesh must be triangle-only");
    }
}

template <typename Scalar, typename Index>
void check_dimension_3d(const SurfaceMesh<Scalar, Index>& mesh)
{
    if (mesh.get_dimension() != Index(3)) {
        throw Error(
            lagrange::format(
                "lagrange::xatlas: input mesh must have dimension 3 (got {})",
                static_cast<uint64_t>(mesh.get_dimension())));
    }
}

template <typename Scalar, typename Index>
void check_uint32_capacity(const SurfaceMesh<Scalar, Index>& mesh)
{
    constexpr uint64_t kMax = std::numeric_limits<uint32_t>::max();
    if (static_cast<uint64_t>(mesh.get_num_vertices()) > kMax) {
        throw Error(
            lagrange::format(
                "lagrange::xatlas: mesh has {} vertices, exceeds xatlas UInt32 limit",
                mesh.get_num_vertices()));
    }
    // For triangle-only meshes, num_corners = 3 * num_facets and is also passed to xatlas as
    // MeshDecl.indexCount, so check facet count against UINT32_MAX/3 to ensure 3*N still fits.
    if (static_cast<uint64_t>(mesh.get_num_facets()) > kMax / 3) {
        throw Error(
            lagrange::format(
                "lagrange::xatlas: mesh has {} facets; 3 * facets exceeds xatlas UInt32 index "
                "limit",
                mesh.get_num_facets()));
    }
    if (static_cast<uint64_t>(mesh.get_num_corners()) > kMax) {
        throw Error(
            lagrange::format(
                "lagrange::xatlas: mesh has {} corners, exceeds xatlas UInt32 index limit",
                mesh.get_num_corners()));
    }
}

template <typename Scalar, typename Index>
void copy_indices(const SurfaceMesh<Scalar, Index>& mesh, std::vector<uint32_t>& out)
{
    const Index num_corners = mesh.get_num_corners();
    out.resize(num_corners);
    const auto& facet_indices = mesh.get_corner_to_vertex();
    auto span = facet_indices.get_all();
    constexpr uint64_t kMax = std::numeric_limits<uint32_t>::max();
    for (Index c = 0; c < num_corners; ++c) {
        const auto v = span[c];
        if (static_cast<uint64_t>(v) > kMax) {
            throw Error(
                lagrange::format(
                    "lagrange::xatlas: vertex index {} exceeds xatlas UInt32 limit",
                    static_cast<uint64_t>(v)));
        }
        out[c] = static_cast<uint32_t>(v);
    }
}

template <typename Scalar, typename Index, typename ValueType>
bool try_copy_vertex_attribute_as_float(
    const SurfaceMesh<Scalar, Index>& mesh,
    std::string_view name,
    size_t channels,
    std::vector<float>& out)
{
    if (!mesh.template is_attribute_type<ValueType>(name)) return false;
    const auto& attr = mesh.template get_attribute<ValueType>(name);
    if (attr.get_element_type() != AttributeElement::Vertex) {
        throw Error(
            lagrange::format(
                "lagrange::xatlas: attribute '{}' must be a per-vertex attribute",
                name));
    }
    if (attr.get_num_channels() != channels) {
        throw Error(
            lagrange::format(
                "lagrange::xatlas: attribute '{}' must have {} channels (got {})",
                name,
                channels,
                attr.get_num_channels()));
    }
    if (attr.get_num_elements() != mesh.get_num_vertices()) {
        throw Error(
            lagrange::format(
                "lagrange::xatlas: attribute '{}' has {} elements, expected {} (one per vertex)",
                name,
                attr.get_num_elements(),
                mesh.get_num_vertices()));
    }
    auto values = attr.get_all();
    out.resize(values.size());
    for (size_t i = 0; i < values.size(); ++i) {
        out[i] = static_cast<float>(values[i]);
    }
    return true;
}

} // namespace

::xatlas::MeshDecl MeshAdapter::as_decl() const noexcept
{
    ::xatlas::MeshDecl decl{};
    decl.vertexCount = vertex_count;
    decl.vertexPositionData = positions.data();
    decl.vertexPositionStride = sizeof(float) * 3;
    if (!normals.empty()) {
        decl.vertexNormalData = normals.data();
        decl.vertexNormalStride = sizeof(float) * 3;
    }
    if (!uvs.empty()) {
        decl.vertexUvData = uvs.data();
        decl.vertexUvStride = sizeof(float) * 2;
    }
    decl.indexCount = static_cast<uint32_t>(indices.size());
    decl.indexData = indices.data();
    decl.indexFormat = ::xatlas::IndexFormat::UInt32;
    decl.faceCount = face_count;
    return decl;
}

template <typename Scalar, typename Index>
MeshAdapter build_mesh_adapter(const SurfaceMesh<Scalar, Index>& mesh, const UnwrapOptions& options)
{
    check_triangle_only(mesh);
    check_dimension_3d(mesh);
    check_uint32_capacity(mesh);

    const auto& uv_name = options.input_uv_attribute_name;
    const bool want_uv_hint = !uv_name.empty() || options.chart.use_input_mesh_uvs;
    if (options.chart.use_input_mesh_uvs && uv_name.empty()) {
        throw Error(
            "lagrange::xatlas: chart.use_input_mesh_uvs is true but input_uv_attribute_name is "
            "empty");
    }

    // If the input UV attribute is indexed, lower it to per-vertex UVs by unifying.
    // Extraction reads atlas output corners back onto the original mesh's corners, so we don't
    // need an expanded→original vertex mapping — the unified working_mesh is only used to
    // build per-vertex UV hints for xatlas.
    SurfaceMesh<Scalar, Index> working_mesh;
    const SurfaceMesh<Scalar, Index>* src = &mesh;

    if (want_uv_hint) {
        if (!mesh.has_attribute(uv_name)) {
            throw Error(
                lagrange::format("lagrange::xatlas: input UV attribute '{}' not found", uv_name));
        }
        if (mesh.is_attribute_indexed(uv_name)) {
            working_mesh = unify_named_index_buffer(
                mesh,
                std::vector<std::string_view>{std::string_view(uv_name)});
            src = &working_mesh;
        }
    }

    MeshAdapter adapter;
    adapter.vertex_count = static_cast<uint32_t>(src->get_num_vertices());
    adapter.face_count = static_cast<uint32_t>(src->get_num_facets());
    copy_positions_as_float(*src, adapter.positions);
    copy_indices(*src, adapter.indices);

    // Optional normal hint.
    if (!options.input_normal_attribute_name.empty()) {
        const auto& nname = options.input_normal_attribute_name;
        if (!src->has_attribute(nname)) {
            throw Error(
                lagrange::format("lagrange::xatlas: input normal attribute '{}' not found", nname));
        }
        if (src->is_attribute_indexed(nname)) {
            // Indexed normals not handled: skip with a clear error.
            throw Error(
                "lagrange::xatlas: indexed normal attribute hints are not supported; "
                "convert to a vertex attribute first");
        }
        bool ok = try_copy_vertex_attribute_as_float<Scalar, Index, float>(
                      *src,
                      nname,
                      3,
                      adapter.normals) ||
                  try_copy_vertex_attribute_as_float<Scalar, Index, double>(
                      *src,
                      nname,
                      3,
                      adapter.normals);
        if (!ok) {
            throw Error(
                lagrange::format(
                    "lagrange::xatlas: input normal attribute '{}' must be float or double",
                    nname));
        }
    }

    // Optional UV hint.
    if (want_uv_hint) {
        bool ok = try_copy_vertex_attribute_as_float<Scalar, Index, float>(
                      *src,
                      uv_name,
                      2,
                      adapter.uvs) ||
                  try_copy_vertex_attribute_as_float<Scalar, Index, double>(
                      *src,
                      uv_name,
                      2,
                      adapter.uvs);
        if (!ok) {
            throw Error(
                lagrange::format(
                    "lagrange::xatlas: input UV attribute '{}' must be float or double",
                    uv_name));
        }
    }

    return adapter;
}

::xatlas::UvMeshDecl UvMeshAdapter::as_decl() const noexcept
{
    ::xatlas::UvMeshDecl decl{};
    decl.vertexCount = vertex_count;
    decl.vertexUvData = uvs.data();
    decl.vertexStride = sizeof(float) * 2;
    decl.indexCount = static_cast<uint32_t>(indices.size());
    decl.indexData = indices.data();
    decl.indexFormat = ::xatlas::IndexFormat::UInt32;
    // Pin islands to charts when chart ids were provided (otherwise xatlas
    // re-derives charts by flood-filling over shared UV edges).
    if (!face_materials.empty()) {
        decl.faceMaterialData = face_materials.data();
    }
    return decl;
}

template <typename Scalar, typename Index>
UvMeshAdapter build_uv_mesh_adapter(
    const SurfaceMesh<Scalar, Index>& mesh,
    std::string_view uv_attribute_name,
    std::string_view chart_attribute_name)
{
    check_triangle_only(mesh);
    check_uint32_capacity(mesh);

    if (uv_attribute_name.empty()) {
        throw Error("lagrange::xatlas: repack input UV attribute name must not be empty");
    }
    if (!mesh.has_attribute(uv_attribute_name)) {
        throw Error(
            lagrange::format(
                "lagrange::xatlas: input UV attribute '{}' not found",
                uv_attribute_name));
    }
    if (!mesh.is_attribute_indexed(uv_attribute_name)) {
        throw Error(
            lagrange::format(
                "lagrange::xatlas: repack input UV attribute '{}' must be indexed",
                uv_attribute_name));
    }

    UvMeshAdapter adapter;
    adapter.face_count = static_cast<uint32_t>(mesh.get_num_facets());
    adapter.indices.resize(mesh.get_num_corners());

    auto fill_from = [&](auto* tag) {
        using ValueType = std::remove_pointer_t<decltype(tag)>;
        const auto& attr = mesh.template get_indexed_attribute<ValueType>(uv_attribute_name);
        if (attr.get_num_channels() != 2) {
            throw Error("lagrange::xatlas: input UV attribute must have 2 channels");
        }
        auto values = attr.values().get_all();
        constexpr uint64_t kMax = std::numeric_limits<uint32_t>::max();
        const size_t num_uv_values = values.size() / 2;
        if (static_cast<uint64_t>(num_uv_values) > kMax) {
            throw Error(
                lagrange::format(
                    "lagrange::xatlas: indexed UV attribute has {} values, exceeds xatlas UInt32 "
                    "limit",
                    num_uv_values));
        }
        adapter.vertex_count = static_cast<uint32_t>(num_uv_values);
        adapter.uvs.resize(values.size());
        for (size_t i = 0; i < values.size(); ++i) {
            adapter.uvs[i] = static_cast<float>(values[i]);
        }
        auto idx = attr.indices().get_all();
        for (size_t i = 0; i < idx.size(); ++i) {
            const auto v = idx[i];
            if (static_cast<uint64_t>(v) > kMax) {
                throw Error(
                    lagrange::format(
                        "lagrange::xatlas: UV index {} exceeds xatlas UInt32 limit",
                        static_cast<uint64_t>(v)));
            }
            adapter.indices[i] = static_cast<uint32_t>(v);
        }
    };

    if (mesh.template is_attribute_type<float>(uv_attribute_name)) {
        float* tag = nullptr;
        fill_from(tag);
    } else if (mesh.template is_attribute_type<double>(uv_attribute_name)) {
        double* tag = nullptr;
        fill_from(tag);
    } else {
        throw Error(
            lagrange::format(
                "lagrange::xatlas: input UV attribute '{}' must be float or double",
                uv_attribute_name));
    }

    // Optionally pin input islands to charts via xatlas faceMaterialData.
    if (!chart_attribute_name.empty()) {
        if (!mesh.has_attribute(chart_attribute_name)) {
            throw Error(
                lagrange::format(
                    "lagrange::xatlas: chart attribute '{}' not found",
                    chart_attribute_name));
        }
        if (mesh.is_attribute_indexed(chart_attribute_name)) {
            throw Error(
                lagrange::format(
                    "lagrange::xatlas: chart attribute '{}' must be a per-facet (non-indexed) "
                    "attribute",
                    chart_attribute_name));
        }
        adapter.face_materials.resize(adapter.face_count);
        auto fill_charts = [&](auto* tag) {
            using ChartType = std::remove_pointer_t<decltype(tag)>;
            const auto& chart_attr = mesh.template get_attribute<ChartType>(chart_attribute_name);
            if (chart_attr.get_element_type() != AttributeElement::Facet) {
                throw Error(
                    lagrange::format(
                        "lagrange::xatlas: chart attribute '{}' must be a Facet attribute",
                        chart_attribute_name));
            }
            auto data = chart_attr.get_all();
            for (uint32_t f = 0; f < adapter.face_count; ++f) {
                adapter.face_materials[f] = static_cast<uint32_t>(data[f]);
            }
        };
        if (mesh.template is_attribute_type<uint32_t>(chart_attribute_name)) {
            uint32_t* tag = nullptr;
            fill_charts(tag);
        } else if (mesh.template is_attribute_type<int32_t>(chart_attribute_name)) {
            int32_t* tag = nullptr;
            fill_charts(tag);
        } else if (mesh.template is_attribute_type<uint64_t>(chart_attribute_name)) {
            uint64_t* tag = nullptr;
            fill_charts(tag);
        } else if (mesh.template is_attribute_type<int64_t>(chart_attribute_name)) {
            int64_t* tag = nullptr;
            fill_charts(tag);
        } else {
            throw Error(
                lagrange::format(
                    "lagrange::xatlas: chart attribute '{}' must be an integer type",
                    chart_attribute_name));
        }
    }

    return adapter;
}

// Explicit instantiations.
#define LA_X_mesh_to_xatlas(_, Scalar, Index)                    \
    template MeshAdapter build_mesh_adapter<Scalar, Index>(      \
        const SurfaceMesh<Scalar, Index>&,                       \
        const UnwrapOptions&);                                   \
    template UvMeshAdapter build_uv_mesh_adapter<Scalar, Index>( \
        const SurfaceMesh<Scalar, Index>&,                       \
        std::string_view,                                        \
        std::string_view);
LA_SURFACE_MESH_X(mesh_to_xatlas, 0)
#undef LA_X_mesh_to_xatlas

} // namespace lagrange::xatlas::internal
