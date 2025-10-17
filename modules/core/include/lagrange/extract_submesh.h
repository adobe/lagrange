/*
 * Copyright 2019 Adobe. All rights reserved.
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

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
    #include <lagrange/legacy/extract_submesh.h>
#endif

#include <lagrange/SurfaceMesh.h>
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
/// Options for extract submesh.
///
struct SubmeshOptions
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

    SubmeshOptions() = default;
    SubmeshOptions(const SubmeshOptions&) = default;
    SubmeshOptions(SubmeshOptions&&) = default;
    SubmeshOptions& operator=(const SubmeshOptions&) = default;
    SubmeshOptions& operator=(SubmeshOptions&&) = default;

    ///
    /// Explicit conversion from other compatible option structs.
    ///
    template <typename T>
    explicit SubmeshOptions(const T& options)
    {
        source_vertex_attr_name = options.source_vertex_attr_name;
        source_facet_attr_name = options.source_facet_attr_name;
        map_attributes = options.map_attributes;
    }
};

///
/// Extract a submesh that consists of a subset of the facets of the source mesh.
///
/// @tparam Scalar              The scalar type.
/// @tparam Index               The index type.
///
/// @param[in]  mesh            The source mesh.
/// @param[in]  selected_facets The set of selected facets to extract.
/// @param[in]  options         Extraction options.
///
/// @return The mesh containing the selected facets.
///
/// @see @ref SubmeshOptions
///
template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> extract_submesh(
    const SurfaceMesh<Scalar, Index>& mesh,
    span<const Index> selected_facets,
    const SubmeshOptions& options = {});


/// @}

} // namespace lagrange
