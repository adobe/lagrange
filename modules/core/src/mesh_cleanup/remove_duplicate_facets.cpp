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

#include <lagrange/mesh_cleanup/remove_duplicate_facets.h>

#include "../internal/bucket_sort.h"

#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/invalid.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <tbb/parallel_for.h>
#include <tbb/concurrent_vector.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <algorithm>
#include <iterator>
#include <numeric>
#include <unordered_set>
#include <vector>

namespace lagrange {

namespace {

// Finds the smallest cyclic shift of a string in linear time and constant memory.
//
// - Reference paper:
//   Duval, Jean Pierre. "Factorizing words over an ordered alphabet." Journal of Algorithms 4.4
//   (1983): 363-381.
//   https://doi.org/10.1016/0196-6774(83)90017-2
//
// - Explanatory article:
//   https://cp-algorithms.com/string/lyndon_factorization.html (CC BY-SA 4.0)
//   https://e-maxx.ru/algo/duval_algorithm (public domain)
//
template <typename Sequence>
size_t min_cyclic_shift(Sequence&& s, size_t n)
{
    size_t i = 0;
    size_t ans = 0;
    while (i < n) {
        ans = i;
        size_t j = i + 1;
        size_t k = i;
        while (j < 2 * n && s[k % n] <= s[j % n]) {
            if (s[k % n] < s[j % n])
                k = i;
            else
                k++;
            j++;
        }
        while (i <= k) {
            i += j - k;
        }
    }
    return ans;
}

///
/// Compares facets i and j lexicographically, starting from a given offset position in the given
/// direction.
///
/// @param[in]  facet_i   First facet indices.
/// @param[in]  facet_j   Second facet indices.
/// @param[in]  offset_i  First facet starting offset.
/// @param[in]  offset_j  Second facet starting offset.
/// @param[in]  fwdi      First facet forward direction.
/// @param[in]  fwdj      Second facet forward direction.
///
/// @tparam     Index     Index type.
///
/// @return     * -1 if facet_i < facet_j
///             * 0 if facet_i == facet_j
///             * 1 if facet_i > facet_j
///
template <typename Index>
int compare_facets(
    span<const Index> facet_i,
    span<const Index> facet_j,
    size_t offset_i,
    size_t offset_j,
    bool fwdi,
    bool fwdj)
{
    const size_t ni = facet_i.size();
    const size_t nj = facet_j.size();
    if (ni != nj) {
        return ni < nj ? -1 : 1;
    }
    for (size_t k = 0; k < ni; ++k) {
        Index vi = facet_i[(offset_i + (fwdi ? k : ni - k)) % ni];
        Index vj = facet_j[(offset_j + (fwdj ? k : nj - k)) % nj];
        if (vi < vj) {
            return -1;
        } else if (vi > vj) {
            return 1;
        }
    }
    return 0;
}

template <bool ConsiderOrientation, typename Scalar, typename Index>
void remove_duplicate_facets_internal(SurfaceMesh<Scalar, Index>& mesh)
{
    auto num_facets = mesh.get_num_facets();

    // Per-facet index i such that (f[i], f[i+1], ..., f[i+n-1]) is the lexicographically smallest
    // facet among all rotations of the facet indices.
    std::vector<size_t> min_offset(num_facets, 0);

    // Encodes the direction of the facet with the lexicographically smallest facet indices. I.e.,
    // when considering facets with opposite orientations as potential duplicates, we also account
    // for (f[i], f[i-1], ..., f[i-n+1]) among all rotations of the facet indices.
    std::vector<uint8_t> facet_direction;
    if (!ConsiderOrientation) {
        facet_direction.resize(num_facets, true);
    }

    // Sorted facet indices according to our duplicate-aware comparison function.
    std::vector<Index> facets(num_facets);
    std::iota(facets.begin(), facets.end(), 0);

    tbb::parallel_for(Index(0), num_facets, [&](Index f) {
        auto facet = mesh.get_facet_vertices(f);
        const size_t n = facet.size();
        min_offset[f] = min_cyclic_shift(facet, n);
        if constexpr (!ConsiderOrientation) {
            const size_t reverse_offset = n - 1 - min_cyclic_shift(facet.rbegin(), n);
            if (compare_facets<Index>(facet, facet, min_offset[f], reverse_offset, true, false) >
                0) {
                min_offset[f] = reverse_offset;
                facet_direction[f] = false;
            }
        }
    });

    auto facet_dir = [&]([[maybe_unused]] Index i) -> bool {
        if constexpr (!ConsiderOrientation) {
            return facet_direction[i];
        } else {
            return true;
        }
    };

    // Three-way comparison operator between facet i and facet j.
    auto compare = [&](Index i, Index j) {
        auto fi = mesh.get_facet_vertices(i);
        auto fj = mesh.get_facet_vertices(j);
        return compare_facets<Index>(
            fi,
            fj,
            min_offset[i],
            min_offset[j],
            facet_dir(i),
            facet_dir(j));
    };

    auto is_less_than = [&](Index i, Index j) -> bool { return compare(i, j) < 0; };
    auto not_equal_to = [&](Index i, Index j) -> bool { return compare(i, j) != 0; };

    // First pass: bucket sort according to first vertex in the lexicographically smallest facet.
    auto result = internal::bucket_sort(facets, mesh.get_num_vertices(), [&](Index f) {
        return mesh.get_facet_vertex(f, static_cast<Index>(min_offset[f]));
    });

    // Process range of equals facets and vote on facet orientation
    std::vector<uint8_t> keep(num_facets, true);
    auto process_range = [&](auto&& range_begin, auto&& range_end) {
        auto count = std::distance(range_begin, range_end);
        la_debug_assert(count != 0);
        if (count == 1) {
            // Facet is unique.
            return;
        }
        // Vote on facet orientation.
        int num_positive = 0;
        Index facet_to_keep = invalid<Index>();
        for (auto it = range_begin; it != range_end; ++it) {
            int old_value = num_positive;
            if (facet_dir(*it)) {
                ++num_positive;
            } else {
                --num_positive;
            }
            keep[*it] = false;
            if (old_value == 0 && num_positive != 0) {
                facet_to_keep = *it;
            }
        }
        // If we have a majority, keep one facet with the majority orientation.
        if (num_positive != 0) {
            keep[facet_to_keep] = true;
        }
    };

    // Second pass: parallel sort of each facet in each bucket + identify unique facets.
    tbb::parallel_for(Index(0), result.num_representatives, [&](Index r) {
        // Sort inner bucket
        const auto bucket_begin = facets.begin() + result.representative_offsets[r];
        const auto bucket_end = facets.begin() + result.representative_offsets[r + 1];
        tbb::parallel_sort(bucket_begin, bucket_end, is_less_than);

        // Process each equal range inside the bucket
        auto range_begin = bucket_begin;
        while (range_begin != bucket_end) {
            // Find next range of equal elements
            la_debug_assert(range_begin < bucket_end);
            auto range_end = std::adjacent_find(range_begin, bucket_end, not_equal_to);
            if (range_end != bucket_end) {
                ++range_end;
            }
            process_range(range_begin, range_end);
            range_begin = range_end;
        }
    });

    mesh.remove_facets([&](Index i) { return keep[i] == 0; });
}
} // namespace

template <typename Scalar, typename Index>
void remove_duplicate_facets(
    SurfaceMesh<Scalar, Index>& mesh,
    const RemoveDuplicateFacetOptions& opts)
{
    if (opts.consider_orientation) {
        remove_duplicate_facets_internal<true>(mesh);
    } else {
        remove_duplicate_facets_internal<false>(mesh);
    }
}

#define LA_X_remove_duplicate_facets(_, Scalar, Index)                \
    template LA_CORE_API void remove_duplicate_facets<Scalar, Index>( \
        SurfaceMesh<Scalar, Index>&,                                  \
        const RemoveDuplicateFacetOptions&);
LA_SURFACE_MESH_X(remove_duplicate_facets, 0)

} // namespace lagrange
