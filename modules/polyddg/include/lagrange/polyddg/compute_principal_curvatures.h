/*
 * Copyright 2026 Adobe. All rights reserved.
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
#include <lagrange/polyddg/DifferentialOperators.h>
#include <lagrange/polyddg/api.h>

#include <string_view>

namespace lagrange::polyddg {

/// @addtogroup module-polyddg
/// @{

///
/// Options for compute_principal_curvatures().
///
struct PrincipalCurvaturesOptions
{
    /// Output attribute name for the minimum principal curvature (scalar, per vertex).
    std::string_view kappa_min_attribute = "@kappa_min";

    /// Output attribute name for the maximum principal curvature (scalar, per vertex).
    std::string_view kappa_max_attribute = "@kappa_max";

    /// Output attribute name for the principal direction of kappa_min (3-D vector, per vertex).
    std::string_view direction_min_attribute = "@principal_direction_min";

    /// Output attribute name for the principal direction of kappa_max (3-D vector, per vertex).
    std::string_view direction_max_attribute = "@principal_direction_max";
};

///
/// Result of compute_principal_curvatures().
///
struct PrincipalCurvaturesResult
{
    /// Attribute ID of the minimum principal curvature attribute.
    AttributeId kappa_min_id = invalid_attribute_id();

    /// Attribute ID of the maximum principal curvature attribute.
    AttributeId kappa_max_id = invalid_attribute_id();

    /// Attribute ID of the principal direction for kappa_min.
    AttributeId direction_min_id = invalid_attribute_id();

    /// Attribute ID of the principal direction for kappa_max.
    AttributeId direction_max_id = invalid_attribute_id();
};

///
/// Compute per-vertex principal curvatures and principal curvature directions.
///
/// Performs an eigendecomposition of the adjoint shape operator at each vertex. The two
/// eigenvalues are the principal curvatures (kappa_min <= kappa_max) and the corresponding
/// eigenvectors, mapped back to 3-D through the vertex tangent basis, are the principal
/// directions. Results are stored as vertex attributes in the mesh.
///
/// @param[in,out] mesh    Input surface mesh. Output attributes are added or overwritten.
/// @param[in]     ops     Precomputed differential operators for the mesh.
/// @param[in]     options Attribute name options. Defaults produce attributes named
///                        @kappa_min, @kappa_max, @principal_direction_min,
///                        @principal_direction_max.
///
/// @return Attribute IDs of the four output attributes.
///
template <typename Scalar, typename Index>
LA_POLYDDG_API PrincipalCurvaturesResult compute_principal_curvatures(
    SurfaceMesh<Scalar, Index>& mesh,
    const DifferentialOperators<Scalar, Index>& ops,
    PrincipalCurvaturesOptions options = {});

///
/// Compute per-vertex principal curvatures and principal curvature directions.
///
/// Convenience overload that constructs a DifferentialOperators object internally.
///
/// @param[in,out] mesh    Input surface mesh. Output attributes are added or overwritten.
/// @param[in]     options Attribute name options.
///
/// @return Attribute IDs of the four output attributes.
///
template <typename Scalar, typename Index>
LA_POLYDDG_API PrincipalCurvaturesResult compute_principal_curvatures(
    SurfaceMesh<Scalar, Index>& mesh,
    PrincipalCurvaturesOptions options = {});

/// @}

} // namespace lagrange::polyddg
