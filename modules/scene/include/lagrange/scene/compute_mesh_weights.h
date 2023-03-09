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

#include <lagrange/scene/SimpleScene.h> // FIXME: forward declare

#include <vector>

namespace lagrange::scene {

///
/// Facet allocation strategy.
///
enum class FacetAllocationStrategy {
    /// Split facet budget evenly between all meshes in a scene.
    EvenSplit,
    /// Allocate facet budget according to the mesh area in the scene.
    RelativeToMeshArea,
    /// Allocate facet budget according to the mesh number of facets.
    RelativeToNumFacets
};

///
/// Computes mesh weights of a scene
///
/// @param[in]  scene                       Input scene.
/// @param[in]  facet_allocation_strategy   Strategy used to compute the weights distribution.
///
/// @tparam     Scalar      Scalar type of scene.
/// @tparam     Index       Index type of scene.
/// @tparam     Dimension   Dimension value of scene.
///
/// @return     vector<double>  Weights for each mesh of the scene that sums to unity and each being in [0, 1].
///
template <typename Scalar, typename Index, size_t Dimension>
std::vector<double> compute_mesh_weights(
    const SimpleScene<Scalar, Index, Dimension>& scene,
    const FacetAllocationStrategy facet_allocation_strategy);

} // namespace lagrange::scene
