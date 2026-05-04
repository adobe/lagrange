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
/// For each element in the index buffer set to `invalid<Index>()`, appends a new
/// value set to invalid<ValueType>() and updates the index to point to it.
///
/// @tparam     ValueType  Attribute value type.
/// @tparam     Index      Attribute index type.
///
/// @param[in,out]  attr   The indexed attribute to fix up.
///
template <typename ValueType, typename Index>
void set_invalid_indexed_values(IndexedAttribute<ValueType, Index>& attr)
{
    auto indices = attr.indices().ref_all();
    const size_t num_indices = attr.indices().get_num_elements();

    // Count the number of invalid indices.
    size_t num_invalid = 0;
    for (size_t i = 0; i < num_indices; ++i) {
        if (indices[i] == invalid<Index>()) {
            ++num_invalid;
        }
    }

    if (num_invalid == 0) return;

    // Append invalid<ValueType>() values for each invalid index.
    const size_t num_valid = attr.values().get_num_elements();

    const ValueType old_default_value = attr.values().get_default_value();
    auto scope = make_scope_guard([&] { attr.values().set_default_value(old_default_value); });
    attr.values().set_default_value(invalid<ValueType>());
    attr.values().insert_elements(num_invalid);

    // Assign each invalid index to its own new value.
    Index next_value = static_cast<Index>(num_valid);
    for (size_t i = 0; i < num_indices; ++i) {
        if (indices[i] == invalid<Index>()) {
            indices[i] = next_value++;
        }
    }
}

/// @}

} // namespace lagrange::internal
