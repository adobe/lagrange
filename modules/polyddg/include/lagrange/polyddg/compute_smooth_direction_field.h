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

#include <cstdint>
#include <string_view>

namespace lagrange::polyddg {

/// @addtogroup module-polyddg
/// @{

///
/// Options for compute_smooth_direction_field().
///
struct SmoothDirectionFieldOptions
{
    /// Symmetry order of the direction field (e.g. 1 = vector field, 2 = line field,
    /// 4 = cross field).
    uint8_t nrosy = 4;

    /// Stabilization weight for the VEM projection term in the connection Laplacian.
    double lambda = 1.0;

    /// Name of a per-vertex 3-D tangent vector field attribute used as alignment constraints.
    /// Each vertex with a non-zero vector is softly constrained to align to that direction.
    /// Vertices with a zero vector are unconstrained. If empty (the default), no alignment
    /// constraints are applied and the globally smoothest field is computed via inverse power
    /// iteration.
    std::string_view alignment_attribute = "";

    /// Scaling factor for the spectral shift in the alignment solve, following the fieldgen
    /// formulation (Knöppel et al. 2013). The actual shift is @f$ \alpha = s \cdot
    /// \sigma_{\min} @f$, where @f$ s @f$ is this value and @f$ \sigma_{\min} @f$ is the
    /// smallest eigenvalue of the connection Laplacian (computed automatically). At the
    /// default value of 1.0, the shift equals @f$ \sigma_{\min} @f$, giving maximum
    /// alignment. Values in (0, 1) give weaker alignment (more smoothness).
    double alignment_weight = 1.0;

    /// Output attribute name for the smooth direction field (3-D vector, per vertex).
    std::string_view direction_field_attribute = "@smooth_direction_field";
};

///
/// Compute the globally smoothest n-direction field on a surface mesh.
///
/// This function is based on the following paper:
///
/// Knöppel, Felix, et al. "Globally optimal direction fields." ACM Transactions on Graphics (ToG)
/// 32.4 (2013): 1-10.
///
/// The solution is stored as a per-vertex 3-D tangent vector attribute in world-space
/// coordinates, obtained by mapping the local 2-D solution through the vertex tangent basis.
///
/// @param[in,out] mesh    Input surface mesh. The output attribute is added or overwritten.
/// @param[in]     ops     Precomputed differential operators for the mesh.
/// @param[in]     options Options controlling the rosy order, stabilization weight,
///                        optional alignment constraints, and output attribute name.
///
/// @return Attribute ID of the output direction field attribute.
///
template <typename Scalar, typename Index>
LA_POLYDDG_API AttributeId compute_smooth_direction_field(
    SurfaceMesh<Scalar, Index>& mesh,
    const DifferentialOperators<Scalar, Index>& ops,
    SmoothDirectionFieldOptions options = {});

/// @}

} // namespace lagrange::polyddg
