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
#include <polyscope/curve_network.h>
#include <lagrange/utils/warnon.h>
// clang-format on

namespace lagrange::polyscope {

///
/// Registers an edge network (curve network) with Polyscope.
///
/// @param[in]  name    Name of the edge network in Polyscope
/// @param[in]  mesh    Surface mesh to register (must have size-2 facets only)
///
/// @tparam     Scalar  Mesh scalar type.
/// @tparam     Index   Mesh index type.
///
/// @return     A pointer to the registered Polyscope curve network
///
template <typename Scalar, typename Index>
::polyscope::CurveNetwork* register_edge_network(
    std::string_view name,
    const SurfaceMesh<Scalar, Index>& mesh);

///
/// Manually registers an attribute on a Polyscope curve network. This is useful when creating an
/// attribute on an edge network after it has been registered with Polyscope.
///
/// @param[in,out] ps_curve_network  The Polyscope curve network
/// @param[in]     name              Name of the attribute in Polyscope
/// @param[in]     attr              Attribute to register or update
///
/// @tparam        ValueType         Attribute value type.
///
/// @return        A pointer to the registered or updated Polyscope quantity
///
template <typename ValueType>
::polyscope::CurveNetworkQuantity* register_attribute(
    ::polyscope::CurveNetwork& ps_curve_network,
    std::string_view name,
    const lagrange::Attribute<ValueType>& attr);

} // namespace lagrange::polyscope
