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
#include <lagrange/compute_barycentric_coordinates.h>
#include <lagrange/create_mesh.h>
#include <lagrange/raycasting/create_ray_caster.h>
#include <lagrange/utils/safe_cast.h>

#include <tbb/parallel_for.h>

#include <string>
#include <type_traits>
#include <vector>

namespace lagrange {
namespace raycasting {

///
/// Project vertex attributes from one mesh to another, by copying attributes from the closest point
/// on the input mesh. Values are linearly interpolated from the face corners.
///
/// @param[in]     source          Source mesh.
/// @param[in,out] target          Target mesh to be modified.
/// @param[in]     names           Name of the vertex attributes to transfer.
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
///
template <typename SourceMeshType, typename TargetMeshType>
void project_attributes_closest_point(
    const SourceMeshType& source,
    TargetMeshType& target,
    const std::vector<std::string>& names,
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
    std::vector<const SourceArray*> source_attrs(names.size());
    std::vector<TargetArray> target_attrs(names.size());
    for (size_t k = 0; k < names.size(); ++k) {
        const auto& name = names[k];
        la_runtime_assert(source.has_vertex_attribute(name));
        source_attrs[k] = &source.get_vertex_attribute(name);
        if (target.has_vertex_attribute(name)) {
            target.export_vertex_attribute(name, target_attrs[k]);
        } else {
            target_attrs[k].resize(target.get_num_vertices(), source_attrs[k]->cols());
        }
    }

    tbb::parallel_for(Index(0), target.get_num_vertices(), [&](Index i) {
        if (skip_vertex && skip_vertex(i)) {
            logger().trace("skipping vertex: {}", i);
            return;
        }
        Point query = target.get_vertices().row(i).transpose();
        auto res = ray_caster->query_closest_point(query);
        la_runtime_assert(
            res.facet_index >= 0 && res.facet_index < (unsigned)source.get_num_facets());
        auto face = source.get_facets().row(res.facet_index).eval();
        Point bary = res.barycentric_coord;

        for (size_t k = 0; k < source_attrs.size(); ++k) {
            target_attrs[k].row(i).setZero();
            for (int lv = 0; lv < 3; ++lv) {
                target_attrs[k].row(i) += source_attrs[k]->row(face[lv]) * bary[lv];
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
}

} // namespace raycasting
} // namespace lagrange
