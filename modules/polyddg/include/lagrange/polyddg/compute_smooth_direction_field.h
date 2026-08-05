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
#include <optional>
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

    /// Controls where the output direction field is stored.
    ///
    /// - @c AttributeElement::Vertex (default): stores a per-vertex 3-D tangent vector
    ///   attribute using the vertex-based connection Laplacian (Knöppel et al. 2013).
    /// - @c AttributeElement::Facet: stores a per-facet 3-D tangent vector attribute using
    ///   the face-based connection Laplacian.
    AttributeElement output_element_type = AttributeElement::Vertex;

    /// Stabilization weight for the VEM projection term in the connection Laplacian
    /// (vertex-based path only).
    double lambda = 1.0;

    /// Name of an alignment constraint attribute used as soft constraints.
    /// Each non-zero entry is softly constrained to align to that direction; zero entries are
    /// unconstrained. If empty (the default), no constraints are applied and the globally
    /// smoothest field is computed.
    ///
    /// Must match the element type selected by @c output_element_type:
    /// - @c AttributeElement::Vertex: a per-vertex 3-D tangent vector attribute
    ///   (AttributeElement::Vertex, AttributeUsage::Vector, 3 channels).
    /// - @c AttributeElement::Facet: a per-facet 3-D tangent vector attribute
    ///   (AttributeElement::Facet, AttributeUsage::Vector, 3 channels).
    std::string_view alignment_attribute = "";

    /// Output attribute name for the smooth direction field. If not set (std::nullopt, the
    /// default), the canonical name depends on @c output_element_type:
    ///
    /// - @c AttributeElement::Vertex: @c \@smooth_direction_field
    /// - @c AttributeElement::Facet: @c \@smooth_direction_field_facets
    std::optional<std::string_view> direction_field_attribute;

    /// Alignment tradeoff parameter λ_t balancing smoothness against alignment strength in the
    /// constrained solve (L_reg − λ_t M) u = M q (Knöppel et al. 2013, Eq. 16 / Algorithm 3).
    /// Only used when @c alignment_attribute is set.
    ///
    /// - @c 0 (default): the paper's recommended balanced value.
    /// - Negative (toward −∞): stronger alignment, weaker smoothness.
    /// - Positive (toward λ₁, the smallest generalized eigenvalue): weaker alignment, smoother
    ///   field. Must stay strictly below λ₁; values at or above it make the system indefinite
    ///   and the solve fails.
    double alignment_lambda = 0.0;
};

///
/// Compute the globally smoothest n-direction field on a surface mesh.
///
/// Dispatches to a vertex-based or facet-based implementation depending on
/// @c options.output_element_type:
///
/// - @c AttributeElement::Vertex (default): solves the vertex-based connection Laplacian
///   (Knöppel et al., "Globally optimal direction fields", ACM ToG 32(4), 2013).  The result
///   is stored as a per-vertex 3-D tangent vector attribute.
/// - @c AttributeElement::Facet: solves the face-based connection Laplacian, minimizing
///   @f$ E(u) = \sum_{e=(f,g)} w_e \| R_{f \to g}^n u_f - u_g \|^2 @f$.  The result is
///   stored as a per-facet 3-D tangent vector attribute.
///
/// @param[in,out] mesh    Input surface mesh. The output attribute is added or overwritten.
/// @param[in]     ops     Precomputed differential operators for the mesh.
/// @param[in]     options Options controlling the rosy order, output element type,
///                        stabilization weight, optional alignment constraints, and output
///                        attribute name.
///
/// @return Attribute ID of the output direction field attribute.
///
template <typename Scalar, typename Index>
LA_POLYDDG_API AttributeId compute_smooth_direction_field(
    SurfaceMesh<Scalar, Index>& mesh,
    const DifferentialOperators<Scalar, Index>& ops,
    SmoothDirectionFieldOptions options = {});

///
/// Convenience overload that constructs a DifferentialOperators object internally.
///
template <typename Scalar, typename Index>
LA_POLYDDG_API AttributeId compute_smooth_direction_field(
    SurfaceMesh<Scalar, Index>& mesh,
    SmoothDirectionFieldOptions options = {});

/// @}

} // namespace lagrange::polyddg
