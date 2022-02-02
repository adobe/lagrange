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

#include <Eigen/Core>

#include <lagrange/Mesh.h>
#include <lagrange/MeshTrait.h>
#include <lagrange/attributes/map_facet_attributes.h>
#include <lagrange/attributes/map_vertex_attributes.h>
#include <lagrange/create_mesh.h>

namespace lagrange {

template <typename MeshType>
auto quad_to_tri(const MeshType& mesh)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");
    const auto& vertices = mesh.get_vertices();
    const auto& facets = mesh.get_facets();
    la_runtime_assert(facets.cols() == 4);

    using Index = typename MeshType::Index;
    using FacetArray = Eigen::Matrix<Index, Eigen::Dynamic, 3, Eigen::RowMajor>;
    using AttributeArray = typename MeshType::AttributeArray;

    const Index num_quads = mesh.get_num_facets();
    FacetArray triangles(num_quads * 2, 3);
    for (Index i = 0; i < num_quads; i++) {
        triangles.row(i * 2) << facets(i, 0), facets(i, 1), facets(i, 3);
        triangles.row(i * 2 + 1) << facets(i, 3), facets(i, 1), facets(i, 2);
    }

    auto tri_mesh = create_mesh(vertices, triangles);

    if (mesh.is_uv_initialized()) {
        const auto& uv = mesh.get_uv();
        const auto& uv_indices = mesh.get_uv_indices();
        assert(uv_indices.rows() == num_quads);
        assert(uv_indices.cols() == 4);

        FacetArray tri_uv_indices(num_quads * 2, 3);

        for (Index i = 0; i < num_quads; i++) {
            tri_uv_indices.row(i * 2) << uv_indices(i, 0), uv_indices(i, 1), uv_indices(i, 3);
            tri_uv_indices.row(i * 2 + 1) << uv_indices(i, 3), uv_indices(i, 1), uv_indices(i, 2);
        }
        tri_mesh->initialize_uv(uv, tri_uv_indices);
    }

    map_vertex_attributes(mesh, *tri_mesh);
    std::vector<Index> facet_map(num_quads * 2);
    for (Index i = 0; i < num_quads; i++) {
        facet_map[i * 2] = i;
        facet_map[i * 2 + 1] = i;
    }
    map_facet_attributes(mesh, *tri_mesh, facet_map);

    const auto& corner_attr_names = mesh.get_corner_attribute_names();
    for (const auto& name : corner_attr_names) {
        const auto& attr = mesh.get_corner_attribute(name);
        AttributeArray tri_attr(num_quads * 6, attr.cols());
        for (Index i = 0; i < num_quads; i++) {
            tri_attr.row(i * 6) = attr.row(i * 4);
            tri_attr.row(i * 6 + 1) = attr.row(i * 4 + 1);
            tri_attr.row(i * 6 + 2) = attr.row(i * 4 + 3);
            tri_attr.row(i * 6 + 3) = attr.row(i * 4 + 3);
            tri_attr.row(i * 6 + 4) = attr.row(i * 4 + 1);
            tri_attr.row(i * 6 + 5) = attr.row(i * 4 + 2);
        }
        tri_mesh->add_corner_attribute(name);
        tri_mesh->set_corner_attribute(name, tri_attr);
    }

    return tri_mesh;
}

} // namespace lagrange
