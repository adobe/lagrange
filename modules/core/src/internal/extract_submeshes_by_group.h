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
#include <lagrange/extract_submesh.h>
#include <lagrange/utils/span.h>

#include <vector>

namespace lagrange::internal {

///
/// Extract multiple submeshes defined by facet groups in a single linear pass.
///
/// This avoids the O(G*V) cost of calling extract_submesh once per group.
///
/// @note       Users should call separate_by_facet_groups instead of this function directly.
///
/// @param      mesh           Source mesh.
/// @param      num_groups     Number of groups.
/// @param      facet_indices  Source of all facet ids sorted by group.
/// @param      group_offsets  Array of size num_groups+1 such that
///                            facet_indices[group_offsets[g]:group_offsets[g+1]]
///                            is the range of facet indices for group g.
/// @param      options        Submesh extraction options.
///
/// @tparam     Scalar         Mesh scalar type.
/// @tparam     Index          Mesh index type.
///
/// @return     One SurfaceMesh per group.
///
template <typename Scalar, typename Index>
std::vector<SurfaceMesh<Scalar, Index>> extract_submeshes_by_group(
    const SurfaceMesh<Scalar, Index>& mesh,
    size_t num_groups,
    span<const Index> facet_indices,
    span<const Index> group_offsets,
    const SubmeshOptions& options);

} // namespace lagrange::internal
