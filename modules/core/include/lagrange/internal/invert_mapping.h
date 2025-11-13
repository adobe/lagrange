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
#include <optional>
#include <vector>

namespace lagrange::internal {

///
/// A simple struct representing the inverse of a 1-to-many mapping. Target element `i` is mapped to
/// source elements with index in the range from `mapping.data[mapping.offsets[i]]` to
/// `mapping.data[mapping.offsets[i+1]]`.
///
/// @tparam     Index  Mapping index type.
///
/// @todo       Write iterator helpers to facilitate iterating over mapped data.
///
template <typename Index>
struct InverseMapping
{
    /// A flat array of indices of the source elements.
    std::vector<Index> data;

    /// An array of `data` offset indices. It is of size `num_target_elements + 1`.
    std::vector<Index> offsets;

    /// Iterate over all source elements mapped to target element `i`.
    template <typename Func>
    void foreach_mapped_to(Index i, Func&& func) const
    {
        la_debug_assert(i >= 0 && i < static_cast<Index>(offsets.size() - 1));
        for (Index j = offsets[i]; j < offsets[i + 1]; ++j) {
            func(data[j]);
        }
    }

    /// Iterate over all source elements mapped to target element `i`.
    template <typename Func>
    void foreach_mapped_to(Index i, Func&& func)
    {
        la_debug_assert(i >= 0 && i < static_cast<Index>(offsets.size() - 1));
        for (Index j = offsets[i]; j < offsets[i + 1]; ++j) {
            func(data[j]);
        }
    }
};

///
/// Compute the target-to-source (i.e. backward) mapping from an input source-to-target (i.e.
/// forward) mapping.
///
/// @note       The input source-to-target mapping may be a 1-to-many mapping, where multiple source
///             elements may be mapped to a single target element. If a target element is set to
///             `invalid<Index>()`, no backward mapping will be created for that target element.
///
/// @param[in]  num_source_elements  The number of source elements.
/// @param[in]  old_to_new           Source-to-target mapping function.
/// @param[in]  num_target_elements  The total number of target elements. If set to
///                                  `invalid<Index>()`, it is automatically calculated from the
///                                  forward mapping.
///
/// @tparam     Index                The index type.
/// @tparam     Function             Mapping function type.
///
/// @return     A struct representing the target-to-source mapping.
///
template <typename Index, typename Function>
InverseMapping<Index> invert_mapping(
    Index num_source_elements,
    Function old_to_new,
    Index num_target_elements = invalid<Index>())
{
    const bool has_target_count = num_target_elements != invalid<Index>();
    InverseMapping<Index> mapping;
    mapping.offsets.assign(has_target_count ? num_target_elements + 1 : num_source_elements + 1, 0);

    for (Index i = 0; i < num_source_elements; ++i) {
        Index j = old_to_new(i);
        if (j == invalid<Index>()) {
            continue;
        }
        la_runtime_assert(
            j < static_cast<Index>(mapping.offsets.size()),
            fmt::format(
                "Mapped element index cannot exceeds {} number of elements!",
                has_target_count ? "target" : "source"));
        ++mapping.offsets[j + 1];
    }

    if (!has_target_count) {
        // If the number of target elements is not provided, we need to resize our offset array now
        num_target_elements = num_source_elements;
        while (num_target_elements != 0 && mapping.offsets[num_target_elements] == 0) {
            --num_target_elements;
        }
        mapping.offsets.resize(num_target_elements + 1);
    }

    std::partial_sum(mapping.offsets.begin(), mapping.offsets.end(), mapping.offsets.begin());
    la_debug_assert(mapping.offsets.back() <= num_source_elements);
    mapping.data.resize(mapping.offsets.back());
    for (Index i = 0; i < num_source_elements; i++) {
        Index j = old_to_new(i);
        if (j == invalid<Index>()) {
            continue;
        }
        mapping.data[mapping.offsets[j]++] = i;
    }

    std::rotate(mapping.offsets.begin(), std::prev(mapping.offsets.end()), mapping.offsets.end());
    mapping.offsets[0] = 0;

    return mapping;
}

///
/// Compute the target-to-source (i.e. backward) mapping from an input source-to-target (i.e.
/// forward) mapping.
///
/// @note       The input source-to-target mapping may be a 1-to-many mapping, where multiple source
///             elements may be mapped to a single target element. If a target element is set to
///             `invalid<Index>()`, no backward mapping will be created for that target element.
///
/// @param[in]  old_to_new           Source-to-target mapping.
/// @param[in]  num_target_elements  The total number of target elements. If set to
///                                  `invalid<Index>()`, it is automatically calculated from the
///                                  forward mapping.
///
/// @tparam     Index                The index type.
///
/// @return     A struct representing the target-to-source mapping.
///
/// @overload
///
template <typename Index>
InverseMapping<Index> invert_mapping(
    span<const Index> old_to_new,
    Index num_target_elements = invalid<Index>())
{
    Index num_source_elements = static_cast<Index>(old_to_new.size());
    return invert_mapping(
        num_source_elements,
        [&](Index i) { return old_to_new[i]; },
        num_target_elements);
}

} // namespace lagrange::internal
