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

#include <lagrange/scene/RemeshingOptions.h>
#include <lagrange/scene/SimpleScene.h> // FIXME: forward declare

#include <vector>

namespace lagrange::scene {

///
/// @addtogroup module-scene
/// @{
///

///
/// Computes one weight per source mesh, indexed by mesh index.
///
/// EvenSplit includes all meshes equally. RelativeToNumFacets uses each source mesh's facet count,
/// independent of instances. RelativeToMeshArea uses the maximum transformed surface area over
/// each mesh's instances; a mesh without instances has zero area weight. Synchronized gives the
/// best facet budget allocation but is not weight-based: it is handled directly by the remeshing
/// backends, and passing it to this function throws a std::runtime_error.
///
/// @param[in]  scene                       Input scene.
/// @param[in]  facet_allocation_strategy   Strategy used to compute the weights distribution.
///
/// @tparam     Scalar      Scalar type of scene.
/// @tparam     Index       Index type of scene.
/// @tparam     Dimension   Dimension value of scene.
///
/// @return     Weights in [0, 1], normalized to sum to one when the total weight is positive.
///             If all areas or facet counts are zero, the corresponding strategy returns all
///             zeros. An empty scene returns an empty vector.
///
/// @throws     std::runtime_error if the strategy is FacetAllocationStrategy::Synchronized.
///
template <typename Scalar, typename Index, size_t Dimension>
std::vector<double> compute_mesh_weights(
    const SimpleScene<Scalar, Index, Dimension>& scene,
    const FacetAllocationStrategy facet_allocation_strategy);

/// @}

} // namespace lagrange::scene
