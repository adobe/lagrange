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

#include <lagrange/SurfaceMesh.h>

#include <string_view>

namespace lagrange {

///
/// @defgroup   group-surfacemesh-attr-utils Attributes utilities
/// @ingroup    group-surfacemesh
///
/// Various attribute processing utilities
///
/// @{

///
/// @name Attribute remapping
/// @{
///
/// Mapping attributes between different element types.
///
/// - When mapping indexed to value attributes, values are duplicated for each corner, since
///   otherwise the mapping corner -> value would be lost.
/// - When mapping value to indexed attributes, the size of the value buffer must be equal to the
///   number of corners. The values are not deduplicated in the new indexed attribute.
///

///
/// Map attribute values to a new attribute with a different element type. If the input attribute is
/// a value attribute, its number of rows must match the number of target mesh element (or number of
/// corners if the target is an indexed attribute).
///
/// @param[in,out] mesh         Input mesh. Modified to add a new attribute.
/// @param[in]     id           Id of the input attribute to map.
/// @param[in]     new_name     Name of the new mesh attribute to create.
/// @param[in]     new_element  New attribute element type.
///
/// @tparam        Scalar       Mesh scalar type.
/// @tparam        Index        Mesh index type.
///
/// @return        Id of the newly created attribute.
///
template <typename Scalar, typename Index>
AttributeId map_attribute(
    SurfaceMesh<Scalar, Index>& mesh,
    AttributeId id,
    std::string_view new_name,
    AttributeElement new_element);

///
/// Map attribute values to a new attribute with a different element type. If the input attribute is
/// a value attribute, its number of rows must match the number of target mesh element (or number of
/// corners if the target is an indexed attribute).
///
/// @param[in,out] mesh         Input mesh. Modified to add a new attribute.
/// @param[in]     old_name     Name of the input attribute to map.
/// @param[in]     new_name     Name of the new mesh attribute to create.
/// @param[in]     new_element  New attribute element type.
///
/// @tparam        Scalar       Mesh scalar type.
/// @tparam        Index        Mesh index type.
///
/// @return        Id of the newly created attribute.
///
template <typename Scalar, typename Index>
AttributeId map_attribute(
    SurfaceMesh<Scalar, Index>& mesh,
    std::string_view old_name,
    std::string_view new_name,
    AttributeElement new_element);

///
/// Map attribute values to a different element type. A new attribute with the new element type is
/// created with the same name as the old one, and the old one is removed. If the input attribute is
/// a value attribute, its number of rows must match the number of target mesh element (or number of
/// corners if the target is an indexed attribute).
///
/// @todo          To be truly in-place ideally the new AttributeId should be the same as the old
///                one.
///
/// @param[in,out] mesh         Input mesh. Modified to add a new attribute.
/// @param[in]     id           Id of the input attribute to map.
/// @param[in]     new_element  New attribute element type.
///
/// @tparam        Scalar       Mesh scalar type.
/// @tparam        Index        Mesh index type.
///
/// @return        Id of the modified attribute.
///
template <typename Scalar, typename Index>
AttributeId map_attribute_in_place(
    SurfaceMesh<Scalar, Index>& mesh,
    AttributeId id,
    AttributeElement new_element);

///
/// Map attribute values to a different element type. A new attribute with the new element type is
/// created with the same name as the old one, and the old one is removed. If the input attribute is
/// a value attribute, its number of rows must match the number of target mesh element (or number of
/// corners if the target is an indexed attribute).
///
/// @todo          To be truly in-place ideally the new AttributeId should be the same as the old
///                one.
///
/// @param[in,out] mesh         Input mesh. Modified to add a new attribute.
/// @param[in]     name         Name of the attribute to map.
/// @param[in]     new_element  New attribute element type.
///
/// @tparam        Scalar       Mesh scalar type.
/// @tparam        Index        Mesh index type.
///
/// @return        Id of the modified attribute.
///
template <typename Scalar, typename Index>
AttributeId map_attribute_in_place(
    SurfaceMesh<Scalar, Index>& mesh,
    std::string_view name,
    AttributeElement new_element);

/// @}
/// @}

} // namespace lagrange
