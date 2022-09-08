/*
 * Copyright 2022 Adobe. All rights reserved.
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

#include <lagrange/MeshTrait.h>
#include <lagrange/create_mesh.h>
#include <lagrange/raycasting/create_ray_caster.h>
#include <lagrange/raycasting/project_options.h>
#include <lagrange/utils/safe_cast.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <igl/bounding_box_diagonal.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <tbb/parallel_for.h>

#include <algorithm>
#include <string>
#include <type_traits>
#include <vector>

namespace lagrange {
namespace raycasting {

///
/// Project particles (a particle contains origin and tangent info) to another mesh, by projecting
/// their positions along a prescribed direction. The returned results are particles whose transform
/// is recorded in the local coordinate.
///
/// @note          How to compute the particle transformation after the projection:
///
///                Assuming the projection transformation is P and the cumulated parents transformation
///                is C, The matrix M that transforms local to world coordinates is given by:
///
///                M = P * C
///                  = C * (C^-1 * P * C)
///
///                Where the matrix in parenthesis is the new axis system matrix after projection:
///                P' = C^-1 * P * C
///
///                We compute P * C, and then deduce P'
///
/// @param[in]     origins                  Origin of the particles.
/// @param[in]     mesh_proj_on             Mesh to be projected on.
/// @param[in]     direction                Raycasting direction to project attributes.
/// @param[out]    out_origins              Output origin of the particles.
/// @param[out]    out_normals              Output normal of the polygon at intersections.
/// @param[in]     parent_transforms        Cumulated parent transforms applied on the particles
/// @param[in,out] ray_caster               If provided, the use ray_caster to perform the queries instead.
///                                         The target mesh will assume to have been added to `ray_caster` in
///                                         advance, and this function will not try to add it. This allows to
///                                         use a different ray caster than the one computed by this
///                                         function, and allows to nest function calls.
/// @param[in]     has_normals              Whether to compute and output normals
///
/// @tparam        ParticleDataType         Particle data vector type.
/// @tparam        MeshType                 Mesh type.
/// @tparam        DefaultScalar            Scalar type used in computation.
/// @tparam        MatrixType               Matrix type for the transform.
/// @tparam        VectorType               Vector type for the direction.
///
template <
    typename ParticleDataType,
    typename MeshType,
    typename DefaultScalar = typename ParticleDataType::value_type::Scalar,
    typename MatrixType = typename Eigen::Matrix<DefaultScalar, 4, 4>,
    typename VectorType = typename Eigen::Matrix<DefaultScalar, 3, 1>>
void project_particles_directional(
    const ParticleDataType& origins,
    const MeshType& mesh_proj_on,
    const VectorType& direction,
    ParticleDataType& out_origins,
    ParticleDataType& out_normals,
    const MatrixType& parent_transforms = MatrixType::Identity(),
    EmbreeRayCaster<ScalarOf<MeshType>>* ray_caster = nullptr,
    bool has_normals = true)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Mesh type is wrong");

    // Typedef festival because templates...
    using Scalar = DefaultScalar;
    using Index = typename MeshType::Index;
    using Point = typename EmbreeRayCaster<Scalar>::Point;
    using Direction = typename EmbreeRayCaster<Scalar>::Direction;
    using Scalar4 = typename EmbreeRayCaster<Scalar>::Scalar4;
    using Index4 = typename EmbreeRayCaster<Scalar>::Index4;
    using Point4 = typename EmbreeRayCaster<Scalar>::Point4;
    using Direction4 = typename EmbreeRayCaster<Scalar>::Direction4;
    using Mask4 = typename EmbreeRayCaster<Scalar>::Mask4;

    // We need to convert to a shared_ptr AND the ray caster will make another copy of the data..
    std::unique_ptr<EmbreeRayCaster<Scalar>> engine;
    if (!ray_caster) {
        auto mesh = lagrange::to_shared_ptr(
            lagrange::create_mesh(mesh_proj_on.get_vertices(), mesh_proj_on.get_facets()));
        // Robust mode gives slightly more accurate results...
        engine = create_ray_caster<Scalar>(EMBREE_ROBUST, BUILD_QUALITY_HIGH);

        // Gosh why do I need to specify a transform here?
        engine->add_mesh(mesh, Eigen::Matrix<Scalar, 4, 4>::Identity());

        // Do a dummy raycast to trigger scene update, otherwise `cast()` will not work in
        // multithread mode... (this is why we need const-safety...)
        engine->cast(Point(0, 0, 0), Direction(0, 0, 1));
        ray_caster = engine.get();
    } else {
        logger().debug("Using provided ray-caster");
    }

    Index num_particles = safe_cast<Index>(origins.size());

    // C^-1
    MatrixType parent_transforms_inv = parent_transforms.inverse();
    bool use_parent_transforms = !parent_transforms.isIdentity();

    VectorType dir = direction.normalized();
    Index num_ray_packets = (num_particles + 3) / 4;
    Direction4 dirs;
    dirs.row(0) = dir.transpose();
    for (int i = 1; i < 4; ++i) {
        dirs.row(i) = dirs.row(0);
    }

    std::vector<uint8_t> vector_hits(num_ray_packets, safe_cast<uint8_t>(0u));

    ParticleDataType projected_origins;
    ParticleDataType projected_normals;

    out_origins.resize(num_particles);
    if (has_normals) out_normals.resize(num_particles);

    tbb::parallel_for(Index(0), num_ray_packets, [&](Index packet_index) {
        uint32_t batchsize =
            safe_cast<uint32_t>(std::min(num_particles - (packet_index << 2), safe_cast<Index>(4)));
        Mask4 mask = Mask4::Constant(-1);
        Point4 ray_origins;

        for (uint32_t b = 0; b < batchsize; ++b) {
            Index i = (packet_index << 2) + b;

            // C * L
            if (use_parent_transforms) {
                ray_origins.row(b) =
                    (parent_transforms * origins[i].homogeneous()).hnormalized().transpose();
            } else {
                ray_origins.row(b) = origins[i].transpose();            
            }
        }

        for (Index b = batchsize; b < 4; ++b) {
            mask(b) = 0;
        }

        Index4 mesh_indices;
        Index4 instance_indices;
        Index4 facet_indices;
        Scalar4 ray_depths;
        Point4 barys;
        Point4 normals;
        uint8_t hits = ray_caster->cast4(
            batchsize,
            ray_origins,
            dirs,
            mask,
            mesh_indices,
            instance_indices,
            facet_indices,
            ray_depths,
            barys,
            normals);

        if (!hits) return;

        vector_hits[packet_index] = hits;

        for (uint32_t b = 0; b < batchsize; ++b) {
            bool hit = hits & (1 << b);
            if (hit) {
                Index i = (packet_index << 2) + b;
                if (has_normals) {
                    VectorType norm =
                        (ray_caster->get_transform(mesh_indices[b], instance_indices[b])
                             .template topLeftCorner<3, 3>() *
                         normals.row(b).transpose())
                            .normalized();
                    if (use_parent_transforms) {
                        norm = parent_transforms_inv.template topLeftCorner<3, 3>() * norm;
                    }
                    out_normals[i] = norm;
                }

                VectorType old_pos = ray_origins.row(b).transpose();
                out_origins[i] = old_pos + dir * ray_depths(b);
                if (use_parent_transforms) {
                    out_origins[i] =
                        (parent_transforms_inv * out_origins[i].homogeneous()).hnormalized();
                }
            }
        }
    });

    // remove redundant output data
    Index i = 0;
    auto remove_func = [&](const VectorType&) -> bool {
        Index packet_index = i >> 2;
        Index b = i++ - (packet_index << 2);
        return !(vector_hits[packet_index] & (1 << b));
    };

    out_origins.erase(
        std::remove_if(out_origins.begin(), out_origins.end(), remove_func),
        out_origins.end());

    if (has_normals) {
        i = 0;
        out_normals.erase(
            std::remove_if(out_normals.begin(), out_normals.end(), remove_func),
            out_normals.end());
    }
}
} // namespace raycasting
} // namespace lagrange
