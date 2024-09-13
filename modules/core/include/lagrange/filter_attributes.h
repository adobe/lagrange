/*
 * Copyright 2024 Adobe. All rights reserved.
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

#include <lagrange/SurfaceMesh.h>
#include <lagrange/utils/BitField.h>

#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace lagrange {

///
/// @defgroup   group-surfacemesh-utils Mesh utilities
/// @ingroup    group-surfacemesh
///
/// Various attribute processing utilities
///
/// @{
///

///
/// Helper object to filter attributes based on name, id, usage or element type.
///
struct AttributeFilter
{
    /// Variant identifying an attribute by its name or id.
    using AttributeNameOrId = std::variant<AttributeId, std::string>;

    /// Optional list of attributes to include. If not provided, all attributes are included.
    std::optional<std::vector<AttributeNameOrId>> included_attributes;

    /// Optional list of attributes to exclude. If not provided, no attribute is excluded.
    std::optional<std::vector<AttributeNameOrId>> excluded_attributes;

    /// List of attribute usages to include. By default, all usages are included.
    BitField<AttributeUsage> included_usages = BitField<AttributeUsage>::all();

    /// List of attribute element types to include. By default, all element types are included.
    BitField<AttributeElement> included_element_types = BitField<AttributeElement>::all();

    /// If set, match all attributes that satisfy any of the filters in this list.
    ///
    /// @note       A filter cannot contain both or_filters and and_filters.
    std::vector<AttributeFilter> or_filters;

    /// If set, match all attributes that satisfy all of the filters in this list.
    ///
    /// @note       A filter cannot contain both or_filters and and_filters.
    std::vector<AttributeFilter> and_filters;
};

///
/// Create a list of attribute ids corresponding to the given filter.
///
/// @param[in]  mesh     Mesh whose attributes are being filtered.
/// @param[in]  options  Filter options.
///
/// @tparam     Scalar   Mesh scalar type.
/// @tparam     Index    Mesh index type.
///
/// @return     A list of attribute ids matching the filter.
///
template <typename Scalar, typename Index>
std::vector<AttributeId> filtered_attribute_ids(
    const SurfaceMesh<Scalar, Index>& mesh,
    const AttributeFilter& options);

///
/// Filters the attributes of mesh according to user specifications.
///
/// @note       If the filter option does not contains AttributeElement::Edge as one of its element
///             type, mesh edge information will be removed in the output mesh.
///
/// @note       To convert a mesh and its attributes to different types, use the cast function.
///
/// @param[in]  source_mesh  Input mesh.
/// @param[in]  options      Filter options.
///
/// @tparam     Scalar       Mesh scalar type.
/// @tparam     Index        Mesh index type.
///
/// @return     Output mesh.
///
/// @see        cast
///
template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> filter_attributes(
    SurfaceMesh<Scalar, Index> source_mesh,
    const AttributeFilter& options = {});

///
/// @}
///

} // namespace lagrange
