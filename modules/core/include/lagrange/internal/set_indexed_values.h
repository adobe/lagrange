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

#include <lagrange/IndexedAttribute.h>
#include <lagrange/utils/invalid.h>
#include <lagrange/utils/scope_guard.h>

namespace lagrange::internal {

///
/// @addtogroup group-surfacemesh-attr-utils
/// @{
///

///
/// For each index matching the given predicate, appends a new value set to
/// `invalid<ValueType>()` and reassigns the index to point to it.
///
/// @param[in,out]  attr       The indexed attribute to fix up.
/// @param[in]      predicate  Returns true for each index that must be remapped.
///
/// @tparam         ValueType  Attribute value type.
/// @tparam         Index      Attribute index type.
/// @tparam         Predicate  Callable of signature `bool(Index)`.
///
template <typename ValueType, typename Index, typename Predicate>
void set_predicated_indexed_values(IndexedAttribute<ValueType, Index>& attr, Predicate&& predicate)
{
    auto indices = attr.indices().ref_all();
    const size_t num_indices = attr.indices().get_num_elements();

    // Count the number of indices to remap.
    size_t num_remap = 0;
    for (size_t i = 0; i < num_indices; ++i) {
        if (predicate(indices[i])) {
            ++num_remap;
        }
    }

    if (num_remap == 0) return;

    // Append an invalid<ValueType>() value for each index to remap.
    const size_t num_valid = attr.values().get_num_elements();

    const ValueType old_default_value = attr.values().get_default_value();
    auto scope = make_scope_guard([&] { attr.values().set_default_value(old_default_value); });
    attr.values().set_default_value(invalid<ValueType>());
    attr.values().insert_elements(num_remap);

    // Assign each remapped index to its own new value.
    Index next_value = static_cast<Index>(num_valid);
    for (size_t i = 0; i < num_indices; ++i) {
        if (predicate(indices[i])) {
            indices[i] = next_value++;
        }
    }
}

///
/// For each element in the index buffer set to `invalid<Index>()`, appends a new
/// value set to `invalid<ValueType>()` and updates the index to point to it.
///
/// @param[in,out]  attr       The indexed attribute to fix up.
///
/// @tparam         ValueType  Attribute value type.
/// @tparam         Index      Attribute index type.
///
template <typename ValueType, typename Index>
void set_invalid_indexed_values(IndexedAttribute<ValueType, Index>& attr)
{
    set_predicated_indexed_values(attr, [](Index index) { return index == invalid<Index>(); });
}

///
/// For each element in the index buffer pointing outside the value buffer, appends a new
/// value set to `invalid<ValueType>()` and updates the index to point to it.
///
/// @param[in,out]  attr       The indexed attribute to fix up.
///
/// @tparam         ValueType  Attribute value type.
/// @tparam         Index      Attribute index type.
///
template <typename ValueType, typename Index>
void set_out_of_range_indexed_values(IndexedAttribute<ValueType, Index>& attr)
{
    const size_t num_values = attr.values().get_num_elements();
    set_predicated_indexed_values(attr, [num_values](Index index) {
        return static_cast<size_t>(index) >= num_values;
    });
}

/// @}

} // namespace lagrange::internal
