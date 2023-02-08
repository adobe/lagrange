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

#include <Eigen/Core>

namespace lagrange {
///
/// @defgroup   group-surfacemesh-utils Mesh utilities
/// @ingroup    group-surfacemesh
///
/// Various mesh processing utilities.
///
/// @{

///
/// Option struct for computing per-facet area.
///
struct FacetAreaOptions
{
    /// Output attribute name for facet area.
    std::string_view output_attribute_name = "@facet_area";

    /// For 2D mesh only: whether the computed area should be signed.
    bool use_signed_area = true;
};

///
/// Compute per-facet area.
///
/// @param[in,out] mesh     The input mesh.
/// @param[in]     options  The options controlling the computation.
///
/// @tparam        Scalar   Mesh scalar type.
/// @tparam        Index    Mesh index type.
///
/// @return        The attribute id of the facet area attribute.
/// @see           `FacetAreaOptions`
///
template <typename Scalar, typename Index>
AttributeId compute_facet_area(
    SurfaceMesh<Scalar, Index>& mesh,
    FacetAreaOptions options = {});

///
/// Compute per-facet area.
///
/// @param[in,out] mesh            The input mesh.
/// @param[in]     transformation  Affine transformation to apply on mesh geometry.
/// @param[in]     options         The options controlling the computation.
///
/// @tparam        Scalar          Mesh scalar type.
/// @tparam        Index           Mesh index type.
/// @tparam        Dimension       Mesh dimension.
///
/// @return        The attribute id of the facet area attribute.
/// @see           `FacetAreaOptions`
///
template <typename Scalar, typename Index, int Dimension>
AttributeId compute_facet_area(
    SurfaceMesh<Scalar, Index>& mesh,
    const Eigen::Transform<Scalar, Dimension, Eigen::Affine>& transformation,
    FacetAreaOptions options = {});

///
/// Option struct for computing mesh area.
///
struct MeshAreaOptions
{
    /// Precomputed facet area attribute name.
    /// If the attribute does not exist, a temp attribute will be computed and stored in this name.
    std::string_view input_attribute_name = "@facet_area";

    /// For 2D mesh only: whether the computed facet area (if any) should be signed.
    bool use_signed_area = true;
};

///
/// Compute mesh area.
///
/// @param[in]  mesh     The input mesh.
/// @param[in]  options  The options controlling the computation.
///
/// @tparam     Scalar   Mesh scalar type.
/// @tparam     Index    Mesh index type.
///
/// @return     The computed mesh area.
/// @see        `MeshAreaOptions`
///
template <typename Scalar, typename Index>
Scalar compute_mesh_area(
    const SurfaceMesh<Scalar, Index>& mesh,
    MeshAreaOptions options = {});

///
/// Compute mesh area.
///
/// @param[in]  mesh            The input mesh.
/// @param[in]  transformation  Affine transformation to apply on mesh geometry.
/// @param[in]  options         The options controlling the computation.
///
/// @tparam     Scalar          Mesh scalar type.
/// @tparam     Index           Mesh index type.
/// @tparam     Dimension       Mesh dimension.
///
/// @return     The computed mesh area.
/// @see        `MeshAreaOptions`
///
template <typename Scalar, typename Index, int Dimension>
Scalar compute_mesh_area(
    const SurfaceMesh<Scalar, Index>& mesh,
    const Eigen::Transform<Scalar, Dimension, Eigen::Affine>& transformation,
    MeshAreaOptions options = {});

/// @}
} // namespace lagrange
