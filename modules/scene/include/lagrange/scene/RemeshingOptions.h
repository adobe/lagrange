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
#include <vector>

namespace lagrange::scene {

///
/// @addtogroup module-scene
/// @{
///

///
/// Facet allocation strategy.
///
enum class FacetAllocationStrategy {
    /// Split facet budget evenly between all meshes in a scene.
    EvenSplit,
    /// Allocate facet budget according to the mesh area in the scene.
    RelativeToMeshArea,
    /// Allocate facet budget according to the mesh number of facets.
    RelativeToNumFacets,
    /// Synchronize simplification between multiple meshes in a scene by computing a conservative
    /// threshold on the QEF error of all edges in the scene. This option gives the best result in
    /// terms of facet budget allocation, but is a bit slower than other options.
    Synchronized,
};

///
/// Strategy for processing meshes without instances in a scene.
///
enum class UninstantiatedMeshesStrategy {
    /// Use backend-specific default behavior.
    None,
    /// Skip meshes with zero instances, leaving the original in the output scene.
    Skip,
    /// Replace meshes with zero instances with an empty mesh instead.
    ReplaceWithEmpty,
};

///
/// Options that define how remeshing algorithms (tessellate, decimate, quadrangulate) treat meshes
/// within a scene.
///
struct RemeshingOptions
{
    /// Facet allocation strategy for meshes in the scene.
    FacetAllocationStrategy facet_allocation_strategy = FacetAllocationStrategy::EvenSplit;

    /// Minimum number of facets per mesh.
    /// Set to lagrange::invalid<size_t>() to use the backend-specific default.
    size_t min_facets = lagrange::invalid<size_t>();

    /// Behavior for meshes without instances in the scene.
    UninstantiatedMeshesStrategy uninstantiated_meshes_strategy =
        UninstantiatedMeshesStrategy::None;

    /// Optional positive per-instance importance values. Empty means no weighting; otherwise one
    /// value is required per instance, ordered by mesh index, then local instance index (the order
    /// of SimpleScene::foreach_instances()).
    std::vector<float> per_instance_importance;
};

/// @}

} // namespace lagrange::scene
