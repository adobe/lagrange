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

#include <lagrange/bvh/project_attributes_closest_vertex.h>
#include <lagrange/raycasting/project_attributes_closest_point.h>
#include <lagrange/raycasting/project_attributes_directional.h>
#include <lagrange/raycasting/project_options.h>

namespace lagrange {
namespace raycasting {

///
/// Project vertex attributes from one mesh to another. Different projection modes can be prescribed.
///
/// @param[in]     source          Source mesh.
/// @param[in,out] target          Target mesh to be modified.
/// @param[in]     names           Name of the vertex attributes to transfer.
/// @param[in]     project_mode    Projection mode to choose from.
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
    typename DerivedVector = Eigen::Matrix<ScalarOf<SourceMeshType>, 3, 1>,
    typename DefaultScalar = typename SourceMeshType::Scalar>
void project_attributes(
    const SourceMeshType& source,
    TargetMeshType& target,
    const std::vector<std::string>& names,
    ProjectMode project_mode,
    const Eigen::MatrixBase<DerivedVector>& direction = DerivedVector(0, 0, 1),
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

    switch (project_mode) {
    case ProjectMode::CLOSEST_VERTEX:
        ::lagrange::bvh::project_attributes_closest_vertex(source, target, names, skip_vertex);
        break;
    case ProjectMode::CLOSEST_POINT:
        project_attributes_closest_point(source, target, names, ray_caster, skip_vertex);
        break;
    case ProjectMode::RAY_CASTING:
        project_attributes_directional(
            source,
            target,
            names,
            direction,
            cast_mode,
            wrap_mode,
            default_value,
            user_callback,
            ray_caster,
            skip_vertex);
        break;
    default: throw std::runtime_error("Not implemented");
    }
}

} // namespace raycasting
} // namespace lagrange
