/*
 * Copyright 2022 Adobe. All rights reserved.
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

#include <lagrange/internal/invert_mapping.h>
#include <lagrange/utils/DisjointSets.h>

#include <numeric>

namespace lagrange::internal {

///
/// Bucket sort result object.
///
/// @tparam     Index  Index type.
///
template <typename Index>
struct BucketSortResult
{
    /// Number of representatives in the input disjoint set.
    Index num_representatives;

    /// Sorted element in the input range.
    std::vector<Index> sorted_elements;

    /// Vector of size num_representatives + 1 storing the start/end of each disjoint set in the
    /// sorted element vector.
    std::vector<Index> representative_offsets;
};

///
/// Performs a bucket sort over a range of elements.
///
/// @param[in,out] unified_indices         Disjoint sets covering the range of elements to sort. Due
///                                        to path compression in the disjoint sets' find() method,
///                                        this argument is not const.
/// @param[out]    element_representative  Output buffer storing the representative index for each
///                                        element in the range. Typically this will be the index
///                                        buffer of a target indexed attribute.
///
/// @tparam        Index                   Index type to sort.
///
/// @return        Bucket sort result containing a list of sorted element indices and an offset for
///                each representative element.
///
template <typename Index>
BucketSortResult<Index> bucket_sort(
    DisjointSets<Index>& unified_indices,
    span<Index> element_representative)
{
    la_debug_assert(unified_indices.size() == element_representative.size());
    BucketSortResult<Index> result;

    auto& num_representatives = result.num_representatives;
    auto& sorted_elements = result.sorted_elements;
    auto& representative_offsets = result.representative_offsets;

    Index num_elements = static_cast<Index>(element_representative.size());

    // Calculate representative element for each bucket
    num_representatives = 0;
    std::fill(element_representative.begin(), element_representative.end(), invalid<Index>());
    for (Index e = 0; e < num_elements; ++e) {
        Index r = unified_indices.find(e);
        if (element_representative[r] == invalid<Index>()) {
            element_representative[r] = num_representatives++;
        }
        element_representative[e] = element_representative[r];
    }

    std::tie(sorted_elements, representative_offsets) = invert_mapping(
        {element_representative.data(), element_representative.size()},
        num_representatives);

    return result;
}

///
/// Bucket sort offset infos.
///
/// @tparam     Index  Index type.
///
template <typename Index>
struct BucketSortOffset
{
    /// Number of representatives in the input disjoint set.
    Index num_representatives;

    /// Vector of size num_representatives + 1 storing the start/end of each disjoint set in the
    /// sorted element vector.
    std::vector<Index> representative_offsets;
};

///
/// Perform a bucket sort over a range of elements in place.
///
/// @param[in,out] elements            Elements to sort.
/// @param[in]     num_buckets         Number of buckets (max element in the range + 1).
/// @param[in]     get_representative  Function to get the representative bucket for a given
///                                    element.
///
/// @tparam        Index               Index type.
/// @tparam        Function            Callback function type.
///
/// @return        Info about the sorted buckets.
///
template <typename Index, typename Function>
BucketSortOffset<Index>
bucket_sort(std::vector<Index>& elements, Index num_buckets, Function get_representative)
{
    BucketSortOffset<Index> result;

    auto& num_representatives = result.num_representatives;
    auto& representative_offsets = result.representative_offsets;
    num_representatives = num_buckets;
    std::tie(elements, representative_offsets) =
        invert_mapping<Index>(static_cast<Index>(elements.size()), get_representative, num_representatives);
    return result;
}

} // namespace lagrange::internal
