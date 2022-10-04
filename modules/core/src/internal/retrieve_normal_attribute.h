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

namespace lagrange::internal {

///
/// Either retrieve or create a normal attribute with a given name. When retrieving an existing
/// attribute, this function performs additional sanity checks, such as ensuring that the attribute
/// usage is correctly set, that the number of channels is correct, etc.
///
/// @param      mesh     Mesh whose attribute to retrieve.
/// @param[in]  name     Name of the attribute to retrieve.
/// @param[in]  element  Element type of the attribute to retrieve.
///
/// @tparam     Scalar   Mesh scalar type.
/// @tparam     Index    Mesh index type.
///
/// @return     Attribute id for the retrieved attribute.
///
template <typename Scalar, typename Index>
AttributeId retrieve_normal_attribute(
    SurfaceMesh<Scalar, Index>& mesh,
    std::string_view name,
    AttributeElement element);

} // namespace lagrange::internal
