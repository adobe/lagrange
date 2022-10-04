/*
 * Copyright 2020 Adobe. All rights reserved.
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
    #include <lagrange/legacy/compute_normal.h>
#endif

#include <lagrange/NormalWeightingType.h>
#include <lagrange/SurfaceMesh.h>
#include <lagrange/utils/function_ref.h>
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
/// Option struct for computing indexed mesh normals.
///
struct NormalOptions
{
    /// Output normal attribute name.
    std::string_view output_attribute_name = "@normal";

    /// Per-vertex normal averaging weighting type.
    NormalWeightingType weight_type = NormalWeightingType::Angle;

    /// Precomputed facet normal attribute name. Used to determine the orientation of accumulated
    /// corner normals. If the attribute does not exist, the algorithm will compute it.
    std::string_view facet_normal_attribute_name = "@facet_normal";

    /// Whether to recompute the facet normal attribute, or reuse existing cached values if present.
    bool recompute_facet_normals = false;

    /// Whether to keep any newly added facet normal attribute. If such an attribute is already
    /// present in the input mesh, it will not be removed, even if this argument is set to false.
    bool keep_facet_normals = false;
};

/**
 * Compute smooth normals based on specified sharp edges and cone vertices.
 *
 * @param[in]  mesh            The input mesh.
 * @param[in]  is_edge_smooth  Returns true on e if the edge is smooth.
 * @param[in]  cone_vertices   A list of cone vertices.
 * @param[in]  options         Optional arguments to control normal generation.
 *
 * @tparam     Scalar          Mesh scalar type.
 * @tparam     Index           Mesh index type.
 *
 * @return     The indexed attribute id of normal attribute.
 *
 * @see        `NormalOptions`.
 */
template <typename Scalar, typename Index>
AttributeId compute_normal(
    SurfaceMesh<Scalar, Index>& mesh,
    function_ref<bool(Index)> is_edge_smooth,
    span<const Index> cone_vertices = {},
    NormalOptions options = {});

/**
 * Compute smooth normals based on specified sharp edges and cone vertices.
 *
 * @param[in]  mesh            The input mesh.
 * @param[in]  is_edge_smooth  Returns true on (fi, fj) if the edge between fi and fj is smooth.
 *                             Assumes fi and fi are adjacent.
 * @param[in]  cone_vertices   A list of cone vertices.
 * @param[in]  options         Optional arguments to control normal generation.
 *
 * @tparam     Scalar          Mesh scalar type.
 * @tparam     Index           Mesh index type.
 *
 * @return     The indexed attribute id of normal attribute.
 *
 * @see        `NormalOptions`.
 */
template <typename Scalar, typename Index>
AttributeId compute_normal(
    SurfaceMesh<Scalar, Index>& mesh,
    function_ref<bool(Index, Index)> is_edge_smooth,
    span<const Index> cone_vertices = {},
    NormalOptions options = {});

/**
 * Compute smooth normal based on specified dihedral angle threshold and cone vertices.
 *
 * @param[in]  mesh                     The input mesh.
 * @param[in]  feature_angle_threshold  An edge with dihedral angle larger than this threshold is
 *                                      considered as an feature edge.
 * @param[in]  cone_vertices            A list of cone vertices.
 * @param[in]  options                  Optional arguments to control normal generation.
 *
 * @tparam     Scalar                   Mesh scalar type.
 * @tparam     Index                    Mesh index type.
 *
 * @return     The indexed attribute id of normal attribute.
 *
 * @see        `NormalOptions`.
 */
template <typename Scalar, typename Index>
AttributeId compute_normal(
    SurfaceMesh<Scalar, Index>& mesh,
    Scalar feature_angle_threshold,
    span<const Index> cone_vertices = {},
    NormalOptions options = {});

/// @}

} // namespace lagrange
