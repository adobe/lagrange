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
/// @defgroup   group-surfacemesh-utils Mesh utilities
/// @ingroup    group-surfacemesh
///
/// Various mesh processing utilities
///
/// @{

/**
 * Compute facet normals.
 *
 * @param[in, out] mesh    The input mesh.
 * @param[in]      name    The facet normal attribute name.
 *
 * @tparam         Scalar  Mesh scalar type.
 * @tparam         Index   Mesh index type.
 *
 * @return         AttributeId  The attribute id of the facet normal attribute.
 *
 * @post           The computed facet normals are stored in `mesh` as a facet attribute named
 *                 `name`.
 *
 * @note           Non-planar polygonal facet's normal is not well defined.  This method can only
 *                 compute an approximated normal using a triangle fan.
 */
template <typename Scalar, typename Index>
AttributeId compute_facet_normal(
    SurfaceMesh<Scalar, Index>& mesh,
    std::string_view name = "@facet_normal");

/// @}

} // namespace lagrange
