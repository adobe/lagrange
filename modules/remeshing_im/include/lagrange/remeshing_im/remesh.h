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

namespace lagrange::remeshing_im {

///
/// Positional symmetry type.
///
enum class PosyType : uint8_t { Triangle = 3, Quad = 4 };

///
/// Rotational symmetry type.
///
enum class RosyType : uint8_t { Line = 2, Cross = 4, Hex = 6 };

///
/// Options for the remeshing process.
///
struct RemeshingOptions
{
    /// Target number of facets in the remeshed output.
    size_t target_num_facets = 1000;

    /// If true, the remeshing process will be deterministic.
    bool deterministic = false;

    /// Number of nearest neighbors to use when processing point clouds.
    size_t knn_points = 1000;

    /// Positional symmetry type. Options: Triangle (3), Quad (4)
    PosyType posy = PosyType::Quad;

    /// Rotational symmetry type. Options: Line (2), Cross (4), Hex (6)
    RosyType rosy = RosyType::Cross;

    /// Crease angle in degrees. Edges with dihedral angle above this value will be treated as
    /// creases.
    float crease_angle = 0.f;

    /// Whether to align the remeshed output to the input mesh boundaries.
    bool align_to_boundaries = false;

    /// Use extrinsic metrics when aligning cross field.
    bool extrinsic = true;

    /// Number of smoothing iterations.
    size_t num_smooth_iter = 0;
};

///
/// Remeshes the input surface mesh using the Instant Meshes algorithm.
///
/// \tparam Scalar Scalar type of the mesh geometry.
/// \tparam Index  Index type of the mesh connectivity.
///
/// \param mesh    Input surface mesh to be remeshed. This mesh may be modified in-place during
///                the remeshing process (for example, to compute auxiliary attributes or perform
///                preprocessing steps).
/// \param options Options controlling the remeshing process.
///
/// \return A new surface mesh containing the remeshed output.
///
template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> remesh(
    SurfaceMesh<Scalar, Index>& mesh,
    const RemeshingOptions& options = {});

} // namespace lagrange::remeshing_im
