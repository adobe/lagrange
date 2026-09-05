/*
 * Copyright 2023 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/utils/assert.h>
#include "internal/extract_submeshes_by_group.h"

#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/separate_by_facet_groups.h>

#include <algorithm>
#include <limits>
#include <numeric>
#include <vector>

namespace lagrange {

template <typename Scalar, typename Index>
std::vector<SurfaceMesh<Scalar, Index>> separate_by_facet_groups(
    const SurfaceMesh<Scalar, Index>& mesh,
    size_t num_groups,
    span<const Index> facet_group_indices,
    const SeparateByFacetGroupsOptions& options)
{
    const Index num_facets = mesh.get_num_facets();
    if (num_facets == 0) return {};
    la_runtime_assert(static_cast<Index>(facet_group_indices.size()) == num_facets);
    la_runtime_assert(num_groups < std::numeric_limits<size_t>::max(), "num_groups is too large");
    std::vector<Index> facet_indices(num_facets);
    std::vector<Index> group_offsets(num_groups + 1, 0);
    for (auto i : facet_group_indices) {
        la_runtime_assert(static_cast<size_t>(i) < num_groups, "facet group index is out of bound");
        group_offsets[i + 1]++;
    }
    std::partial_sum(group_offsets.begin(), group_offsets.end(), group_offsets.begin());
    la_debug_assert(group_offsets.back() == num_facets);
    for (Index i = 0; i < num_facets; i++) {
        facet_indices[group_offsets[facet_group_indices[i]]++] = i;
    }
    std::rotate(group_offsets.begin(), std::prev(group_offsets.end()), group_offsets.end());
    group_offsets[0] = 0;
    la_debug_assert(group_offsets.back() == num_facets);

    return internal::extract_submeshes_by_group(
        mesh,
        num_groups,
        {facet_indices.data(), facet_indices.size()},
        {group_offsets.data(), group_offsets.size()},
        SubmeshOptions(options));
}

template <typename Scalar, typename Index>
std::vector<SurfaceMesh<Scalar, Index>> separate_by_facet_groups(
    const SurfaceMesh<Scalar, Index>& mesh,
    span<const Index> facet_group_indices,
    const SeparateByFacetGroupsOptions& options)
{
    auto itr = std::max_element(facet_group_indices.begin(), facet_group_indices.end());
    if (itr == facet_group_indices.end()) return {};
    size_t num_groups = static_cast<size_t>(*itr + 1);
    return separate_by_facet_groups(mesh, num_groups, facet_group_indices, options);
}

template <typename Scalar, typename Index>
std::vector<SurfaceMesh<Scalar, Index>> separate_by_facet_groups(
    const SurfaceMesh<Scalar, Index>& mesh,
    size_t num_groups,
    function_ref<Index(Index)> get_facet_group,
    const SeparateByFacetGroupsOptions& options)
{
    const Index num_facets = mesh.get_num_facets();
    std::vector<Index> facet_group_indices(num_facets);
    for (Index i = 0; i < num_facets; i++) {
        facet_group_indices[i] = get_facet_group(i);
    }
    return separate_by_facet_groups(
        mesh,
        num_groups,
        {facet_group_indices.data(), facet_group_indices.size()},
        options);
}

#define LA_X_separate_by_facet_groups(_, Scalar, Index)                                    \
    template LA_CORE_API std::vector<SurfaceMesh<Scalar, Index>> separate_by_facet_groups( \
        const SurfaceMesh<Scalar, Index>&,                                                 \
        size_t,                                                                            \
        span<const Index>,                                                                 \
        const SeparateByFacetGroupsOptions&);                                              \
    template LA_CORE_API std::vector<SurfaceMesh<Scalar, Index>> separate_by_facet_groups( \
        const SurfaceMesh<Scalar, Index>&,                                                 \
        span<const Index>,                                                                 \
        const SeparateByFacetGroupsOptions&);                                              \
    template LA_CORE_API std::vector<SurfaceMesh<Scalar, Index>> separate_by_facet_groups( \
        const SurfaceMesh<Scalar, Index>&,                                                 \
        size_t,                                                                            \
        function_ref<Index(Index)>,                                                        \
        const SeparateByFacetGroupsOptions&);

LA_SURFACE_MESH_X(separate_by_facet_groups, 0)

} // namespace lagrange
