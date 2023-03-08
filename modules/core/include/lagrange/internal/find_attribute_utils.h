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
#include <lagrange/utils/BitField.h>
#include <lagrange/utils/span.h>

#include <string_view>

namespace lagrange::internal {

enum class ResetToDefault { Yes, No };

///
/// Find an attribute with a given name, ensuring the usage and element type match an expected
/// target. If the provided name is empty, the first attribute with matching properties is returned.
/// If no such attribute is found, invalid_attribute_id() is returned instead.
///
/// @param      mesh               Mesh where to look for attributes.
/// @param[in]  name               Optional name of the attribute to find. If empty, the first
///                                matching attribute id will be returned.
/// @param[in]  expected_element   Expected element type.
/// @param[in]  expected_usage     Expected attribute usage.
/// @param[in]  expected_channels  Expected number of channels. If 0, then the check is skipped.
///
/// @tparam     ExpectedValueType  Expected attribute value type.
/// @tparam     Scalar             Mesh scalar type.
/// @tparam     Index              Mesh index type.
///
/// @return     Attribute id of the first matching attribute.
///
template <typename ExpectedValueType, typename Scalar, typename Index>
AttributeId find_matching_attribute(
    const SurfaceMesh<Scalar, Index>& mesh,
    std::string_view name,
    BitField<AttributeElement> expected_element,
    AttributeUsage expected_usage,
    size_t expected_channels);

///
/// Find an attribute from a selected set of ids, ensuring the usage and element type match an
/// expected target. If the provided `selected_ids` is empty, it will search all attributes.
/// Otherwise, only attributes corresponding to `selected_ids` are searched.  The first attribute
/// with matching properties is returned. If no such attribute is found, invalid_attribute_id() is
/// returned instead.
///
/// @param      mesh               Mesh where to look for attributes.
/// @param[in]  selected_ids       Selected attribute ids.
/// @param[in]  expected_element   Expected element type.
/// @param[in]  expected_usage     Expected attribute usage.
/// @param[in]  expected_channels  Expected number of channels. If 0, then the check is skipped.
///
/// @tparam     ExpectedValueType  Expected attribute value type.
/// @tparam     Scalar             Mesh scalar type.
/// @tparam     Index              Mesh index type.
///
/// @return     Attribute id of the first matching attribute.
///
template <typename ExpectedValueType, typename Scalar, typename Index>
AttributeId find_matching_attribute(
    const SurfaceMesh<Scalar, Index>& mesh,
    span<const AttributeId> selected_ids,
    BitField<AttributeElement> expected_element,
    AttributeUsage expected_usage,
    size_t expected_channels);

///
/// Find an attribute with a given name, ensuring the usage and element type match an expected
/// target. This function does not allow empty names to be provided.
///
/// @param      mesh               Mesh where to look for attributes.
/// @param[in]  name               Name of the attribute to find.
/// @param[in]  expected_element   Expected element type.
/// @param[in]  expected_usage     Expected attribute usage.
/// @param[in]  expected_channels  Expected number of channels. If 0, then the check is skipped.
///
/// @tparam     ExpectedValueType  Expected attribute value type.
/// @tparam     Scalar             Mesh scalar type.
/// @tparam     Index              Mesh index type.
///
/// @return     Attribute id of the first matching attribute.
///
template <typename ExpectedValueType, typename Scalar, typename Index>
AttributeId find_attribute(
    const SurfaceMesh<Scalar, Index>& mesh,
    std::string_view name,
    BitField<AttributeElement> expected_element,
    AttributeUsage expected_usage,
    size_t expected_channels);

///
/// Either retrieve or create an attribute with a prescribed name, element type and usage. When
/// retrieving an existing attribute, this function performs additional sanity checks, such as
/// ensuring that the attribute usage is correctly set, that the number of channels is correct, etc.
///
/// @param      mesh               Mesh whose attribute to retrieve.
/// @param[in]  name               Name of the attribute to retrieve.
/// @param[in]  expected_element   Expected element type.
/// @param[in]  expected_usage     Expected attribute usage.
/// @param[in]  expected_channels  Expected number of channels. If 0, then the check is skipped.
/// @param[in]  reset_tag          Whether to reset attribute values to default (if attribute is not created).
///
/// @tparam     ExpectedValueType  Expected attribute value type.
/// @tparam     Scalar             Mesh scalar type.
/// @tparam     Index              Mesh index type.
///
/// @return     Attribute id for the retrieved attribute.
///
template <typename ExpectedValueType, typename Scalar, typename Index>
AttributeId find_or_create_attribute(
    SurfaceMesh<Scalar, Index>& mesh,
    std::string_view name,
    AttributeElement expected_element,
    AttributeUsage expected_usage,
    size_t expected_channels,
    ResetToDefault reset_tag);

} // namespace lagrange::internal
