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

#include <exception>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <igl/per_face_normals.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <lagrange/Mesh.h>
#include <lagrange/MeshTrait.h>

namespace lagrange {

template <typename MeshType>
auto compute_triangle_normal_const(const MeshType &mesh) -> typename MeshType::AttributeArray
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");
    if (mesh.get_vertex_per_facet() != 3) {
        throw std::runtime_error("Input mesh is not triangle mesh.");
    }

    const auto& vertices = mesh.get_vertices();
    const auto& facets = mesh.get_facets();
    typename Eigen::Matrix<typename MeshType::Scalar, 1, 3> default_normal(0, 0, 0);
    typename MeshType::AttributeArray normals;

    igl::per_face_normals(vertices, facets, default_normal, normals);

    return normals;
}

template <typename MeshType>
void compute_triangle_normal(MeshType& mesh)
{
    mesh.add_facet_attribute("normal");
    mesh.import_facet_attribute("normal", compute_triangle_normal_const(mesh));
}

} // namespace lagrange
