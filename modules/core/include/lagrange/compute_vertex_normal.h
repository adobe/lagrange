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
#include <cassert>
#include <exception>
#include <iostream>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <igl/per_vertex_normals.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <lagrange/common.h>
#include <lagrange/Mesh.h>
#include <lagrange/compute_triangle_normal.h>
#include <lagrange/MeshTrait.h>

namespace lagrange {
template <typename MeshType>
void compute_vertex_normal(MeshType& mesh)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");
    if (mesh.get_vertex_per_facet() != 3) {
        throw std::runtime_error("Input mesh is not triangle mesh.");
    }

    if (!mesh.has_facet_attribute("normal")) {
        compute_triangle_normal(mesh);
        LA_ASSERT(mesh.has_facet_attribute("normal"));
    }

    using AttributeArray = typename MeshType::AttributeArray;
    const auto& vertices = mesh.get_vertices();
    const auto& facets = mesh.get_facets();
    const auto& facet_normals = mesh.get_facet_attribute("normal");
    AttributeArray vertex_normals;
    igl::per_vertex_normals(
        vertices,
        facets,
        igl::PER_VERTEX_NORMALS_WEIGHTING_TYPE_ANGLE,
        facet_normals,
        vertex_normals);
    mesh.add_vertex_attribute("normal");
    mesh.import_vertex_attribute("normal", vertex_normals);
}
} // namespace lagrange
