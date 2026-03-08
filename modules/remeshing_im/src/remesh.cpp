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

#include <lagrange/remeshing_im/remesh.h>

#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/compute_facet_normal.h>
#include <lagrange/eigen_convert.h>
#include <lagrange/remeshing_im/api.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/warning.h>
#include <lagrange/views.h>
#include <lagrange/utils/scope_guard.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <bvh.h>
#include <common.h>
#include <dedge.h>
#include <extract.h>
#include <field.h>
#include <hierarchy.h>
#include <meshstats.h>
#include <normal.h>
#include <subdivide.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <map>
#include <memory>
#include <set>

namespace lagrange::remeshing_im {

template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> remesh(SurfaceMesh<Scalar, Index>& mesh, const RemeshingOptions& options)
{
    using namespace ::instant_meshes;

    la_runtime_assert(mesh.is_triangle_mesh(), "Input mesh must be triangular.");
    const auto rosy = static_cast<uint8_t>(options.rosy);
    const auto posy = static_cast<uint8_t>(options.posy);
    la_runtime_assert(
        rosy == 2 || rosy == 4 || rosy == 6,
        "Only 2-RoSy, 4-RoSy and 6-rosy fields are supported.");
    la_runtime_assert(posy == 3 || posy == 4, "Only 3-PoSy and 4-PoSy fields are supported.");

    MatrixXf V = vertex_view(mesh).template cast<float>().transpose();
    MatrixXu F = facet_view(mesh).template cast<uint32_t>().transpose();
    bool pointcloud = F.size() == 0;
    std::set<uint32_t> crease_in, crease_out;

    auto normal_attr_id = compute_facet_normal(mesh);
    MatrixXf N =
        attribute_matrix_view<Scalar>(mesh, normal_attr_id).template cast<float>().transpose();

    MeshStats stats = compute_mesh_stats(F, V, options.deterministic);
    AdjacencyMatrix adj = nullptr;

    std::shared_ptr<BVH> bvh;
    VectorXf A;
    if (pointcloud) {
        bvh = std::make_shared<BVH>(&F, &V, &N, stats.mAABB);
        bvh->build();
        adj = generate_adjacency_matrix_pointcloud(
            V,
            N,
            bvh.get(),
            stats,
            static_cast<uint32_t>(options.knn_points),
            options.deterministic);
        A.resize(V.cols());
        A.setConstant(1.0f);
    }

    size_t face_count = options.target_num_facets;
    float face_area = stats.mSurfaceArea / face_count;
    size_t vertex_count = posy == 4 ? face_count : (face_count / 2);
    float scale =
        posy == 4 ? std::sqrt(face_area) : (2 * std::sqrt(face_area * std::sqrt(1.f / 3.f)));

    logger().info(
        "Remeshing target: {} vertices, {} facets, {} edge length",
        vertex_count,
        face_count,
        scale);

    MultiResolutionHierarchy mRes;
    auto _ = make_scope_guard([&]() { mRes.free(); });

    if (!pointcloud) {
        /* Subdivide the mesh if necessary */
        VectorXu V2E, E2E;
        VectorXb boundary, nonManifold;
        if (stats.mMaximumEdgeLength * 2 > scale ||
            stats.mMaximumEdgeLength > stats.mAverageEdgeLength * 2) {
            logger().warn(
                "Input mesh is too coarse for the desired output edge length "
                "(max input mesh edge length="
                "{}), subdividing ..",
                stats.mMaximumEdgeLength);
            build_dedge(F, V, V2E, E2E, boundary, nonManifold);
            subdivide(
                F,
                V,
                V2E,
                E2E,
                boundary,
                nonManifold,
                std::min(scale / 2, (float)stats.mAverageEdgeLength * 2),
                options.deterministic);
        }

        /* Compute a directed edge data structure */
        build_dedge(F, V, V2E, E2E, boundary, nonManifold);

        /* Compute adjacency matrix */
        adj = generate_adjacency_matrix_uniform(F, V2E, E2E, nonManifold);

        /* Compute vertex/crease normals */
        if (options.crease_angle >= 0)
            generate_crease_normals(
                F,
                V,
                V2E,
                E2E,
                boundary,
                nonManifold,
                options.crease_angle,
                N,
                crease_in);
        else
            generate_smooth_normals(F, V, V2E, E2E, nonManifold, N);

        /* Compute dual vertex areas */
        compute_dual_vertex_areas(F, V, V2E, E2E, nonManifold, A);

        mRes.setE2E(std::move(E2E));
    }

    /* Build multi-resolution hierarrchy */
    mRes.setAdj(std::move(adj));
    mRes.setF(std::move(F));
    mRes.setV(std::move(V));
    mRes.setA(std::move(A));
    mRes.setN(std::move(N));
    mRes.setScale(scale);
    mRes.build(options.deterministic);
    mRes.resetSolution();

    if (options.align_to_boundaries && !pointcloud) {
        mRes.clearConstraints();
        uint32_t num_corners = static_cast<uint32_t>(3 * mRes.F().cols());
        for (uint32_t i = 0; i < num_corners; ++i) {
            if (mRes.E2E()[i] == INVALID) {
                uint32_t i0 = mRes.F()(i % 3, i / 3);
                uint32_t i1 = mRes.F()((i + 1) % 3, i / 3);
                LA_IGNORE_ARRAY_BOUNDS_BEGIN
                Vector3f p0 = mRes.V().col(i0), p1 = mRes.V().col(i1);
                Vector3f edge = p1 - p0;
                if (edge.squaredNorm() > 0) {
                    edge.normalize();
                    mRes.CO().col(i0) = p0;
                    mRes.CO().col(i1) = p1;
                    mRes.CQ().col(i0) = mRes.CQ().col(i1) = edge;
                    mRes.CQw()[i0] = mRes.CQw()[i1] = mRes.COw()[i0] = mRes.COw()[i1] = 1.0f;
                }
                LA_IGNORE_ARRAY_BOUNDS_END
            }
        }
        mRes.propagateConstraints(rosy, posy);
    }

    if (bvh) {
        bvh->setData(&mRes.F(), &mRes.V(), &mRes.N());
    } else if (options.num_smooth_iter > 0) {
        bvh = std::make_shared<BVH>(&mRes.F(), &mRes.V(), &mRes.N(), stats.mAABB);
        bvh->build();
    }

    Optimizer optimizer(mRes, false);
    optimizer.setRoSy(rosy);
    optimizer.setPoSy(posy);
    optimizer.setExtrinsic(options.extrinsic);

    optimizer.optimizeOrientations(-1);
    optimizer.notify();
    optimizer.wait();

    std::map<uint32_t, uint32_t> sing;
    compute_orientation_singularities(mRes, sing, options.extrinsic, rosy);

    optimizer.optimizePositions(-1);
    optimizer.notify();
    optimizer.wait();

    optimizer.shutdown();

    MatrixXf O_extr, N_extr, Nf_extr;
    std::vector<std::vector<TaggedLink>> adj_extr;
    extract_graph(
        mRes,
        options.extrinsic,
        rosy,
        posy,
        adj_extr,
        O_extr,
        N_extr,
        crease_in,
        crease_out,
        options.deterministic);

    MatrixXu F_extr;
    extract_faces(
        adj_extr,
        O_extr,
        N_extr,
        Nf_extr,
        F_extr,
        posy,
        mRes.scale(),
        crease_out,
        true,
        posy == 4,
        bvh.get(),
        static_cast<int>(options.num_smooth_iter));

    return eigen_to_surface_mesh<Scalar, Index>(O_extr.transpose(), F_extr.transpose());
}

#define LA_X_remesh(_, Scalar, Index)                                              \
    template LA_REMESHING_IM_API SurfaceMesh<Scalar, Index> remesh<Scalar, Index>( \
        SurfaceMesh<Scalar, Index>&,                                               \
        const RemeshingOptions&);
LA_SURFACE_MESH_X(remesh, 0)

} // namespace lagrange::remeshing_im
