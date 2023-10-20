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
#include <lagrange/mesh_cleanup/remove_duplicate_facets.h>
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
        size_t j = i + 1, k = i;
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

} // namespace

template <typename Scalar, typename Index>
void remove_duplicate_facets(
    SurfaceMesh<Scalar, Index>& mesh,
    const RemoveDuplicateFacetOptions& opts)
{
    auto num_facets = mesh.get_num_facets();
    std::vector<Index> order(num_facets);
    std::iota(order.begin(), order.end(), 0);

    // Per-facet index i such that (f[i], f[i+1], ..., f[i+n-1]) is the lexicographically smallest
    // facet among all rotations of the facet indices.
    std::vector<size_t> min_offset_forward(num_facets, 0);

    // Same as above, but for the reverse order. I.e. we find the index i such that (f[i], f[i-1],
    // ..., f[i-n+1]) is the lexicographically smallest facet among all rotations of the facet
    // indices.
    std::vector<size_t> min_offset_backward;
    if (!opts.consider_orientation) {
        min_offset_backward.resize(num_facets, 0);
    }

    tbb::parallel_for(Index(0), num_facets, [&](Index f) {
        auto facet = mesh.get_facet_vertices(f);
        size_t n = facet.size();
        min_offset_forward[f] = min_cyclic_shift(facet, n);
        if (!opts.consider_orientation) {
            min_offset_backward[f] = n - 1 - min_cyclic_shift(facet.rbegin(), n);
        }
    });

    // Returns 0 if facet i and j are not duplicates.
    // Returns 1 if facet i and j are duplicates with the same orientation.
    // Returns -1 if facet i and j are duplicates with the opposite orientation.
    auto comp = [&](Index i, Index j) -> int8_t {
        auto fi = mesh.get_facet_vertices(i);
        auto fj = mesh.get_facet_vertices(j);
        if (fi.size() != fj.size()) return 0;
        const size_t n = fi.size();

        int8_t result = 1;
        for (size_t k = 0; k < n; k++) {
            if (fi[(k + min_offset_forward[i]) % n] != fj[(k + min_offset_forward[j]) % n]) {
                result = 0;
                break;
            }
        }
        if (result == 0 && !opts.consider_orientation) {
            result = -1;
            for (size_t k = 0; k < n; k++) {
                if (fi[(k + min_offset_forward[i]) % n] !=
                    fj[(min_offset_backward[j] + n - k) % n]) {
                    result = 0;
                    break;
                }
            }
        }
        return result;
    };

    std::unordered_multiset facet_set(
        order.begin(),
        order.end(),
        mesh.get_num_facets(),
        [&](Index i) {
            auto fi = mesh.get_facet_vertices(i);
            size_t r = 0;
            std::hash<Index> hash_fn;
            std::for_each(fi.begin(), fi.end(), [&](Index v) { r += hash_fn(v); });
            return r;
        },
        [&](Index i, Index j) {
            if (opts.consider_orientation) {
                return comp(i, j) == 1;
            } else {
                return comp(i, j) != 0;
            }
        });

    std::vector<uint8_t> keep(num_facets, 0);
    auto check_facet = [&](Index i) {
        if (keep[i] != 0) return;
        auto [range_begin, range_end] = facet_set.equal_range(i);
        auto count = std::distance(range_begin, range_end);
        la_debug_assert(count != 0);
        if (count == 1) {
            // Facet is unique.
            keep[i] = 1;
        } else {
            // Vote on facet orientation.
            int8_t orientation = 0;
            Index positive_facet = i;
            Index negative_facet = invalid<Index>();
            for (auto itr2 = range_begin; itr2 != range_end; itr2++) {
                Index j = *itr2;
                auto orientation_j = comp(i, j);
                la_debug_assert(orientation_j != 0);
                orientation += orientation_j;
                if (orientation_j < 0 && j < negative_facet) {
                    negative_facet = j;
                } else if (orientation_j > 0 && j < positive_facet) {
                    positive_facet = j;
                }
            }
            if (orientation > 0) {
                la_debug_assert(positive_facet < num_facets);
                keep[positive_facet] = 1;
            } else if (orientation < 0) {
                la_debug_assert(negative_facet < num_facets);
                keep[negative_facet] = 1;
            }
        }
    };
    tbb::parallel_for(
        tbb::blocked_range<Index>(0, num_facets),
        [&](const tbb::blocked_range<Index>& range) {
            for (Index i = range.begin(); i != range.end(); ++i) {
                check_facet(i);
            }
        });

    mesh.remove_facets([&](Index i) { return keep[i] == 0; });
}

#define LA_X_remove_duplicate_facets(_, Scalar, Index)    \
    template void remove_duplicate_facets<Scalar, Index>( \
        SurfaceMesh<Scalar, Index>&,                      \
        const RemoveDuplicateFacetOptions&);
LA_SURFACE_MESH_X(remove_duplicate_facets, 0)

} // namespace lagrange
