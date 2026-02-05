#pragma once

#include <lagrange/Logger.h>
#include <lagrange/utils/assert.h>
#include <lagrange/volume/types.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <polyscope/polyscope.h>
#include <polyscope/volume_grid.h>
#include <openvdb/tools/FastSweeping.h>
#include <openvdb/tools/Dense.h>
#include <openvdb/tools/Interpolation.h>
#include <tbb/parallel_for.h>
#include <tbb/blocked_range3d.h>
#include <tbb/enumerable_thread_specific.h>
#include <lagrange/utils/warnon.h>
// clang-format on

using FloatGrid = lagrange::volume::Grid<float>;

void register_grid(std::string_view name, const FloatGrid& grid)
{
    auto bbox_index = grid.evalActiveVoxelBoundingBox();
    la_runtime_assert(!bbox_index.empty(), "Grid has no active voxels.");
    auto bbox_world = grid.transform().indexToWorld(bbox_index);

    uint32_t dimX = bbox_index.dim().x();
    uint32_t dimY = bbox_index.dim().y();
    uint32_t dimZ = bbox_index.dim().z();
    glm::vec3 bbox_min(
        static_cast<float>(bbox_world.min().x()),
        static_cast<float>(bbox_world.min().y()),
        static_cast<float>(bbox_world.min().z()));
    glm::vec3 bbox_max(
        static_cast<float>(bbox_world.max().x()),
        static_cast<float>(bbox_world.max().y()),
        static_cast<float>(bbox_world.max().z()));

    // register the grid
    polyscope::VolumeGrid* ps_grid =
        polyscope::registerVolumeGrid(std::string(name), {dimX, dimY, dimZ}, bbox_min, bbox_max);

    // add a scalar function on the grid
    uint32_t num_voxels = dimX * dimY * dimZ;
    std::vector<float> values(num_voxels, 0.f);
    auto o = bbox_index.min();
    struct Data
    {
        FloatGrid::ConstAccessor accessor;
    };
    tbb::enumerable_thread_specific<Data> data([&]() { return Data{grid.getConstAccessor()}; });
    using Range3d = tbb::blocked_range3d<int32_t>;
    const Range3d voxel_range(0, dimZ, 0, dimY, 0, dimX);
    tbb::parallel_for(voxel_range, [&](const Range3d& range) {
        auto rz = range.pages();
        auto ry = range.rows();
        auto rx = range.cols();
        auto& accessor = data.local().accessor;
        for (int32_t k = rz.begin(); k != rz.end(); ++k) {
            for (int32_t j = ry.begin(); j != ry.end(); ++j) {
                for (int32_t i = rx.begin(); i != rx.end(); ++i) {
                    openvdb::Vec3R ijk_is(i + o.x(), j + o.y(), k + o.z());
                    float value = openvdb::tools::BoxSampler::sample(accessor, ijk_is);
                    values[k * dimY * dimX + j * dimX + i] = value;
                }
            }
        }
    });
    lagrange::logger().info(
        "Registered {} voxels. Min corner: {}, {}, {}",
        num_voxels,
        bbox_min.x,
        bbox_min.y,
        bbox_min.z);

    polyscope::VolumeGridNodeScalarQuantity* ps_scalars =
        ps_grid->addNodeScalarQuantity("values", std::make_tuple(values.data(), num_voxels));
    ps_scalars->setEnabled(true);
}
