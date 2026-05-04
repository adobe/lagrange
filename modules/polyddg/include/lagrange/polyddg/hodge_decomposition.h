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
/// Options for Hodge decomposition functions.
///
/// Depending on which function is passed this argument, the attributes are interpreted differently:
/// - For hodge_decomposition_1_form(): input/output are per-edge scalars (1-forms)
/// - For hodge_decomposition_vector_field(): input/output are per-vertex 3D vectors (vector fields)
///
struct HodgeDecompositionOptions
{
    /// Stabilization weight @f$ \lambda @f$ for the VEM 1-form inner product.
    double lambda = 1.0;

    /// N-rosy symmetry order. Only used by hodge_decomposition_vector_field().
    /// When n = 1 (default) the input is a plain vector field.
    /// When n > 1 the input is one representative vector of an n-rosy field. The vector is
    /// encoded in the local tangent plane via n-fold angle multiplication before the 1-form
    /// decomposition and decoded back afterwards.
    uint8_t nrosy = 1;

    /// Input attribute name.
    /// - For hodge_decomposition_1_form(): per-edge scalar (1-form)
    /// - For hodge_decomposition_vector_field(): per-vertex 3D vector (global coordinates)
    std::string_view input_attribute = "@hodge_input";

    /// Output attribute name for the exact component.
    /// - For hodge_decomposition_1_form(): per-edge scalar
    /// - For hodge_decomposition_vector_field(): per-vertex 3D vector (global coordinates)
    std::string_view exact_attribute = "@hodge_exact";

    /// Output attribute name for the co-exact component.
    /// - For hodge_decomposition_1_form(): per-edge scalar
    /// - For hodge_decomposition_vector_field(): per-vertex 3D vector (global coordinates)
    std::string_view coexact_attribute = "@hodge_coexact";

    /// Output attribute name for the harmonic component.
    /// - For hodge_decomposition_1_form(): per-edge scalar
    /// - For hodge_decomposition_vector_field(): per-vertex 3D vector (global coordinates)
    std::string_view harmonic_attribute = "@hodge_harmonic";
};

///
/// Result of Hodge decomposition functions.
///
/// Depending on which function returned this result, the attribute IDs refer to different types:
/// - For hodge_decomposition_1_form(): per-edge scalar attributes
/// - For hodge_decomposition_vector_field(): per-vertex 3D vector attributes
///
struct HodgeDecompositionResult
{
    /// Attribute ID of the exact component.
    AttributeId exact_id = invalid_attribute_id();

    /// Attribute ID of the co-exact component.
    AttributeId coexact_id = invalid_attribute_id();

    /// Attribute ID of the harmonic component.
    AttributeId harmonic_id = invalid_attribute_id();
};

// ---- 1-form level -----------------------------------------------------------

///
/// Compute the Helmholtz-Hodge decomposition of a 1-form on a closed surface mesh.
///
/// Takes a discrete 1-form (scalar per edge) and decomposes it into three orthogonal components:
///
/// @f[
///   \omega = \omega_{\text{exact}} + \omega_{\text{coexact}} + \omega_{\text{harmonic}}
/// @f]
///
/// where:
///   - @f$ \omega_{\text{exact}} = d_0 \alpha @f$ is the **exact** (curl-free) part,
///   - @f$ \omega_{\text{coexact}} @f$ is the **co-exact** (divergence-free) part,
///   - @f$ \omega_{\text{harmonic}} @f$ is the **harmonic** part (zero for genus-0 surfaces).
///
/// The exact part is obtained by solving the scalar Laplacian @f$ L_0 \alpha = \delta_1 \omega @f$.
/// The co-exact part is obtained by solving a saddle-point system that minimizes the @f$ M_1 @f$-norm
/// subject to the constraint @f$ d_1 \omega_{\text{coexact}} = d_1 \omega @f$.
/// The harmonic part is the residual.
///
/// @param[in,out] mesh    Input surface mesh. Must be closed with a single connected component.
///                        The input edge attribute must already exist.
/// @param[in]     ops     Precomputed differential operators for the mesh.
/// @param[in]     options Options. The nrosy field is ignored for 1-form decomposition.
///
/// @return Attribute IDs of the three output per-edge scalar attributes.
///
template <typename Scalar, typename Index>
LA_POLYDDG_API HodgeDecompositionResult hodge_decomposition_1_form(
    SurfaceMesh<Scalar, Index>& mesh,
    const DifferentialOperators<Scalar, Index>& ops,
    HodgeDecompositionOptions options = {});

///
/// Convenience overload that constructs a DifferentialOperators object internally.
///
template <typename Scalar, typename Index>
LA_POLYDDG_API HodgeDecompositionResult hodge_decomposition_1_form(
    SurfaceMesh<Scalar, Index>& mesh,
    HodgeDecompositionOptions options = {});

// ---- Per-vertex vector field level ------------------------------------------

///
/// Compute the Helmholtz-Hodge decomposition of a per-vertex vector field on a surface mesh.
///
/// Takes a per-vertex vector field in global 3D coordinates and decomposes it into three orthogonal
/// components, each stored as a per-vertex vector field in global coordinates:
///
/// @f[
///   V = V_{\text{exact}} + V_{\text{coexact}} + V_{\text{harmonic}}
/// @f]
///
/// Internally, the per-vertex vector field is converted to a 1-form (scalar per edge) using
/// midpoint integration along edges: @f$ \omega_e = \frac{V_i + V_j}{2} \cdot (x_j - x_i) @f$.
/// Any normal component of the input vectors is automatically annihilated by the dot product
/// with the edge vector, so explicit tangent-plane projection is not needed for n=1.
/// The 1-form is then decomposed using hodge_decomposition_1_form(), and each
/// component is converted back to a per-vertex vector field using the discrete sharp operator
/// followed by area-weighted averaging from faces to vertices.
///
/// When the n-rosy option is greater than 1, each input vector is first projected into the
/// local vertex tangent plane and its angle is multiplied by n (encoding), converting the
/// n-rosy representative into a regular vector field. After decomposition, each output
/// component is decoded by dividing the tangent-plane angle by n.
///
/// @param[in,out] mesh    Input surface mesh. The input attribute (per-vertex 3D vector) must
///                        already exist. Output attributes are created or overwritten.
/// @param[in]     ops     Precomputed differential operators for the mesh.
/// @param[in]     options Options.
///
/// @return Attribute IDs of the three output per-vertex vector attributes.
///
template <typename Scalar, typename Index>
LA_POLYDDG_API HodgeDecompositionResult hodge_decomposition_vector_field(
    SurfaceMesh<Scalar, Index>& mesh,
    const DifferentialOperators<Scalar, Index>& ops,
    HodgeDecompositionOptions options = {});

///
/// Convenience overload that constructs a DifferentialOperators object internally.
///
template <typename Scalar, typename Index>
LA_POLYDDG_API HodgeDecompositionResult hodge_decomposition_vector_field(
    SurfaceMesh<Scalar, Index>& mesh,
    HodgeDecompositionOptions options = {});

/// @}

} // namespace lagrange::polyddg
