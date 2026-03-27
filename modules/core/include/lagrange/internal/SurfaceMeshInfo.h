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

#include <lagrange/AttributeFwd.h>
#include <lagrange/AttributeValueType.h>
#include <lagrange/utils/span.h>

#include <cstdint>
#include <string_view>
#include <vector>

namespace lagrange::internal {

///
/// Metadata and raw byte views for a single serialized attribute.
///
/// Byte views are non-owning — the underlying data must outlive this struct.
///
struct AttributeInfo
{
    std::string_view name;
    AttributeId attribute_id = 0;
    std::underlying_type_t<AttributeValueType> value_type = 0;
    std::underlying_type_t<AttributeElement> element_type = 0;
    std::underlying_type_t<AttributeUsage> usage = 0;
    size_t num_channels = 0;
    size_t num_elements = 0;
    bool is_indexed = false;

    /// Non-indexed attribute: raw data bytes (num_elements * num_channels * sizeof(ValueType)).
    span<const uint8_t> data_bytes;

    /// Indexed attribute: values and indices stored separately.
    span<const uint8_t> values_bytes;
    size_t values_num_elements = 0;
    size_t values_num_channels = 0;
    span<const uint8_t> indices_bytes;
    size_t indices_num_elements = 0;
    uint8_t index_type_size = 0;
};

///
/// Complete serialized mesh representation using standard types.
///
/// All byte views are non-owning — the source data must outlive this struct.
///
struct SurfaceMeshInfo
{
    uint8_t scalar_type_size = 0; ///< sizeof(Scalar): 4 for float, 8 for double.
    uint8_t index_type_size = 0; ///< sizeof(Index): 4 for uint32_t, 8 for uint64_t.

    size_t num_vertices = 0;
    size_t num_facets = 0;
    size_t num_corners = 0;
    size_t num_edges = 0;
    size_t dimension = 0;
    size_t vertex_per_facet = 0; ///< >0 for regular meshes, 0 for hybrid meshes.

    std::vector<AttributeInfo> attributes;
};

} // namespace lagrange::internal
