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
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/extract_submesh.h>
#include <lagrange/separate_by_facet_groups.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <tbb/parallel_for.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <algorithm>
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
    std::vector<Index> facet_indices(num_facets);
    std::vector<Index> group_offsets(num_groups + 1, 0);
    for (auto i : facet_group_indices) {
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

    std::vector<SurfaceMesh<Scalar, Index>> results(num_groups);

    SubmeshOptions submesh_options(options);
    tbb::parallel_for((size_t)0, num_groups, [&](size_t i) {
        span<const Index> selected_facets(
            facet_indices.data() + group_offsets[i],
            static_cast<size_t>(group_offsets[i + 1] - group_offsets[i]));
        results[i] = extract_submesh(mesh, selected_facets, submesh_options);
    });
    return results;
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
    std::vector<Index> facet_group_indices(num_groups);
    for (size_t i = 0; i < num_groups; i++) {
        facet_group_indices[i] = get_facet_group(static_cast<Index>(i));
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
