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
#pragma once

#include <lagrange/SurfaceMesh.h>
#include <lagrange/filter_attributes.h>

#include <optional>
#include <string>
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
/// Cast a mesh to a mesh of different scalar and/or index type.
///
/// @note       To filter only certain attributes prior to casting a mesh, use the filter_attributes
///             function.
///
/// @param[in]  source_mesh                 Input mesh.
/// @param[in]  convertible_attributes      Filter to determine which attribute are convertible.
/// @param[out] converted_attributes_names  Optional output arg storing the list of non-reserved
///                                         attribute names that were actually converted to a
///                                         different type.
///
/// @tparam     ToScalar                    Scalar type of the output mesh.
/// @tparam     ToIndex                     Index type of the output mesh.
/// @tparam     FromScalar                  Scalar type of the input mesh.
/// @tparam     FromIndex                   Index type of the input mesh.
///
/// @return     Output mesh.
///
/// @see        filter_attributes
///
template <typename ToScalar, typename ToIndex, typename FromScalar, typename FromIndex>
SurfaceMesh<ToScalar, ToIndex> cast(
    const SurfaceMesh<FromScalar, FromIndex>& source_mesh,
    const AttributeFilter& convertible_attributes = {},
    std::vector<std::string>* converted_attributes_names = nullptr);

///
/// @}
///

} // namespace lagrange
