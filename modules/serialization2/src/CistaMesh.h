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

#include <cista/containers/string.h>
#include <cista/containers/vector.h>

#include <cstdint>
#include <type_traits>

namespace lagrange::serialization::internal {

namespace data = cista::offset;

/// Metadata and raw data for a single serialized attribute.
struct CistaAttributeInfo
{
    data::string name;
    AttributeId attribute_id = 0;
    std::underlying_type_t<AttributeValueType> value_type = 0;
    std::underlying_type_t<AttributeElement> element_type = 0;
    std::underlying_type_t<AttributeUsage> usage = 0;
    uint64_t num_channels = 0;
    uint64_t num_elements = 0;
    bool is_indexed = false;

    // Non-indexed attribute: raw data bytes (num_elements * num_channels * sizeof(ValueType))
    data::vector<uint8_t> data_bytes;

    // Indexed attribute: values and indices stored separately
    data::vector<uint8_t> values_bytes;
    uint64_t values_num_elements = 0;
    uint64_t values_num_channels = 0;
    data::vector<uint8_t> indices_bytes;
    uint64_t indices_num_elements = 0;
    uint8_t index_type_size = 0; // sizeof(Index) of the mesh (4 or 8)
};

/// Complete serialized mesh representation.
struct CistaMesh
{
    uint32_t version = 1; // Format version for forward/backward compatibility

    uint8_t scalar_type_size = 0; // sizeof(Scalar): 4 for float, 8 for double
    uint8_t index_type_size = 0; // sizeof(Index): 4 for uint32_t, 8 for uint64_t

    uint64_t num_vertices = 0;
    uint64_t num_facets = 0;
    uint64_t num_corners = 0;
    uint64_t num_edges = 0;
    uint64_t dimension = 0;
    uint64_t vertex_per_facet = 0;

    data::vector<CistaAttributeInfo> attributes;
};

} // namespace lagrange::serialization::internal
