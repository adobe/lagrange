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
#include <lagrange/types/ConnectivityType.h>
#include <lagrange/utils/span.h>

#include <string_view>

namespace lagrange {

///
/// @defgroup   group-surfacemesh-utils Mesh utilities
/// @ingroup    group-surfacemesh
///
/// Various mesh processing utilities.
///
/// @{

///
/// Option settings for `separate_by_components`.
///
struct SeparateByComponentsOptions
{
    /// The name of the output attribute holding source vertex indices.
    ///
    /// @note If empty, source vertex mapping will not be computed.
    std::string_view source_vertex_attr_name;

    /// The name of the output attribute holding source facet indices.
    ///
    /// @note If empty, source facet mapping will not be computed.
    std::string_view source_facet_attr_name;

    /// Map all attributes over to submesh.
    bool map_attributes = false;

    /// Connectivity type used for component computation.
    ConnectivityType connectivity_type = ConnectivityType::Edge;
};

///
/// Separate a mesh by connected components.
///
/// @tparam Scalar   The scalar type.
/// @tparam Index    The index type.
///
/// @param[in] mesh      The source mesh.
/// @param[in] options   Option settings.
///
/// @return A list of meshes representing the set of connected components.
///
/// @see @ref SeparateByComponentsOptions
///
template <typename Scalar, typename Index>
std::vector<SurfaceMesh<Scalar, Index>> separate_by_components(
    const SurfaceMesh<Scalar, Index>& mesh,
    const SeparateByComponentsOptions& options = {});

/// @}
} // namespace lagrange
