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
#include <lagrange/bvh/BVHNanoflann.h>
#include <lagrange/common.h>

#include <tbb/parallel_for.h>

#include <string>
#include <vector>

namespace lagrange {
namespace bvh {

///
/// Project vertex attributes from one mesh to another, by copying attributes from the closest
/// vertex on the input mesh.
///
/// @note          In the future we may want to add other interpolation methods such as RBFs /
///                Voronoi.
///
/// @param[in]     source          Source mesh.
/// @param[in,out] target          Target mesh to be modified.
/// @param[in]     names           Name of the vertex attributes to transfer.
/// @param[in]     skip_vertex     If provided, whether to skip assignment for a target vertex or
///                                not. This can be used for partial assignment (e.g. to only set
///                                boundary vertices of a mesh).
///
/// @tparam        SourceMeshType  Source mesh type.
/// @tparam        TargetMeshType  Target mesh type.
///
template <typename SourceMeshType, typename TargetMeshType>
void project_attributes_closest_vertex(
    const SourceMeshType &source,
    TargetMeshType &target,
    const std::vector<std::string> &names,
    std::function<bool(IndexOf<TargetMeshType>)> skip_vertex = nullptr)
{
    static_assert(MeshTrait<SourceMeshType>::is_mesh(), "Input type is not Mesh");
    static_assert(MeshTrait<TargetMeshType>::is_mesh(), "Output type is not Mesh");

    using VertexArray = typename SourceMeshType::VertexArray;
    using BVH_t = BVHNanoflann<VertexArray>;
    using Index = typename TargetMeshType::Index;
    using SourceArray = typename SourceMeshType::AttributeArray;
    using TargetArray = typename SourceMeshType::AttributeArray;

    auto engine = std::make_unique<BVH_t>();
    engine->build(source.get_vertices());

    // Store pointer to source/target arrays
    std::vector<const SourceArray *> source_attrs(names.size());
    std::vector<TargetArray> target_attrs(names.size());
    for (size_t k = 0; k < names.size(); ++k) {
        const auto &name = names[k];
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
        auto res = engine->query_closest_point(target.get_vertices().row(i));
        for (size_t k = 0; k < source_attrs.size(); ++k) {
            target_attrs[k].row(i) = source_attrs[k]->row(res.closest_vertex_idx);
        }
    });

    // Not super pretty way, we still need to separately add/create the attribute,
    // THEN import it without copy. Would be better if we could get a ref to it.
    for (size_t k = 0; k < names.size(); ++k) {
        const auto &name = names[k];
        target.add_vertex_attribute(name);
        target.import_vertex_attribute(name, target_attrs[k]);
    }
}

} // namespace bvh
} // namespace lagrange
