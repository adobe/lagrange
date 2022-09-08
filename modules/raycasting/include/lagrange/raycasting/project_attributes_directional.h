/*
 * Copyright 2020 Adobe. All rights reserved.
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
#include <lagrange/bvh/project_attributes_closest_vertex.h>
#include <lagrange/create_mesh.h>
#include <lagrange/raycasting/create_ray_caster.h>
#include <lagrange/raycasting/project_attributes_closest_point.h>
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
/// Project vertex attributes from one mesh to another, by projecting target vertices along a
/// prescribed direction, and interpolating surface values from facet corners of the source mesh.
///
/// @note          In the future may want to support using a vector field instead of a constant
///                direction for projection.
///
/// @param[in]     source          Source mesh.
/// @param[in,out] target          Target mesh to be modified.
/// @param[in]     names           Name of the vertex attributes to transfer.
/// @param[in]     direction       Raycasting direction to project attributes.
/// @param[in]     cast_mode       Whether to project forward along the ray, or to project along the
///                                whole ray (both forward and backward).
/// @param[in]     wrap_mode       Wrapping mode for values where there is no hit.
/// @param[in]     default_value   Scalar used to fill attributes in CONSTANT wrapping mode.
/// @param[in]     user_callback   Optional user callback that can be used to set attribute values
///                                depending on whether there is a hit or not.
/// @param[in,out] ray_caster      If provided, the use ray_caster to perform the queries instead.
///                                The source mesh will assume to have been added to `ray_caster` in
///                                advance, and this function will not try to add it. This allows to
///                                use a different ray caster than the one computed by this
///                                function, and allows to nest function calls.
/// @param[in]     skip_vertex     If provided, whether to skip assignment for a target vertex or
///                                not. This can be used for partial assignment (e.g. to only set
///                                boundary vertices of a mesh).
///
/// @tparam        SourceMeshType  Source mesh type.
/// @tparam        TargetMeshType  Target mesh type.
/// @tparam        DerivedVector   Vector type for the direction.
/// @tparam        DefaultScalar   Scalar type used to fill attributes.
///
template <
    typename SourceMeshType,
    typename TargetMeshType,
    typename DerivedVector,
    typename DefaultScalar = typename SourceMeshType::Scalar>
void project_attributes_directional(
    const SourceMeshType& source,
    TargetMeshType& target,
    const std::vector<std::string>& names,
    const Eigen::MatrixBase<DerivedVector>& direction,
    CastMode cast_mode = CastMode::BOTH_WAYS,
    WrapMode wrap_mode = WrapMode::CONSTANT,
    DefaultScalar default_value = DefaultScalar(0),
    std::function<void(typename TargetMeshType::Index, bool)> user_callback = nullptr,
    EmbreeRayCaster<ScalarOf<SourceMeshType>>* ray_caster = nullptr,
    std::function<bool(IndexOf<TargetMeshType>)> skip_vertex = nullptr)
{
    static_assert(MeshTrait<SourceMeshType>::is_mesh(), "Input type is not Mesh");
    static_assert(MeshTrait<TargetMeshType>::is_mesh(), "Output type is not Mesh");
    la_runtime_assert(source.get_vertex_per_facet() == 3);

    // Typedef festival because templates...
    using Scalar = typename SourceMeshType::Scalar;
    using Index = typename TargetMeshType::Index;
    using SourceArray = typename SourceMeshType::AttributeArray;
    using TargetArray = typename SourceMeshType::AttributeArray;
    using Point = typename EmbreeRayCaster<Scalar>::Point;
    using Direction = typename EmbreeRayCaster<Scalar>::Direction;
    using RayCasterIndex = typename EmbreeRayCaster<Scalar>::Index;
    using Scalar4 = typename EmbreeRayCaster<Scalar>::Scalar4;
    using Index4 = typename EmbreeRayCaster<Scalar>::Index4;
    using Point4 = typename EmbreeRayCaster<Scalar>::Point4;
    using Direction4 = typename EmbreeRayCaster<Scalar>::Direction4;
    using Mask4 = typename EmbreeRayCaster<Scalar>::Mask4;

    // We need to convert to a shared_ptr AND the ray caster will make another copy of the data..
    std::unique_ptr<EmbreeRayCaster<Scalar>> engine;
    if (!ray_caster) {
        auto mesh = lagrange::to_shared_ptr(
            lagrange::create_mesh(source.get_vertices(), source.get_facets()));
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

    // Store pointer to source/target arrays
    std::vector<const SourceArray*> source_attrs;
    source_attrs.reserve(names.size());
    std::vector<TargetArray> target_attrs(names.size());
    for (size_t k = 0; k < names.size(); ++k) {
        const auto& name = names[k];
        la_runtime_assert(source.has_vertex_attribute(name));
        source_attrs.push_back(&source.get_vertex_attribute(name));
        if (target.has_vertex_attribute(name)) {
            target.export_vertex_attribute(name, target_attrs[k]);
        } else {
            target_attrs[k].resize(target.get_num_vertices(), source_attrs[k]->cols());
        }
    }

    auto diag = igl::bounding_box_diagonal(source.get_vertices());

    // Using `char` instead of `bool` because we write concurrently to this
    std::vector<char> is_hit;
    if (wrap_mode != WrapMode::CONSTANT) {
        is_hit.assign(target.get_num_vertices(), false);
    }

    Index num_vertices = target.get_num_vertices();
    Index num_packets = (num_vertices + 3) / 4;

    Direction4 dirs;
    dirs.row(0) = direction.normalized().transpose();
    for (int i = 1; i < 4; ++i) {
        dirs.row(i) = dirs.row(0);
    }
    Direction4 dirs2 = -dirs;

    tbb::parallel_for(Index(0), num_packets, [&](Index packet_index) {
        Index batchsize = std::min(num_vertices - (packet_index << 2), 4);
        Mask4 mask = Mask4::Constant(-1);
        Point4 origins;

        int num_skipped_in_packet = 0;
        for (Index b = 0; b < batchsize; ++b) {
            Index i = (packet_index << 2) + b;
            if (skip_vertex && skip_vertex(i)) {
                logger().trace("skipping vertex: {}", i);
                if (!is_hit.empty()) {
                    is_hit[i] = true;
                }
                mask(b) = 0;
                ++num_skipped_in_packet;
                continue;
            }

            origins.row(b) = target.get_vertices().row(i);
        }

        if (num_skipped_in_packet == batchsize) return;

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
            origins,
            dirs,
            mask,
            mesh_indices,
            instance_indices,
            facet_indices,
            ray_depths,
            barys,
            normals);

        if (cast_mode == CastMode::BOTH_WAYS) {
            // Try again in the other direction. Slightly offset ray origin, and keep closest point.
            Point4 origins2 = origins + Scalar(1e-6) * diag * dirs;
            Index4 mesh_indices2;
            Index4 instance_indices2;
            Index4 facet_indices2;
            Scalar4 ray_depths2;
            Point4 barys2;
            Point4 norms2;
            uint8_t hits2 = ray_caster->cast4(
                batchsize,
                origins2,
                dirs2,
                mask,
                mesh_indices2,
                instance_indices2,
                facet_indices2,
                ray_depths2,
                barys2,
                norms2);

            for (Index b = 0; b < batchsize; ++b) {
                if (!mask(b)) continue;
                auto len2 = std::abs(Scalar(1e-6) * diag - ray_depths2(b));
                bool hit = hits & (1 << b);
                bool hit2 = hits2 & (1 << b);
                if (hit2 && (!hit || len2 < ray_depths(b))) {
                    hits |= 1 << b;
                    mesh_indices(b) = mesh_indices2(b);
                    facet_indices(b) = facet_indices2(b);
                    barys.row(b) = barys2.row(b);
                }
            }
        }
        for (Index b = 0; b < batchsize; ++b) {
            if (!mask(b)) continue;
            bool hit = hits & (1 << b);
            Index i = (packet_index << 2) + b;
            if (hit) {
                // Hit occurred, interpolate data
                la_runtime_assert(
                    facet_indices(b) >= 0 &&
                    facet_indices(b) < static_cast<RayCasterIndex>(source.get_num_facets()));
                auto face = source.get_facets().row(facet_indices(b));
                for (size_t k = 0; k < source_attrs.size(); ++k) {
                    target_attrs[k].row(i).setZero();
                    for (int lv = 0; lv < 3; ++lv) {
                        target_attrs[k].row(i) += source_attrs[k]->row(face[lv]) * barys(b, lv);
                    }
                }
            } else {
                // Not hit. Set values according to wrap_mode + call user-callback at the end if
                // needed
                for (size_t k = 0; k < source_attrs.size(); ++k) {
                    switch (wrap_mode) {
                    case WrapMode::CONSTANT:
                        target_attrs[k].row(i).setConstant(safe_cast<Scalar>(default_value));
                        break;
                    default: break;
                    }
                }
            }
            if (!is_hit.empty()) {
                is_hit[i] = hit;
            }
            if (user_callback) {
                user_callback(i, hit);
            }
        }
    });

    // Not super pretty way, we still need to separately add/create the attribute,
    // THEN import it without copy. Would be better if we could get a ref to it.
    for (size_t k = 0; k < names.size(); ++k) {
        const auto& name = names[k];
        target.add_vertex_attribute(name);
        target.import_vertex_attribute(name, target_attrs[k]);
    }

    // If there is any vertex without a hit, we defer to the relevant functions with filtering
    if (wrap_mode != WrapMode::CONSTANT) {
        bool all_hit = std::all_of(is_hit.begin(), is_hit.end(), [](char x) { return bool(x); });
        if (!all_hit) {
            if (wrap_mode == WrapMode::CLOSEST_POINT) {
                project_attributes_closest_point(source, target, names, ray_caster, [&](Index i) {
                    return bool(is_hit[i]);
                });
            } else if (wrap_mode == WrapMode::CLOSEST_VERTEX) {
                ::lagrange::bvh::project_attributes_closest_vertex(
                    source,
                    target,
                    names,
                    [&](Index i) { return bool(is_hit[i]); });
            } else {
                throw std::runtime_error("not implemented");
            }
        }
    }
}

} // namespace raycasting
} // namespace lagrange
