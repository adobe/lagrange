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

#include <vector>

#include <lagrange/Edge.h>
#include <lagrange/Mesh.h>
#include <lagrange/attributes/attribute_utils.h>
#include <lagrange/attributes/map_corner_attributes.h>
#include <lagrange/attributes/rename_attribute.h>
#include <lagrange/common.h>
#include <lagrange/create_mesh.h>
#include <lagrange/mesh_cleanup/split_triangle.h>
#include <lagrange/utils/safe_cast.h>

namespace lagrange {

// TODO: Should accept const ref, needs refactor (non-moving export facet attribute)
template <typename MeshType>
std::unique_ptr<MeshType>
split_long_edges(MeshType& mesh, typename MeshType::Scalar sq_tol, bool recursive = false)
{
    using Index = typename MeshType::Index;
    using Scalar = typename MeshType::Scalar;
    using Edge = typename MeshType::Edge;
    if (mesh.get_vertex_per_facet() != 3) {
        throw "Only triangle is supported";
    }
    const Index dim = mesh.get_dim();
    const Index num_vertices = mesh.get_num_vertices();
    const Index num_facets = mesh.get_num_facets();
    const auto& vertices = mesh.get_vertices();
    const auto& facets = mesh.get_facets();
    std::vector<typename MeshType::VertexType> additional_vertices;
    EdgeMap<Index, std::vector<Index>> splitting_pts;
    EdgeSet<Index> visited;
    std::vector<std::tuple<Index, Index, Scalar>> vertex_mapping;

    typename MeshType::AttributeArray active_facets;
    const bool has_active_region = mesh.has_facet_attribute("__is_active");
    if (has_active_region) {
        mesh.export_facet_attribute("__is_active", active_facets);
    } else {
        active_facets.resize(num_facets, 1);
        active_facets.setZero();
    }
    auto is_active = [&active_facets](Index fid) { return active_facets(fid, 0) != 0; };

    auto split_edge = [&vertices,
                       &additional_vertices,
                       &splitting_pts,
                       &visited,
                       sq_tol,
                       num_vertices,
                       &vertex_mapping](const Edge& edge) {
        if (visited.find(edge) != visited.end()) return;
        visited.insert(edge);
        if (splitting_pts.find(edge) != splitting_pts.end()) return;
        Scalar sq_length = (vertices.row(edge[0]) - vertices.row(edge[1])).squaredNorm();
        if (sq_length <= sq_tol) return;

        const Scalar num_segments = std::ceil(sqrt(sq_length / sq_tol));
        const auto v0 = vertices.row(edge[0]).eval();
        const auto v1 = vertices.row(edge[1]).eval();
        std::vector<Index> split_pts_indices;
        split_pts_indices.push_back(edge[0]);
        const Index base = safe_cast<Index>(additional_vertices.size()) + num_vertices;
        for (Index i = 1; i < num_segments; i++) {
            Scalar ratio = i / num_segments;
            additional_vertices.push_back(v0 * (1.0f - ratio) + v1 * ratio);
            vertex_mapping.emplace_back(edge[0], edge[1], 1.0f - ratio);
            split_pts_indices.push_back(base + i - 1);
        }
        split_pts_indices.push_back(edge[1]);
        splitting_pts.insert({edge, split_pts_indices});
    };

    for (Index i = 0; i < num_facets; i++) {
        if (has_active_region && !is_active(i)) continue;
        split_edge({{facets(i, 0), facets(i, 1)}});
        split_edge({{facets(i, 1), facets(i, 2)}});
        split_edge({{facets(i, 2), facets(i, 0)}});
    }

    // Concatenate original vertices and additional vertices.
    const Index total_num_vertices = num_vertices + safe_cast<Index>(additional_vertices.size());
    la_runtime_assert(vertex_mapping.size() == additional_vertices.size());
    typename MeshType::VertexArray all_vertices(total_num_vertices, dim);
    all_vertices.block(0, 0, num_vertices, dim) = vertices;
    for (Index i = num_vertices; i < total_num_vertices; i++) {
        all_vertices.row(i) = additional_vertices[i - num_vertices];
    }

    // Re-triangulate facets.
    std::vector<Eigen::Matrix<Index, 3, 1>> out_facets;
    std::vector<Index> facet_map;
    for (Index i = 0; i < num_facets; i++) {
        Index corners[3];
        std::vector<Index> chain;

        // Copy over inactive facets.
        if (has_active_region && !is_active(i)) {
            la_runtime_assert(facets.row(i).maxCoeff() < total_num_vertices);
            la_runtime_assert(facets.row(i).minCoeff() >= 0);
            out_facets.push_back(facets.row(i));
            facet_map.push_back(i);
            continue;
        }

        for (Index j = 0; j < 3; j++) {
            Edge e{{facets(i, j), facets(i, (j + 1) % 3)}};
            corners[j] = safe_cast<Index>(chain.size());
            chain.push_back(e[0]);
            auto itr = splitting_pts.find(e);
            if (itr != splitting_pts.end()) {
                const auto& pts = itr->second;
                la_runtime_assert(pts.size() >= 3);
                if (pts[0] == e[0]) {
                    auto start = std::next(pts.cbegin());
                    auto end = std::prev(pts.cend());
                    chain.insert(chain.end(), start, end);
                } else {
                    la_runtime_assert(pts[0] == e[1]);
                    auto start = std::next(pts.crbegin());
                    auto end = std::prev(pts.crend());
                    chain.insert(chain.end(), start, end);
                }
            }
        }
        if (chain.size() == 3) {
            // No split.
            la_runtime_assert(facets.row(i).maxCoeff() < total_num_vertices);
            la_runtime_assert(facets.row(i).minCoeff() >= 0);
            out_facets.push_back(facets.row(i));
            facet_map.push_back(i);
            active_facets(i, 0) = 0;
        } else {
            auto additional_facets =
                split_triangle(all_vertices, chain, corners[0], corners[1], corners[2]);
            std::copy(
                additional_facets.begin(),
                additional_facets.end(),
                std::back_inserter(out_facets));
            facet_map.insert(facet_map.end(), additional_facets.size(), i);
            active_facets(i, 0) = 1;
        }
    }

    const Index num_out_facets = safe_cast<Index>(out_facets.size());
    typename MeshType::FacetArray all_facets(num_out_facets, 3);
    for (Index i = 0; i < num_out_facets; i++) {
        la_runtime_assert(out_facets[i].minCoeff() >= 0);
        la_runtime_assert(out_facets[i].maxCoeff() < total_num_vertices);
        all_facets.row(i) = out_facets[i];
    }

    // Mark active facets (i.e. facets that are split).
    if (has_active_region) {
        mesh.import_facet_attribute("__is_active", active_facets);
    } else {
        mesh.add_facet_attribute("__is_active");
        mesh.import_facet_attribute("__is_active", active_facets);
    }

    auto out_mesh = create_mesh(all_vertices, all_facets);

    // Port vertex attributes
    auto vertex_attributes = mesh.get_vertex_attribute_names();
    auto map_vertex_fn = [&](Eigen::Index i,
                             std::vector<std::pair<Eigen::Index, double>>& weights) {
        weights.clear();
        if (i < num_vertices) {
            weights.emplace_back(i, 1.0);
        } else {
            const auto& record = vertex_mapping[i - num_vertices];
            Index v0 = std::get<0>(record);
            Index v1 = std::get<1>(record);
            Scalar ratio = std::get<2>(record);
            weights.emplace_back(v0, ratio);
            weights.emplace_back(v1, 1.0 - ratio);
        }
    };

    for (const auto& name : vertex_attributes) {
        auto attr = mesh.get_vertex_attribute_array(name);
        auto attr2 = to_shared_ptr(attr->row_slice(total_num_vertices, map_vertex_fn));
        out_mesh->add_vertex_attribute(name);
        out_mesh->set_vertex_attribute_array(name, std::move(attr2));
    }

    // Port facet attributes
    auto facet_attributes = mesh.get_facet_attribute_names();
    for (const auto& name : facet_attributes) {
        auto attr = mesh.get_facet_attribute_array(name);
        auto attr2 = to_shared_ptr(attr->row_slice(facet_map));
        out_mesh->add_facet_attribute(name);
        out_mesh->set_facet_attribute_array(name, std::move(attr2));
    }

    // Port corner attributes
    map_corner_attributes(mesh, *out_mesh, facet_map);

    // Port indexed attributes.
    const auto& indexed_attr_names = mesh.get_indexed_attribute_names();
    typename MeshType::AttributeArray attr;
    typename MeshType::IndexArray indices;
    for (const auto& attr_name : indexed_attr_names) {
        std::tie(attr, indices) = mesh.get_indexed_attribute(attr_name);
        assert(indices.rows() == facets.rows());

        auto get_vertex_index_in_facet = [&facets](Index fid, Index vid) -> Index {
            if (vid == facets(fid, 0))
                return 0;
            else if (vid == facets(fid, 1))
                return 1;
            else {
                assert(vid == facets(fid, 2));
                return 2;
            }
        };

        typename MeshType::AttributeArray out_attr(num_out_facets * 3, attr.cols());
        for (auto i : range(num_out_facets)) {
            const auto old_fid = facet_map[i];
            assert(old_fid < facets.rows());

            for (Index j = 0; j < 3; j++) {
                const auto vid = all_facets(i, j);
                if (vid < num_vertices) {
                    const auto old_j = get_vertex_index_in_facet(old_fid, vid);
                    // Vertex is part of the input vertices.
                    out_attr.row(i * 3 + j) = attr.row(indices(old_fid, old_j));
                } else {
                    const auto& record = vertex_mapping[vid - num_vertices];
                    Index v0 = std::get<0>(record);
                    Index v1 = std::get<1>(record);
                    Scalar ratio = std::get<2>(record);
                    Index old_j0 = get_vertex_index_in_facet(old_fid, v0);
                    Index old_j1 = get_vertex_index_in_facet(old_fid, v1);

                    out_attr.row(i * 3 + j) = attr.row(indices(old_fid, old_j0)) * ratio +
                                            attr.row(indices(old_fid, old_j1)) * (1.0f - ratio);
                }
            }
        }

        if (attr_name == "normal") {
            for (auto i : range(num_out_facets * 3)) {
                out_attr.row(i).stableNormalize();
            }
        }

        const std::string tmp_name = "__" + attr_name;
        out_mesh->add_corner_attribute(tmp_name);
        out_mesh->import_corner_attribute(tmp_name, out_attr);
        map_corner_attribute_to_indexed_attribute(*out_mesh, tmp_name);
        rename_indexed_attribute(*out_mesh, tmp_name, attr_name);
        out_mesh->remove_corner_attribute(tmp_name);
    }

    if (recursive && total_num_vertices > num_vertices) {
        return split_long_edges(*out_mesh, sq_tol, recursive);
    } else
        return out_mesh;
}
} // namespace lagrange
