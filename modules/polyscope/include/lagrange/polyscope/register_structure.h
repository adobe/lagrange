/*
 * Copyright 2025 Adobe. All rights reserved.
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

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <polyscope/structure.h>
#include <lagrange/utils/warnon.h>
// clang-format on

namespace lagrange::polyscope {

///
/// Registers a structure with Polyscope. If the input mesh has no facets, it registers a point
/// cloud; if the input mesh has only size-2 facets, it registers an edge network; otherwise, it
/// registers a surface mesh.
///
/// @param[in]  name    Name of the structure in Polyscope
/// @param[in]  mesh    Surface mesh to register
///
/// @tparam     Scalar  Mesh scalar type.
/// @tparam     Index   Mesh index type.
///
/// @return     A pointer to the registered Polyscope structure
///
template <typename Scalar, typename Index>
::polyscope::Structure* register_structure(
    std::string_view name,
    const SurfaceMesh<Scalar, Index>& mesh);

///
/// Manually registers an attribute on a Polyscope structure. This is useful when creating an
/// attribute on a structure after it has been registered with Polyscope.
///
/// @param[in,out] ps_struct  The Polyscope structure
/// @param[in]     name       Name of the attribute in Polyscope
/// @param[in]     attr       Attribute to register or update
///
/// @tparam        ValueType  Attribute value type.
///
/// @return        A pointer to the registered or updated Polyscope quantity
///
template <typename ValueType>
::polyscope::Quantity* register_attribute(
    ::polyscope::Structure& ps_struct,
    std::string_view name,
    const lagrange::Attribute<ValueType>& attr);

} // namespace lagrange::polyscope
