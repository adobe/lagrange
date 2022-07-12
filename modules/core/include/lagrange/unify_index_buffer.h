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
#include <vector>

namespace lagrange {

///
/// @defgroup   group-surfacemesh-attr-utils Attributes utilities
/// @ingroup    group-surfacemesh
///
/// Various attribute processing utilities
///
/// @{

///
/// @name Index buffer unification
/// @{

/**
 * Unify index buffers of the input `mesh` for all attributes specified in `attribute_ids`.
 *
 * @post The attributes specified by `attribute_id` will become vertex
 * attributes in the output mesh under the same names.
 *
 * @param[in]  mesh  The input polygonal mesh.
 * @param[in]  attribute_ids  The input indexed attribute ids to be unified.  If
 * empty, all indexed attributes are unified.
 *
 * @returns A polygonal mesh with unified indices for position and attributes
 * specified in `attribute_ids`.
 */
template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> unify_index_buffer(
    const SurfaceMesh<Scalar, Index>& mesh,
    const std::vector<AttributeId>& attribute_ids = {});

/**
 * @overload
 *
 * @param[in]  mesh  The input polygonal mesh.
 * @param[in]  attribute_names  The input indexed attribute names to be unified.
 * If empty, all indexed attributes are unified.
 *
 * @return  A polygonal mesh with unified indices for position and attributes
 * specified in `attribute_names`.
 *
 * @see unify_index_buffer(const SurfaceMesh<Scalar, Index>&, const std::vector<AttributeId>&) for
 * more info.
 */
template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> unify_named_index_buffer(
    const SurfaceMesh<Scalar, Index>& mesh,
    const std::vector<std::string_view>& attribute_names);

/// @}
/// @}

} // namespace lagrange
