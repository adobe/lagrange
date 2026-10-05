/*
 * Copyright 2025 Adobe. All rights reserved.
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
#include <lagrange/image/Array3D.h>
#include <lagrange/image/View3D.h>

#include <optional>
#include <string_view>
#include <utility>

namespace lagrange::texproc {

/// @addtogroup module-texproc
/// @{

///
/// How per-view gradient contributions are normalized when assembling the edge-difference target.
///
enum class GradientNormalization {
    /// Per-edge weighted average of per-view gradients:
    ///     edge_target = sum_i(grad_i * w_i(n1) * w_i(n2)) / sum_i(w_i(n1) * w_i(n2))
    /// Matches the ShapeGradientDomain reference. Produces gradient targets bounded by the range
    /// of per-view gradients (no overshoot from the gradient term alone).
    PerEdge,

    /// Per-texel normalization with geometric-mean combination (legacy behavior):
    ///     edge_target = sum_i(grad_i * sqrt(w_i(n1)/Σw(n1) * w_i(n2)/Σw(n2)))
    /// The gradient weights do not sum to 1 per edge at view boundaries, which attenuates
    /// gradient targets in transition regions.
    PerTexelSqrt,
};

///
/// Options for texture compositing.
///
struct CompositingOptions
{
    /// The weight for fitting the values of the signal.
    double value_weight = 1e3;

    /// The number of quadrature samples to use for integration (in {1, 3, 6, 12, 24, 32}).
    unsigned int quadrature_samples = 6;

    /// Jitter amount per texel (0 to deactivate).
    double jitter_epsilon = 1e-4;

    /// Clamp out-of-range texels to the given range (nullopt to disable).
    std::optional<std::pair<double, double>> clamp_to_range = std::nullopt;

    /// How per-view gradient contributions are normalized into the edge-difference target.
    GradientNormalization gradient_normalization = GradientNormalization::PerTexelSqrt;

    /// Use a direct (LDLT) solver instead of the multigrid v-cycle.
    /// The direct solver is exact, but uses more memory.
    /// The multigrid solver is approximate, uses less memory and runs faster.
    bool use_direct_solver = false;

    /// Weight for combinatorial Laplacian regularization added to the stiffness matrix.
    /// Helps stabilize the system in poorly conditioned regions.
    /// Set to 0 to disable.
    double stiffness_regularization_weight = 1e-9;

    /// When using GradientNormalization::PerTexelSqrt, whether to smooth pixels with a low total
    /// weight (< 1). When enabled, this will dampen the gradient terms for pixels with a low
    /// total weight, resulting in a smoother texture in low-confidence areas.
    bool smooth_low_weight_areas = false;

    /// Whether to run solver sanity checks. If unset, defaults to true in debug builds and false in
    /// release builds.
    std::optional<bool> sanity_check = std::nullopt;

    /// Multigrid solver options (ignored when use_direct_solver is true).
    struct SolverOptions
    {
        /// Number of multigrid levels.
        unsigned int num_multigrid_levels = 4;

        /// Number of Gauss-Seidel iterations per multigrid level.
        unsigned int num_gauss_seidel_iterations = 2;

        /// Number of V-cycles to perform.
        unsigned int num_v_cycles = 4;
    } solver;
};

///
/// A view of a texture with weights associated with each texel.
///
template <typename ValueType>
struct ConstWeightedTextureView
{
    /// Texture data for a specific view.
    image::experimental::View3D<const ValueType> texture;

    /// Confidence weights for each texel. 0 means the texel should be ignored, 1 means the texel
    /// should be fully trusted.
    image::experimental::View3D<const float> weights;
};

///
/// Composite multiple textures into a single texture.
///
/// @param[in]  mesh       Input mesh with UV attributes.
/// @param[in]  textures   Textures to composite. Input textures must have the same dimensions.
/// @param[in]  options    Compositing options.
///
/// @tparam     Scalar     Mesh scalar type.
/// @tparam     Index      Mesh index type.
/// @tparam     ValueType  Texture value type.
///
/// @return     Texture image resulting from the compositing.
///
template <typename Scalar, typename Index, typename ValueType>
image::experimental::Array3D<ValueType> texture_compositing(
    const SurfaceMesh<Scalar, Index>& mesh,
    std::vector<ConstWeightedTextureView<ValueType>> textures,
    const CompositingOptions& options = {});

/// @}

} // namespace lagrange::texproc
