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
#include <lagrange/utils/span.h>

namespace lagrange {

///
/// @defgroup   group-surfacemesh-utils Mesh utilities
/// @ingroup    group-surfacemesh
///
/// Various mesh processing utilities.
///
/// @{

/**
 * Reorder facets of a mesh based on a given permutation. i.e. rearrangement facets so that they
 * are ordered as specified by the `new_to_old` index array. The total number of facets is
 * unchanged.
 *
 * @param[in,out]  mesh        The target mesh whose facets will be reordered in place.
 * @param[in]      new_to_old  The permutation index array specifying the new facet order.
 *                             This array can often be obtained via index-based sorting of the
 *                             facets with customized comparison.
 *
 * @see `extract_submesh` to extract a subset of the facets.
 */
template <typename Scalar, typename Index>
void permute_facets(SurfaceMesh<Scalar, Index>& mesh, span<const Index> new_to_old);

/// @}

} // namespace lagrange
