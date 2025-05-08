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

#include <string_view>

namespace lagrange::filtering {

/// @addtogroup module-filtering
/// @{

///
/// Option struct for anisotropic mesh smoothing.
///
struct SmoothingOptions
{
    ///
    /// The type of smoothing to be performed
    ///
    enum struct FilterMethod {
        ///
        /// Directly process the vertex positions
        ///
        /// * + No fold-overs
        /// * - In the context of smoothing this is a shrinking flow
        ///
        VertexSmoothing,

        ///
        /// Process the normals and then fit the vertices to the normals
        ///
        /// * + Avoids shrinking
        /// * - When the normals are filtered aggressively, it could lead to fold-over
        ///
        NormalSmoothing
    };

    ///
    /// Type of smoothing to be performed
    ///
    FilterMethod filter_method = FilterMethod::NormalSmoothing;

    ///
    /// @name Phase 1: Metric Modification
    ///
    /// Optionally, the metric is modified to scale distances across sharp features so that
    /// diffusion is "slower" across sharp features
    ///
    /// @{

    /// The curvature/inhomogeneity weight: Specifies the extent to which total curvature should be
    /// used to change the underlying metric. (Setting =0 is equivalent to using standard
    /// homogeneous/anisotropic diffusion.)
    double curvature_weight = 0.02;

    /// The normal smoothing weight: Specifies the extent to which normals should be diffused before
    /// curvature is estimated. Formally, this is the time-step for heat-diffusion performed on the
    /// normals
    ///
    /// (Setting =0 will reproduce the original normals.)
    double normal_smoothing_weight = 1e-4;

    ///
    /// @}
    ///
    /// @name Phase 2: Normal Modification
    ///
    /// Target normals are computed by solving for new normals balancing two objectives:
    /// 1. New normal values should be close to the old normal values.
    /// 2. New normal gradients should be close the scaled gradients to the old normals.
    ///
    /// @{

    /// Gradient fitting weight: Specifies the importance of matching the gradient constraints
    /// (objective #2) relative to matching the positional constraints (objective #1).
    ///
    /// (Setting =0 reproduces the original normals.)
    double gradient_weight = 1e-4;

    ///
    /// Gradient modulation scale: Prescribes the scale factor relating the gradients of the source
    /// to those of the target.
    ///
    /// - <1 => gradients are dampened => smoothing
    /// - >1 => gradients are amplified => sharpening
    ///
    /// (Setting =0 is equivalent to performing a semi-implicit step of heat-diffusion, with
    /// time-step equal to gradient_weight.)
    ///
    /// (Setting =1 reproduces the original normals.)
    double gradient_modulation_scale = 0.;

    ///
    /// @}
    /// @name Phase 3: Geometry Fitting
    ///
    /// Given target per-triangle normals, vertex positions are computed by solving for new
    /// positions balancing two objectives:
    ///
    /// 1. New vertex positions should be close to the old vertex positions.
    /// 2. Triangles defined by the new positions should be perpendicular to the target normals.
    ///
    /// @{

    /// Weight for fitting the surface to prescribed normals. Specifies the importance of matching
    /// the target normals (objective #2) relative to matching the original positions (objective
    /// #1).
    ///
    /// (Setting =0 will reproduce the original geometry.)
    double normal_projection_weight = 1e2;

    /// @}
};

///
/// Perform anisotropic mesh smoothing.
///
/// * The mesh need not be manifold.
/// * The mesh can have self-intersections
/// * The mesh can be disconnected
/// * The mesh can have boundaries
/// * The mesh should not have degenerate (i.e. zero-area) triangles. (Strictly speaking, it should
///   not have vertices all of whose incident triangles are degenerate.)
///
/// @param[in,out] mesh     Triangle mesh to smooth.
/// @param[in]     options  Smoothing options.
///
/// @tparam        Scalar   Mesh scalar type.
/// @tparam        Index    Mesh index type.
///
template <typename Scalar, typename Index>
void mesh_smoothing(SurfaceMesh<Scalar, Index>& mesh, const SmoothingOptions& options = {});

/// @}

} // namespace lagrange::filtering
