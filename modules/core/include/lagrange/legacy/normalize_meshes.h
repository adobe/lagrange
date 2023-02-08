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

#include <lagrange/Mesh.h>
#include <lagrange/MeshTrait.h>
#include <lagrange/legacy/inline.h>

namespace lagrange {
LAGRANGE_LEGACY_INLINE
namespace legacy {

///
/// Normalize a list of meshes to fit in a unit box centered at the origin.
///
/// @param[in]  meshes       List of pointers to the meshes to modify.
///
/// @tparam     MeshTypePtr  Mesh pointer type.
///
template <typename MeshTypePtr>
void normalize_meshes(const std::vector<MeshTypePtr>& meshes)
{
    static_assert(MeshTrait<MeshTypePtr>::is_mesh_ptr(), "Input type is not a Mesh pointer type");
    using MeshType = typename std::pointer_traits<MeshTypePtr>::element_type;
    using Scalar = typename MeshType::Scalar;
    using VertexArray = typename MeshType::VertexArray;
    using VectorType = Eigen::Matrix<Scalar, 1, VertexArray::ColsAtCompileTime>;

    if (meshes.empty()) {
        return;
    }

    const auto DIM = meshes.front()->get_vertices().cols();

    VectorType min_pos = VectorType::Constant(DIM, std::numeric_limits<Scalar>::max());
    VectorType max_pos = VectorType::Constant(DIM, std::numeric_limits<Scalar>::lowest());

    for (const auto& mesh : meshes) {
        const auto& V = mesh->get_vertices();
        min_pos = min_pos.cwiseMin(V.colwise().minCoeff());
        max_pos = max_pos.cwiseMax(V.colwise().maxCoeff());
    }

    const Scalar scaling = Scalar(1) / (max_pos - min_pos).maxCoeff();
    const auto origin = Scalar(0.5) * (min_pos + max_pos);

    for (auto& mesh : meshes) {
        VertexArray vertices;
        mesh->export_vertices(vertices);
        vertices = (vertices.array().rowwise() - origin.array()) * scaling;
        mesh->import_vertices(vertices);
    }
}

template <typename MeshType>
void normalize_mesh(MeshType& mesh)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");
    std::vector<MeshType*> vec = {&mesh};
    legacy::normalize_meshes(vec);
}

} // namespace legacy
} // namespace lagrange
