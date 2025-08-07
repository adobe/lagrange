/*
 * Copyright 2023 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/volume/sample_vertex_normal.h>

#include <lagrange/Attribute.h>
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/internal/find_attribute_utils.h>
#include <lagrange/views.h>
#include <lagrange/volume/GridTypes.h>

#include <openvdb/tools/GridOperators.h>
#include <openvdb/tools/Interpolation.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <tbb/parallel_for.h>
#include <tbb/enumerable_thread_specific.h>
#include <lagrange/utils/warnon.h>
// clang-format on

namespace lagrange::volume {

template <typename Scalar, typename Index, typename GridScalar>
void sample_vertex_normal(
    SurfaceMesh<Scalar, Index>& mesh,
    const Grid<GridScalar>& grid,
    const NormalsFromVolumeOptions& options)
{
    openvdb::initialize();

    // allocate attribute for normals
    auto normals_id = internal::find_or_create_attribute<Scalar>(
        mesh,
        options.normal_attribute_name,
        lagrange::AttributeElement::Vertex,
        lagrange::AttributeUsage::Normal,
        3,
        internal::ResetToDefault::No);
    auto normals_view = mesh.template ref_attribute<Scalar>(normals_id).ref_all();

    // compute normals
    using NormalGridType =
        typename openvdb::tools::ScalarToVectorConverter<volume::Grid<GridScalar>>::Type;
    typename NormalGridType::Ptr gradient_grid = openvdb::tools::gradient(grid);

    // Create thread-local data with an accessor and a sampler. Because a value accessor employs
    // a cache for efficient access to grid data, it is important to use a different
    // accessor/sampler per thread.
    struct LocalData
    {
        LocalData(const NormalGridType& grid)
            : accessor(grid.getConstAccessor())
            , sampler(accessor, grid.transform())
        {}

        typename NormalGridType::ConstAccessor accessor;
        openvdb::tools::
            GridSampler<typename NormalGridType::ConstAccessor, openvdb::tools::BoxSampler>
                sampler;
    };
    tbb::enumerable_thread_specific<LocalData> accessors(
        [&gradient_grid]() { return LocalData(*gradient_grid); });

    auto vertices = vertex_view(mesh);

    tbb::parallel_for(
        tbb::blocked_range<size_t>(0, mesh.get_num_vertices()),
        [&](const tbb::blocked_range<size_t>& r) {
            auto& sampler = accessors.local().sampler;

            for (size_t i = r.begin(); i != r.end(); ++i) {
                auto p = openvdb::Vec3d(vertices(i, 0), vertices(i, 1), vertices(i, 2));
                auto n = sampler.wsSample(p);
                n.normalize();
                std::copy_n(n.asV(), 3, normals_view.data() + i * 3);
            }
        });
}

#define LA_X_sample_vertex_normal(GridScalar, Scalar, Index)       \
    template void sample_vertex_normal<Scalar, Index, GridScalar>( \
        SurfaceMesh<Scalar, Index> & mesh,                         \
        const Grid<GridScalar>& grid,                              \
        const NormalsFromVolumeOptions& options);
#define LA_X_sample_vertex_normal_aux(_, GridScalar) \
    LA_SURFACE_MESH_X(sample_vertex_normal, GridScalar)
LA_VOLUME_GRID_X(sample_vertex_normal_aux, 0)

} // namespace lagrange::volume
