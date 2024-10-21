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
#include <lagrange/utils/function_ref.h>
#include <lagrange/utils/invalid.h>
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
/// Option settings for `separate_by_facet_groups`.
///
struct SeparateByFacetGroupsOptions
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

    SeparateByFacetGroupsOptions() = default;
    SeparateByFacetGroupsOptions(const SeparateByFacetGroupsOptions&) = default;
    SeparateByFacetGroupsOptions(SeparateByFacetGroupsOptions&&) = default;
    SeparateByFacetGroupsOptions& operator=(const SeparateByFacetGroupsOptions&) = default;
    SeparateByFacetGroupsOptions& operator=(SeparateByFacetGroupsOptions&&) = default;

    /// Explicit conversion from other compatible option types.
    template <typename T>
    explicit SeparateByFacetGroupsOptions(const T& options)
    {
        source_vertex_attr_name = options.source_vertex_attr_name;
        source_facet_attr_name = options.source_facet_attr_name;
        map_attributes = options.map_attributes;
    }
};

///
/// Extract a set of submeshes based on facet groups.
///
/// Facets with the same group index are grouped together in a single submesh.
///
/// @tparam Scalar                  The scalar type.
/// @tparam Index                   The index type.
///
/// @param[in] mesh                 The source mesh.
/// @param[in] num_groups           The number of face groups.
/// @param[in] facet_group_indices  The group index of each facet. Each group index must be in the range
///                                 of [0, num_groups - 1].
/// @param[in] options              Extraction options.
///
/// @return A list of submeshes representing each facet group.
///
template <typename Scalar, typename Index>
std::vector<SurfaceMesh<Scalar, Index>> separate_by_facet_groups(
    const SurfaceMesh<Scalar, Index>& mesh,
    size_t num_groups,
    span<const Index> facet_group_indices,
    const SeparateByFacetGroupsOptions& options = {});

///
/// Extract a set of submeshes based on facet groups.
///
/// Facets with the same group index are grouped together in a single submesh.
///
/// @tparam Scalar                  The scalar type.
/// @tparam Index                   The index type.
///
/// @param[in] mesh                 The source mesh.
/// @param[in] facet_group_indices  The group index of each facet. Each group index must be in the range
///                                 of [0, max(facet_group_indices)].
/// @param[in] options              Extraction options.
///
/// @return A list of submeshes representing each facet group.
///
template <typename Scalar, typename Index>
std::vector<SurfaceMesh<Scalar, Index>> separate_by_facet_groups(
    const SurfaceMesh<Scalar, Index>& mesh,
    span<const Index> facet_group_indices,
    const SeparateByFacetGroupsOptions& options = {});

///
/// Extract a set of submeshes based on facet groups.
///
/// Facets with the same group index are grouped together in a single submesh.
///
/// @tparam Scalar              The scalar type.
/// @tparam Index               The index type.
///
/// @param[in] mesh             The source mesh.
/// @param[in] num_groups       The number of face groups.
/// @param[in] get_facet_group  Function that returns the facet group id from facet id.
///                             The groud id must be in [0, num_groups - 1].
/// @param[in] options          Extraction options.
///
/// @return A list of submeshes representing each facet group.
///
template <typename Scalar, typename Index>
std::vector<SurfaceMesh<Scalar, Index>> separate_by_facet_groups(
    const SurfaceMesh<Scalar, Index>& mesh,
    size_t num_groups,
    function_ref<Index(Index)> get_facet_group,
    const SeparateByFacetGroupsOptions& options = {});

/// @}

} // namespace lagrange
