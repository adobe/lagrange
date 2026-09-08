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
#include <lagrange/volume/SmoothedMeshGradientField.h>

#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/utils/assert.h>
#include <lagrange/volume/mesh_to_volume.h>

#include <algorithm>
#include <cmath>
#include <limits>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <openvdb/tools/Filter.h>
#include <openvdb/tools/GridOperators.h>
#include <openvdb/tools/Interpolation.h>
#include <lagrange/utils/warnon.h>
// clang-format on

namespace lagrange::volume {

template <typename Scalar, typename Index>
SmoothedMeshGradientField::SmoothedMeshGradientField(
    const SurfaceMesh<Scalar, Index>& mesh,
    double voxel_size,
    double epsilon,
    double query_reach)
{
    la_runtime_assert(voxel_size > 0, "voxel_size must be positive");
    la_runtime_assert(epsilon > 0, "epsilon must be positive");
    la_runtime_assert(query_reach >= 0, "query_reach must be nonnegative");

    openvdb::initialize();
    const int filter_width = std::max(1, static_cast<int>(std::round(epsilon / voxel_size)));

    // - Gaussian filtering performs four separable box-filters (4*w).
    // - Central-difference gradient stencil requires one extra voxel on one axis.
    // - Trilinear interpolation requires one extra voxel along each axis.
    //
    // Therefore, a conservative bandwidth estimate would be B_ext = B + (4*w + 2)*√3
    //
    // A tighter bound could use ‖(4w+2, 4w+1, 4w+1)‖, since the gradient stencil is only applied
    // along one axis. For simplicity, and because axis-aligned correctness is enough here, we drop
    // the *√3 factor and just add 4w+2.
    const float band = std::ceil(query_reach / voxel_size) + 4.0f * filter_width + 2.0f;

    MeshToVolumeOptions options;
    options.voxel_size = voxel_size;
    options.signing_method = MeshToVolumeOptions::Sign::WindingNumber;
    options.exterior_bandwidth = band;
    options.interior_bandwidth = band;
    auto sdf = mesh_to_volume<float>(mesh, options);

    openvdb::tools::Filter<openvdb::FloatGrid> filter(*sdf);
    filter.gaussian(filter_width, 1);
    m_grid = openvdb::tools::gradient(*sdf);

    // TODO: Consider masking & pruning the grid back to the desired narrow-band if storage is a
    // concern.
}

bool SmoothedMeshGradientField::sample_direction(
    const Eigen::Vector3d& position,
    Eigen::Vector3d& direction_out,
    Accessor* accessor_ptr) const
{
    using Sampler = openvdb::tools::GridSampler<Grid::ConstAccessor, openvdb::tools::BoxSampler>;

    const Accessor& accessor = accessor_ptr ? *accessor_ptr : m_grid->getConstAccessor();
    const openvdb::Vec3d world_position(position.x(), position.y(), position.z());
    const openvdb::Coord base = openvdb::Coord::floor(m_grid->worldToIndex(world_position));
    for (int x = 0; x < 2; ++x) {
        for (int y = 0; y < 2; ++y) {
            for (int z = 0; z < 2; ++z) {
                const openvdb::Coord corner(base.x() + x, base.y() + y, base.z() + z);
                if (!accessor.isValueOn(corner)) return false;
            }
        }
    }

    const Sampler sampler(accessor, m_grid->transform());
    const openvdb::Vec3s gradient = sampler.wsSample(world_position);
    const Eigen::Vector3d direction(gradient.x(), gradient.y(), gradient.z());
    const double norm = direction.norm();
    // Vec3SGrid stores floats; values at float numerical zero cannot define a direction.
    if (!std::isfinite(norm) || norm <= std::numeric_limits<float>::epsilon()) {
        return false;
    }
    direction_out = direction / norm;
    return direction_out.allFinite();
}

#define LA_X_construct(_, Scalar, Index)                           \
    template SmoothedMeshGradientField::SmoothedMeshGradientField( \
        const SurfaceMesh<Scalar, Index>&,                         \
        double,                                                    \
        double,                                                    \
        double);
LA_SURFACE_MESH_X(construct, 0)

} // namespace lagrange::volume
