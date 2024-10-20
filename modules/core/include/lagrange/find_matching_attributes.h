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
#include <vector>

namespace lagrange {

///
/// @defgroup   group-surfacemesh-utils Mesh utilities
/// @ingroup    group-surfacemesh
///
/// Various mesh processing utilities.
///
/// @{
///

///
/// Helper object to match attributes based on usage, element type, and number of channels.
///
struct AttributeMatcher
{
    /// List of attribute usages to include. By default, all usages are included.
    BitField<AttributeUsage> usages = BitField<AttributeUsage>::all();

    /// List of attribute element types to include. By default, all element types are included.
    BitField<AttributeElement> element_types = BitField<AttributeElement>::all();

    /// Number of channels to match against. Default value is 0, which disables this test.
    size_t num_channels = 0;
};

///
/// Finds the first attribute with the specified usage/element type/number of channels.
///
/// @param[in]  mesh     Mesh whose attribute to retrieve.
/// @param[in]  options  Attribute properties to match against.
///
/// @tparam     Scalar   Mesh scalar type.
/// @tparam     Index    Mesh index type.
///
/// @return     The attribute id of the first matching attribute.
///
template <typename Scalar, typename Index>
std::optional<AttributeId> find_matching_attribute(
    const SurfaceMesh<Scalar, Index>& mesh,
    const AttributeMatcher& options);

///
/// Finds the first attribute with the specified usage.
///
/// @overload find_matching_attribute
///
/// @param[in]  mesh    Mesh whose attribute to retrieve.
/// @param[in]  usage   Attribute usage to match against.
///
/// @tparam     Scalar  Mesh scalar type.
/// @tparam     Index   Mesh index type.
///
/// @return     The attribute id of the first matching attribute.
///
template <typename Scalar, typename Index>
std::optional<AttributeId> find_matching_attribute(
    const SurfaceMesh<Scalar, Index>& mesh,
    AttributeUsage usage);

///
/// Finds the first attribute with the specified element types.
///
/// @overload find_matching_attribute
///
/// @param[in]  mesh           Mesh whose attribute to retrieve.
/// @param[in]  element_types  Element types to match against.
///
/// @tparam     Scalar         Mesh scalar type.
/// @tparam     Index          Mesh index type.
///
/// @return     The attribute id of the first matching attribute.
///
template <typename Scalar, typename Index>
std::optional<AttributeId> find_matching_attribute(
    const SurfaceMesh<Scalar, Index>& mesh,
    BitField<AttributeElement> element_types);

///
/// Finds all attributes with the specified usage/element type/number of channels.
///
/// @param[in]  mesh     Mesh whose attribute to retrieve.
/// @param[in]  options  Attribute properties to match against.
///
/// @tparam     Scalar   Mesh scalar type.
/// @tparam     Index    Mesh index type.
///
/// @return     A list of attribute ids of each matching attribute.
///
template <typename Scalar, typename Index>
std::vector<AttributeId> find_matching_attributes(
    const SurfaceMesh<Scalar, Index>& mesh,
    const AttributeMatcher& options);

///
/// Finds all attributes with the specified usage.
///
/// @param[in]  mesh    Mesh whose attribute to retrieve.
/// @param[in]  usage   Attribute usage to match against.
///
/// @tparam     Scalar  Mesh scalar type.
/// @tparam     Index   Mesh index type.
///
/// @return     A list of attribute ids of each matching attribute.
///
template <typename Scalar, typename Index>
std::vector<AttributeId> find_matching_attributes(
    const SurfaceMesh<Scalar, Index>& mesh,
    AttributeUsage usage);

///
/// Finds all attributes with the specified element types.
///
/// @param[in]  mesh           Mesh whose attribute to retrieve.
/// @param[in]  element_types  Element types to match against.
///
/// @tparam     Scalar         Mesh scalar type.
/// @tparam     Index          Mesh index type.
///
/// @return     A list of attribute ids of each matching attribute.
///
template <typename Scalar, typename Index>
std::vector<AttributeId> find_matching_attributes(
    const SurfaceMesh<Scalar, Index>& mesh,
    BitField<AttributeElement> element_types);

///
/// @}
///

} // namespace lagrange
