/*
 * Copyright 2019 Adobe. All rights reserved.
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

#include <numeric>
#include <unordered_map>
#include <unordered_set>

#include <lagrange/Mesh.h>
#include <lagrange/attributes/map_attributes.h>
#include <lagrange/bvh/create_BVH.h>
#include <lagrange/common.h>
#include <lagrange/mesh_cleanup/remove_isolated_vertices.h>
#include <lagrange/utils/range.h>

namespace lagrange {
namespace bvh {

/**
 * Zip mesh boundary by removing nearly duplicate boundary vertices.
 */
template <typename MeshType>
std::unique_ptr<MeshType> zip_boundary(MeshType& mesh, typename MeshType::Scalar radius)
{
    using Index = typename MeshType::Index;

    auto get_boundary_vertices = [&mesh]() {
        mesh.initialize_edge_data();

        std::vector<Index> boundary_vertices;
        boundary_vertices.reserve(mesh.get_num_vertices());
        for (auto v : range(mesh.get_num_vertices())) {
            if (mesh.is_boundary_vertex(v)) {
                boundary_vertices.push_back(v);
            }
        }
        return boundary_vertices;
    };

    auto get_boundary_points = [&mesh](const auto& boundary_vertices) {
        typename MeshType::VertexArray points(boundary_vertices.size(), mesh.get_dim());
        for (Index i = 0; i < (Index)boundary_vertices.size(); ++i) {
            const auto v = boundary_vertices[i];
            points.row(i) = mesh.get_vertices().row(v);
        }
        return points;
    };

    auto compute_boundary_vertex_mapping =
        [&radius](const auto& engine, const auto& boundary_vertices, const auto& boundary_points) {
            std::unordered_map<Index, Index> vertex_mapping;
            for (Index bvi = 0; bvi < (Index)boundary_vertices.size(); ++bvi) {
                const Index vi = boundary_vertices[bvi];
                if (vertex_mapping.find(vi) != vertex_mapping.end()) continue;

                auto nearby_vertices =
                    engine->query_in_sphere_neighbours(boundary_points.row(bvi), radius);
                for (const auto& entry : nearby_vertices) {
                    const Index bvj = entry.closest_vertex_idx;
                    const Index vj = boundary_vertices[bvj];

                    vertex_mapping[vj] = vi;
                }
            }

            return vertex_mapping;
        };

    auto remap_vertices = [&mesh](const auto vertex_mapping) {
        auto facets = mesh.get_facets();
        std::transform(
            facets.data(),
            facets.data() + facets.size(),
            facets.data(),
            [&vertex_mapping](Index vi) {
                const auto itr = vertex_mapping.find(vi);
                if (itr == vertex_mapping.end()) {
                    return vi;
                } else {
                    return itr->second;
                }
            });
        auto out_mesh = lagrange::create_mesh(mesh.get_vertices(), facets);

        BackwardMeshMapping<MeshType> mapping;
        mapping.vertex.resize(out_mesh->get_num_vertices());
        std::iota(mapping.vertex.begin(), mapping.vertex.end(), 0);
        for (const auto& entry : vertex_mapping) {
            mapping.vertex[entry.first] = entry.second;
        }
        map_attributes(mesh, *out_mesh, mapping);
        return out_mesh;
    };

    const auto boundary_vertices = get_boundary_vertices();
    const auto boundary_points = get_boundary_points(boundary_vertices);
    auto engine = lagrange::bvh::create_BVH(lagrange::bvh::BVHType::NANOFLANN, boundary_points);
    const auto vertex_mapping =
        compute_boundary_vertex_mapping(engine, boundary_vertices, boundary_points);
    const auto out_mesh = remap_vertices(vertex_mapping);
    return remove_isolated_vertices(*out_mesh);
}

} // namespace bvh
} // namespace lagrange
