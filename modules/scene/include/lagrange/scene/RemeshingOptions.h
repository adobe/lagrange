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

#include <lagrange/utils/invalid.h>

#include <cstddef>

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
/// Options that define how remeshing algorithms (tessellate, decimate, quadrangulate) treat meshes within a scene.
///
struct RemeshingOptions
{
    /// Facet allocation strategy for meshes in the scene.
    FacetAllocationStrategy facet_allocation_strategy = FacetAllocationStrategy::EvenSplit;

    /// Minimum amount of facets for meshes in the scene.
    /// Set to lagrange::invalid to use backend-specific default value.
    size_t min_facets = lagrange::invalid<size_t>();
};

} // namespace lagrange::scene
