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

#include <lagrange/SurfaceMesh.h>
#include <lagrange/xatlas/Options.h>

#include <cstdint>
#include <string_view>
#include <vector>

#include <xatlas/xatlas.h>

namespace lagrange::xatlas::internal {

///
/// Owning storage for an `xatlas::MeshDecl` derived from a Lagrange `SurfaceMesh`. Lifetime of all
/// referenced buffers must extend at least until `xatlas::AddMesh()` returns; the recommended
/// usage is to keep the `MeshAdapter` alive until `AddMeshJoin()` for safety.
///
struct MeshAdapter
{
    /// Float positions tightly packed (3 * vertex_count). Always copied/cast from input.
    std::vector<float> positions;

    /// Optional float normals tightly packed (3 * vertex_count). Empty if no normals supplied.
    std::vector<float> normals;

    /// Optional float UV hints tightly packed (2 * vertex_count). Empty if no hints supplied.
    std::vector<float> uvs;

    /// uint32 indices tightly packed (3 * facet_count for triangle-only meshes).
    std::vector<uint32_t> indices;

    /// Number of vertices (after optional unify).
    uint32_t vertex_count = 0;

    /// Number of facets.
    uint32_t face_count = 0;

    /// Build an `xatlas::MeshDecl` referencing this adapter's buffers.
    ::xatlas::MeshDecl as_decl() const noexcept;
};

///
/// Build an adapter from a triangle mesh. Throws `lagrange::Error` if the mesh is not triangular.
/// If `options.input_normal_attribute_name` is set, the normals are passed through as a hint.
/// If `options.input_uv_attribute_name` is set OR `options.chart.use_input_mesh_uvs` is true, the
/// UV attribute is passed through as a hint. Indexed UVs are first lowered to per-vertex UVs via
/// `unify_index_buffer()`. The original mesh topology is preserved on the way out via the xatlas
/// output index array (which has the same corner ordering as the original mesh).
///
template <typename Scalar, typename Index>
MeshAdapter build_mesh_adapter(
    const SurfaceMesh<Scalar, Index>& mesh,
    const UnwrapOptions& options);

///
/// Owning storage for an `xatlas::UvMeshDecl` derived from a Lagrange `SurfaceMesh` whose UV
/// attribute is to be repacked.
///
struct UvMeshAdapter
{
    /// Float UV values tightly packed (2 * vertex_count).
    std::vector<float> uvs;

    /// uint32 indices tightly packed (3 * facet_count).
    std::vector<uint32_t> indices;

    /// Optional per-face chart ids (size == face_count) forwarded to xatlas as
    /// `faceMaterialData` to pin input islands to charts. Empty if no chart attribute was given.
    std::vector<uint32_t> face_materials;

    /// Number of UV "vertices" (= number of unique indexed UV values).
    uint32_t vertex_count = 0;

    /// Number of triangles.
    uint32_t face_count = 0;

    /// Build an `xatlas::UvMeshDecl` referencing this adapter's buffers.
    ::xatlas::UvMeshDecl as_decl() const noexcept;
};

///
/// Build a UV adapter from a triangle mesh's existing indexed UV attribute. Throws
/// `lagrange::Error` if the mesh is not triangular or the named attribute is missing/invalid.
///
/// @param chart_attribute_name  Optional per-facet chart id attribute. When non-empty, its values
///                              are copied into `face_materials` so xatlas pins each island to its
///                              own chart instead of re-floodfilling charts from UV adjacency.
///
template <typename Scalar, typename Index>
UvMeshAdapter build_uv_mesh_adapter(
    const SurfaceMesh<Scalar, Index>& mesh,
    std::string_view uv_attribute_name,
    std::string_view chart_attribute_name);

} // namespace lagrange::xatlas::internal
