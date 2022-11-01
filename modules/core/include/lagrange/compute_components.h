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
    /// Ouptut component id attribute name.
    std::string_view output_attribute_name = "@component_id";

    /// Connectivity types
    enum class ConnectivityType {
        Vertex, ///< Two facets are considered connected if they share a vertex.
        Edge ///< Two facets are considered connected if they share an edge.
    };

    /// Connectivity type used for component computation.
    ConnectivityType connectivity_type = ConnectivityType::Edge;
};

/**
 * Compute connected components of an input mesh.
 *
 * This methods will create a per-facet component id in an attributed named
 * `ComponentOptions::output_attribute_name`.  Each component id is in [0, num_components).
 *
 * @param      mesh     Input mesh.
 * @param      options  Options to control component computation.
 *
 * @tparam     Scalar   Mesh scalar type.
 * @tparam     Index    Mesh index type.
 *
 * @return     The total number of connected components.
 *
 * @see        `ComopnentOptions`
 */
template <typename Scalar, typename Index>
size_t compute_components(
    SurfaceMesh<Scalar, Index>& mesh,
    ComponentOptions options = {});

/// @}

} // namespace lagrange
