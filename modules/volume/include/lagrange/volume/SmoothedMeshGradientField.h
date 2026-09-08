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
#include <lagrange/volume/api.h>

#include <Eigen/Core>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <openvdb/openvdb.h>
#include <openvdb/tools/Interpolation.h>
#include <lagrange/utils/warnon.h>
// clang-format on

namespace lagrange::volume {

///
/// A world-space direction field obtained by Gaussian-filtering a winding-number signed distance
/// field and differentiating the filtered scalar field.
///
class LA_VOLUME_API SmoothedMeshGradientField
{
public:
    using Grid = openvdb::Vec3SGrid;
    using Accessor = Grid::ConstAccessor;

    ///
    /// Construct a smoothed gradient field for a triangle surface mesh.
    ///
    /// @param[in] mesh         Input mesh. Its winding-number sign defines the SDF interior.
    /// @param[in] voxel_size   Grid voxel size in world units; must be positive.
    /// @param[in] epsilon      Gaussian smoothing scale in world units; must be positive.
    /// @param[in] query_reach  Maximum expected query distance from the surface in world units.
    ///
    /// @tparam Scalar Mesh scalar type.
    /// @tparam Index  Mesh index type.
    ///
    template <typename Scalar, typename Index>
    SmoothedMeshGradientField(
        const SurfaceMesh<Scalar, Index>& mesh,
        double voxel_size,
        double epsilon,
        double query_reach);

    /// Return a read-only shared handle to the underlying OpenVDB grid.
    Grid::ConstPtr grid() const { return m_grid; }

    /// Return a read-only accessor to the underlying OpenVDB grid.
    Accessor accessor() const { return m_grid->getConstAccessor(); }

    ///
    /// Sample and normalize the gradient at a world-space point.
    ///
    /// @param[in]  position       Query position.
    /// @param[out] direction_out  Outward unit direction when sampling succeeds.
    /// @param[in]  accessor       Optional grid accessor to cache repeated queries (one per thread).
    ///
    /// @return True for a finite, non-degenerate in-band gradient; false when the caller should
    ///         use an analytic fallback.
    ///
    bool sample_direction(
        const Eigen::Vector3d& position,
        Eigen::Vector3d& direction_out,
        Accessor* accessor = nullptr) const;

private:
    // The grid is never mutated after construction.
    Grid::Ptr m_grid;
};

} // namespace lagrange::volume
