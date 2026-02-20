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

#include <lagrange/volume/types.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <openvdb/tools/FastSweeping.h>
#include <openvdb/tools/GridTransformer.h>
#include <openvdb/tools/LevelSetFilter.h>
#include <openvdb/tools/Dense.h>
#include <lagrange/utils/warnon.h>
// clang-format on

namespace lagrange::volume::internal {

template <typename GridScalar>
typename Grid<GridScalar>::Ptr resample_grid(const Grid<GridScalar>& grid, double voxel_size)
{
    if (voxel_size <= 0.0) {
        auto vs = grid.voxelSize();
        voxel_size = (vs.x() + vs.y() + vs.z()) / 3.0 * (-voxel_size);
    }
    logger().info("Resampling grid to voxel size {}", voxel_size);
    auto dest = Grid<GridScalar>::create();

    const openvdb::Vec3d offset(voxel_size / 2.0, voxel_size / 2.0, voxel_size / 2.0);
    auto transform = openvdb::math::Transform::createLinearTransform(voxel_size);
    transform->postTranslate(offset);

    dest->setTransform(transform);
    openvdb::tools::resampleToMatch<openvdb::tools::BoxSampler>(grid, *dest);

    return dest;
}

template <typename GridScalar>
typename Grid<GridScalar>::Ptr densify_grid(const Grid<GridScalar>& grid)
{
    openvdb::tools::Dense<GridScalar> dense(grid.evalActiveVoxelBoundingBox());
    openvdb::tools::copyToDense(grid, dense);
    auto tmp = grid.deepCopy();
    openvdb::tools::copyFromDense(dense, *tmp, -1);
    return tmp;
}

template <typename GridScalar>
typename Grid<GridScalar>::Ptr redistance_grid(const Grid<GridScalar>& grid)
{
    return openvdb::tools::sdfToSdf(grid);
}

template <typename GridScalar>
void offset_grid_in_place(Grid<GridScalar>& grid, double offset_radius, bool relative)
{
    if (grid.getGridClass() != openvdb::GRID_LEVEL_SET) {
        logger().warn("Offset can only be applied to signed distance fields.");
    } else {
        if (relative) {
            auto vs = grid.voxelSize();
            double voxel_size = (vs.x() + vs.y() + vs.z()) / 3.0;
            offset_radius = offset_radius * voxel_size;
        }
        openvdb::tools::LevelSetFilter filter(grid);
        filter.offset(offset_radius);
        logger().trace("Number of active voxels after offset: {}", grid.activeVoxelCount());
        filter.prune();
        logger().trace("Number of active voxels after pruning: {}", grid.activeVoxelCount());
    }
}

} // namespace lagrange::volume::internal
