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
#include <lagrange/utils/invalid.h>
#include <lagrange/utils/span.h>

#include <algorithm>
#include <numeric>
#include <vector>

namespace lagrange::internal {

///
/// Compute the target-to-source (i.e. backward) mapping from an input source-to-target (i.e.
/// forward) mapping.
///
/// @note The input source-to-target mapping may be a 1-to-many mapping, where multiple source
/// elements may be mapped to a single target element.
///
/// @tparam Index             The index type.
///
/// @param source2target      The source-to-target element mapping.
/// @param num_target_entries The total number of target elements.
///
/// @return The target-to-source mapping, which is a tuple consists of 2 index arrays, mapping_data
/// and mapping_offsets.
///
/// `mapping_data` is a flat array of indices of the source elements. `mapping_offsets` is an array
/// of `mapping_data` offset indices. It is of size `num_target_entires + 1`.  Target element `i` is
/// mapped to source elements with index in the range from `mapping_data[mapping_offsets[i]]` to
/// `mapping_data[mapping_offsets[i+1]]`.
///
template <typename Index>
auto invert_mapping(span<Index> old2new, Index num_target_entries)
    -> std::tuple<std::vector<Index>, std::vector<Index>>
{
    Index num_source_entries = static_cast<Index>(old2new.size());
    std::vector<Index> mapping_offsets(num_target_entries + 1, 0);
    std::vector<Index> mapping_data;

    for (Index i = 0; i < num_source_entries; ++i) {
        Index j = old2new[i];
        if (j == invalid<Index>()) continue;
        la_runtime_assert(
            j < num_target_entries,
            "Mapped element index cannot exceeds target number of elements!");
        ++mapping_offsets[j + 1];
    }

    std::partial_sum(mapping_offsets.begin(), mapping_offsets.end(), mapping_offsets.begin());
    mapping_data.resize(mapping_offsets.back());
    for (Index i = 0; i < num_source_entries; i++) {
        Index j = old2new[i];
        if (j == invalid<Index>()) continue;
        mapping_data[mapping_offsets[j]++] = i;
    }

    std::rotate(mapping_offsets.begin(), std::prev(mapping_offsets.end()), mapping_offsets.end());
    mapping_offsets[0] = 0;

    return {std::move(mapping_data), std::move(mapping_offsets)};
}

} // namespace lagrange::internal
