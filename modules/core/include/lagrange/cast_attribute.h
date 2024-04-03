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

#include <optional>
#include <string>

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
/// Cast an attribute in place to a different value type. This effectively replaces the existing
/// attribute with a new one.
///
/// @param[in,out] mesh         Input mesh. Modified to replace the attribute being cast.
/// @param[in]     source_id    Id of the source attribute to cast.
/// @param[in]     target_name  Name of the target attribute to be created.
///
/// @tparam        ToValueType  Target value type for the attribute.
/// @tparam        Scalar       Mesh scalar type.
/// @tparam        Index        Mesh value type.
///
template <typename ToValueType, typename Scalar, typename Index>
AttributeId cast_attribute(
    SurfaceMesh<Scalar, Index>& mesh,
    AttributeId source_id,
    std::string_view target_name);

///
/// Cast an attribute in place to a different value type. This effectively replaces the existing
/// attribute with a new one.
///
/// @param[in,out] mesh         Input mesh. Modified to replace the attribute being cast.
/// @param[in]     source_name  Name of the source attribute being cast.
/// @param[in]     target_name  Name of the target attribute to be created.
///
/// @tparam        ToValueType  Target value type for the attribute.
/// @tparam        Scalar       Mesh scalar type.
/// @tparam        Index        Mesh value type.
///
template <typename ToValueType, typename Scalar, typename Index>
AttributeId cast_attribute(
    SurfaceMesh<Scalar, Index>& mesh,
    std::string_view source_name,
    std::string_view target_name);

///
/// Cast an attribute in place to a different value type. This effectively replaces the existing
/// attribute with a new one.
///
/// @param[in,out] mesh          Input mesh. Modified to replace the attribute being cast.
/// @param[in]     attribute_id  Id of the attribute to cast.
///
/// @tparam        ToValueType   Target value type for the attribute.
/// @tparam        Scalar        Mesh scalar type.
/// @tparam        Index         Mesh value type.
///
template <typename ToValueType, typename Scalar, typename Index>
AttributeId cast_attribute_in_place(SurfaceMesh<Scalar, Index>& mesh, AttributeId attribute_id);

///
/// Cast an attribute in place to a different value type. This effectively replaces the existing
/// attribute with a new one.
///
/// @param[in,out] mesh         Input mesh. Modified to replace the attribute being cast.
/// @param[in]     name         Name of the attribute to cast.
///
/// @tparam        ToValueType  Target value type for the attribute.
/// @tparam        Scalar       Mesh scalar type.
/// @tparam        Index        Mesh value type.
///
template <typename ToValueType, typename Scalar, typename Index>
AttributeId cast_attribute_in_place(SurfaceMesh<Scalar, Index>& mesh, std::string_view name);

///
/// @}
///

} // namespace lagrange
