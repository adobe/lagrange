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
#include <lagrange/types/ConnectivityType.h>

namespace lagrange {

///
/// @defgroup   group-surfacemesh-utils Mesh utilities
/// @ingroup    group-surfacemesh
///
/// Various mesh processing utilities.
///
/// @{

/**
 * Options to control connected components computation.
 */
struct ComponentOptions
{
    using ConnectivityType = lagrange::ConnectivityType;

    /// Output component id attribute name.
    std::string_view output_attribute_name = "@component_id";

    /// Connectivity type used for component computation.
    ConnectivityType connectivity_type = ConnectivityType::Edge;
};

/**
 * Compute connected components of an input mesh.
 *
 * This method will create a per-facet component id in an attribute named
 * `ComponentOptions::output_attribute_name`.  Each component id is in [0, num_components-1].
 *
 * @param      mesh     Input mesh.
 * @param      options  Options to control component computation.
 *
 * @tparam     Scalar   Mesh scalar type.
 * @tparam     Index    Mesh index type.
 *
 * @return     The total number of connected components.
 *
 * @see        @ref ComponentOptions
 */
template <typename Scalar, typename Index>
size_t compute_components(SurfaceMesh<Scalar, Index>& mesh, ComponentOptions options = {});

/**
 * Compute connected components of an input mesh.
 *
 * This method will create a per-facet component id in an attribute named
 * `ComponentOptions::output_attribute_name`.  Each component id is in [0, num_components-1].
 *
 * @param      mesh             Input mesh.
 * @param      blocker_elements An array of blocker element indices. The blocker element index is
 *                              either a vertex index or an edge index depending on
 *                              `options.connectivity_type`. If `options.connectivity_type` is
 *                              `ConnectivityType::Edge`, facets adjacent to a blocker edge are not
 *                              considered as connected through this edge. If
 *                              `options.connectivity_type` is `ConnectivityType::Vertex`, facets
 *                              sharing a blocker vertex are not considered as connected through
 *                              this vertex. If empty, no blocker elements are used.
 * @param      options          Options to control component computation.
 *
 * @tparam     Scalar   Mesh scalar type.
 * @tparam     Index    Mesh index type.
 *
 * @return     The total number of connected components.
 *
 * @see        @ref ComponentOptions
 */
template <typename Scalar, typename Index>
size_t compute_components(
    SurfaceMesh<Scalar, Index>& mesh,
    span<const Index> blocker_elements,
    ComponentOptions options = {});

/// @}

} // namespace lagrange
