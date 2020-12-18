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
#include <lagrange/attributes/map_attributes.h>
#include <lagrange/chain_corners_around_edges.h>
#include <lagrange/chain_edges_into_simple_loops.h>
#include <lagrange/common.h>
#include <lagrange/create_mesh.h>
#include <lagrange/DisjointSets.h>
#include <lagrange/utils/stl_eigen.h>

#include <vector>

namespace lagrange {

///
/// Close small topological holes. For holes with > 3 vertices, inserts a vertex
/// at the barycenter of the hole polygon. If a hole is not a simple polygon,
/// we attempt to turn it into simple polygons by cutting ears.
///
/// @todo       Remap attribute. For now all input mesh attributes are dropped.
///
/// @param[in]  mesh           Input mesh whose holes to close.
/// @param[in]  max_hole_size  Maximum number of vertices on a hole to be
///                            closed.
///
/// @tparam     MeshType       Mesh type.
///
/// @return     A new mesh with the holes clossed.
///
template <typename MeshType>
std::unique_ptr<MeshType> close_small_holes(MeshType &mesh, size_t max_hole_size)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");
    LA_ASSERT(mesh.get_vertex_per_facet() == 3, "This method is only for triangle meshes.");

    using Scalar = typename MeshType::Scalar;
    using Index = typename MeshType::Index;
    using VertexArray = typename MeshType::VertexArray;
    using FacetArray = typename MeshType::FacetArray;
    using AttributeArray = typename MeshType::AttributeArray;

    logger().trace("[close_small_holes] initialize edge data");
    mesh.initialize_edge_data_new();

    // Compute boundary edge list + reduce indexing of boundary vertices
    logger().trace("[close_small_holes] clustering holes");
    const Index num_vertices = mesh.get_num_vertices();
    const Index num_facets = mesh.get_num_facets();
    const Index nvpf = mesh.get_vertex_per_facet();
    const Index dim = mesh.get_dim();
    std::vector<Index> vertex_to_reduced(num_vertices, INVALID<Index>());
    std::vector<Index> reduced_to_vertex;
    std::vector<std::array<Index, 2>> boundary_edges;
    std::vector<std::array<Index, 2>> boundary_corners;

    auto get_reduced_index = [&](Index v) {
        if (vertex_to_reduced[v] == INVALID<Index>()) {
            vertex_to_reduced[v] = static_cast<Index>(reduced_to_vertex.size());
            reduced_to_vertex.push_back(v);
        }
        return vertex_to_reduced[v];
    };

    for (Index e = 0; e < mesh.get_num_edges_new(); ++e) {
        const Index c = mesh.get_one_corner_around_edge_new(e);
        assert(c != INVALID<Index>());
        if (mesh.is_boundary_edge_new(e)) {
            const Index f = c / nvpf;
            const Index lv = c % nvpf;
            const Index lv2 = (lv + 1) % nvpf;
            const Index v1 = mesh.get_facets()(f, lv);
            const Index v2 = mesh.get_facets()(f, lv2);
            // Flip boundary edges to correctly orient the hole polygon
            boundary_edges.push_back({{get_reduced_index(v2), get_reduced_index(v1)}});
            boundary_corners.push_back({{f * nvpf + lv2, f * nvpf + lv}});
        }
    }

    // Compute simple loops
    logger().trace("[close_small_holes] chain edges into simple loops");
    std::vector<std::vector<Index>> loops;
    Eigen::Matrix<Index, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> remaining_edges;
    {
        Eigen::Map<const Eigen::Matrix<Index, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>>
            edges_map(
                reinterpret_cast<const Index *>(boundary_edges.data()),
                boundary_edges.size(),
                2);
        chain_edges_into_simple_loops(edges_map, loops, remaining_edges);
    }

    auto get_from_corner = [](const auto &matrix, Index c) {
        const Index nvpf = matrix.cols();
        return matrix(c / nvpf, c % nvpf);
    };

    // Compute which hole can be filled with a single triangle,
    // and which hole needs an additional vertex at its barycenter
    std::vector<bool> needs_barycenter(loops.size(), false);
    for (const auto &name : mesh.get_indexed_attribute_names()) {
        const auto &attr = mesh.get_indexed_attribute(name);
        const auto &indices = std::get<1>(attr);
        for (size_t loop_id = 0; loop_id < loops.size(); ++loop_id) {
            const auto &loop = loops[loop_id];
            if (loop.size() <= max_hole_size && loop.size() <= 3) {
                assert(loop.size() == 3); // holes of size 1 or 2 shouldn't be possible
                for (Index lv = 0; lv < 3; ++lv) {
                    const Index c_prev = boundary_corners[loop[lv]][1];
                    const Index c_next = boundary_corners[loop[(lv + 1) % 3]][0];
                    const Index idx_prev = get_from_corner(indices, c_prev);
                    const Index idx_next = get_from_corner(indices, c_next);
                    if (idx_prev != idx_next) {
                        needs_barycenter[loop_id] = true;
                    }
                }
            }
        }
    }

    // Compute hole barycenters + facets
    logger().trace("[close_small_holes] compute hole barycenters + facets");
    using Triangle = std::array<Index, 3>;
    std::vector<Scalar> values;
    std::vector<Triangle> new_facets;
    eigen_to_flat_vector(mesh.get_vertices(), values, Eigen::RowMajor);
    for (size_t loop_id = 0; loop_id < loops.size(); ++loop_id) {
        const auto &loop = loops[loop_id];
        if (loop.size() <= max_hole_size) {
            if (loop.size() == 3 && !needs_barycenter[loop_id]) {
                assert(loop.size() == 3); // holes of size 1 or 2 shouldn't be possible
                const Index v0 = reduced_to_vertex[boundary_edges[loop[0]][0]];
                const Index v1 = reduced_to_vertex[boundary_edges[loop[1]][0]];
                const Index v2 = reduced_to_vertex[boundary_edges[loop[2]][0]];
                new_facets.push_back({{v0, v1, v2}});
            } else {
                // Hole with a barycenter
                const Index vc = (Index)values.size() / dim;
                values.insert(values.end(), dim, 0);
                assert(values.size() % dim == 0);
                for (auto e : loop) {
                    const Index vi = reduced_to_vertex[boundary_edges[e][0]];
                    const Index vj = reduced_to_vertex[boundary_edges[e][1]];
                    for (Index k = 0; k < dim; ++k) {
                        values[vc * dim + k] += mesh.get_vertices()(vi, k);
                    }
                    new_facets.push_back({{vi, vj, vc}});
                }
                for (Index k = 0; k < dim; ++k) {
                    values[vc * dim + k] /= static_cast<Scalar>(loop.size());
                }
            }
        }
    }

    // Append elements to existing mesh
    logger().trace("[close_small_holes] append new facets");
    VertexArray vertices;
    flat_vector_to_eigen(
        values, vertices, (Index) values.size() / dim, dim, Eigen::RowMajor);

    FacetArray facets(num_facets + new_facets.size(), 3);
    facets.topRows(num_facets) = mesh.get_facets();
    for (Index f = 0; f < (Index)new_facets.size(); ++f) {
        facets.row(num_facets + f) << new_facets[f][0], new_facets[f][1], new_facets[f][2];
    }
    auto new_mesh = lagrange::create_mesh(std::move(vertices), std::move(facets));

    // Remap vertex attributes
    for (const auto &name : mesh.get_vertex_attribute_names()) {
        const auto &attr = mesh.get_vertex_attribute(name);
        AttributeArray vals(new_mesh->get_num_vertices(), attr.cols());
        vals.topRows(attr.rows()) = attr;
        Index counter = mesh.get_num_vertices();
        for (size_t loop_id = 0; loop_id < loops.size(); ++loop_id) {
            const auto &loop = loops[loop_id];
            if (loop.size() <= max_hole_size) {
                if (loop.size() == 3 && !needs_barycenter[loop_id]) {
                    // Nothing to do
                } else {
                    // Compute average attribute value
                    vals.row(counter).setZero();
                    for (auto e : loop) {
                        const Index vi = reduced_to_vertex[boundary_edges[e][0]];
                        vals.row(counter) += attr.row(vi);
                    }
                    vals.row(counter) /= static_cast<Scalar>(loop.size());
                    ++counter;
                }
            }
        }
        new_mesh->add_vertex_attribute(name);
        new_mesh->import_vertex_attribute(name, std::move(vals));
    }

    // Remap facet attributes
    for (const auto &name : mesh.get_facet_attribute_names()) {
        const auto &attr = mesh.get_facet_attribute(name);
        AttributeArray vals(new_mesh->get_num_facets(), attr.cols());
        vals.topRows(attr.rows()) = attr;
        Index counter = mesh.get_num_facets();
        for (size_t loop_id = 0; loop_id < loops.size(); ++loop_id) {
            const auto &loop = loops[loop_id];
            if (loop.size() <= max_hole_size) {
                if (loop.size() == 3 && !needs_barycenter[loop_id]) {
                    // Average attribute from opposite facets
                    vals.row(counter).setZero();
                    for (Index i = 0; i < 3; ++i) {
                        const Index c = boundary_corners[loop[i]][0];
                        vals.row(counter) += vals.row(c / nvpf);
                    }
                    vals.row(counter) /= 3;
                    ++counter;
                } else {
                    // Copy attributes from opposite facets
                    for (auto e : loop) {
                        const Index c = boundary_corners[e][0];
                        vals.row(counter++) = attr.row(c / nvpf);
                    }
                }
            }
        }
        new_mesh->add_facet_attribute(name);
        new_mesh->import_facet_attribute(name, std::move(vals));
    }

    // Remap edge attributes (old API, fill with zeros)
    for (const auto &name : mesh.get_edge_attribute_names()) {
        new_mesh->initialize_edge_data();
        logger().warn("[close_small_holes] edge attribute (old API) filled with zeros: {}", name);
        const auto &attr = mesh.get_edge_attribute(name);
        AttributeArray vals(new_mesh->get_num_edges(), attr.cols());
        vals.topRows(attr.rows()) = attr;
        vals.bottomRows(vals.rows() - attr.rows()).setZero();
        new_mesh->add_edge_attribute(name);
        new_mesh->import_edge_attribute(name, std::move(vals));
    }

    // Remap edge attributes (new API, average values)
    for (const auto &name : mesh.get_edge_attribute_names_new()) {
        new_mesh->initialize_edge_data_new();
        const auto &attr = mesh.get_edge_attribute_new(name);
        AttributeArray vals(new_mesh->get_num_edges_new(), attr.cols());
        vals.setZero();

        // Remap old values
        for (Index f = 0; f < mesh.get_num_facets(); ++f) {
            for (Index lv = 0; lv < nvpf; ++lv) {
                const Index old_e = mesh.get_edge_new(f, lv);
                const Index new_e = new_mesh->get_edge_new(f, lv);
                vals.row(new_e) = attr.row(old_e);
            }
        }

        // Compute new values
        Index facet_counter = mesh.get_num_facets();
        for (size_t loop_id = 0; loop_id < loops.size(); ++loop_id) {
            const auto &loop = loops[loop_id];
            if (loop.size() <= max_hole_size) {
                if (loop.size() == 3 && !needs_barycenter[loop_id]) {
                    // Nothing to do
                    facet_counter++;
                } else {
                    // Copy attributes from opposite facets
                    for (auto e : loop) {
                        const Index v0 = reduced_to_vertex[boundary_edges[e][0]];
                        const Index c = boundary_corners[e][1];
                        const Index f = c / nvpf;
                        assert(f == boundary_corners[e][0] / nvpf);
                        const Index lv = c % nvpf;
                        LA_ASSERT_DEBUG(mesh.get_facets()(f, (lv + 1) % nvpf) == v0);
                        const Index e0 = new_mesh->get_edge_new(facet_counter, 0);
                        const Index e1 = new_mesh->get_edge_new(facet_counter, 1);
                        const Index e2 = new_mesh->get_edge_new(facet_counter, 2);
                        assert(e0 == new_mesh->get_edge_new(f, lv));
                        vals.row(e1) += Scalar(0.5) * vals.row(e0);
                        vals.row(e2) += Scalar(0.5) * vals.row(e0);
                        facet_counter++;
                    }
                }
            }
        }
        new_mesh->add_edge_attribute_new(name);
        new_mesh->import_edge_attribute_new(name, std::move(vals));
    }

    // Remap corner attributes
    for (const auto &name : mesh.get_corner_attribute_names()) {
        const auto &attr = mesh.get_corner_attribute(name);
        AttributeArray vals(new_mesh->get_num_facets() * nvpf, attr.cols());
        vals.topRows(attr.rows()) = attr;
        Index counter = mesh.get_num_facets();
        for (size_t loop_id = 0; loop_id < loops.size(); ++loop_id) {
            const auto &loop = loops[loop_id];
            if (loop.size() <= max_hole_size) {
                if (loop.size() == 3 && !needs_barycenter[loop_id]) {
                    assert(loop.size() == 3); // holes of size 1 or 2 shouldn't be possible
                    // Average contributions from opposite corners
                    const auto c01 = boundary_corners[loop[0]];
                    const auto c12 = boundary_corners[loop[1]];
                    const auto c20 = boundary_corners[loop[2]];
                    const Scalar w = 0.5;
                    vals.row(counter * nvpf + 0) = w * (vals.row(c01[0]) + vals.row(c20[1]));
                    vals.row(counter * nvpf + 1) = w * (vals.row(c12[0]) + vals.row(c01[1]));
                    vals.row(counter * nvpf + 2) = w * (vals.row(c20[0]) + vals.row(c12[1]));
                    ++counter;
                } else {
                    // Copy attributes from opposite facets, and average corner attributes in the barycenter
                    const Index shared_corner = counter * nvpf + 2;
                    vals.row(shared_corner).setZero();
                    for (auto e : loop) {
                        const auto c = boundary_corners[e];
                        vals.row(counter * nvpf + 0) = vals.row(c[0]);
                        vals.row(counter * nvpf + 1) = vals.row(c[1]);
                        vals.row(shared_corner) += vals.row(c[0]);
                        vals.row(shared_corner) += vals.row(c[1]);
                        counter++;
                    }
                    vals.row(shared_corner) /= 2 * loop.size();
                    for (Index i = 0; i < (Index) loop.size(); ++i) {
                        vals.row(shared_corner + i * nvpf) = vals.row(shared_corner);
                    }
                }
            }
        }
        new_mesh->add_corner_attribute(name);
        new_mesh->import_corner_attribute(name, std::move(vals));
    }

    // Remap indexed attributes
    // TODO: There should be way to iterate over attributes without going through a std::string
    std::vector<Index> indices;
    DisjointSets<Index> groups;
    std::vector<Index> group_color;
    std::vector<Index> group_sizes;
    std::vector<Scalar> group_means;
    for (const auto &name : mesh.get_indexed_attribute_names()) {
        const auto &attr = mesh.get_indexed_attribute(name);
        const Index num_coords = std::get<0>(attr).cols();
        logger().trace("[close_small_holes] remaping indexed attribute: {}", name);
        eigen_to_flat_vector(std::get<0>(attr), values, Eigen::RowMajor);
        eigen_to_flat_vector(std::get<1>(attr), indices, Eigen::RowMajor);
        for (size_t loop_id = 0; loop_id < loops.size(); ++loop_id) {
            const auto &loop = loops[loop_id];
            if (loop.size() <= max_hole_size) {
                if (loop.size() == 3 && !needs_barycenter[loop_id]) {
                    assert(loop.size() == 3); // holes of size 1 or 2 shouldn't be possible
                    indices.push_back(indices[boundary_corners[loop[0]][0]]);
                    indices.push_back(indices[boundary_corners[loop[1]][0]]);
                    indices.push_back(indices[boundary_corners[loop[2]][0]]);
                } else {
                    // Compute groups by joining corners that share an attribute index
                    const size_t nv = loop.size(); // number of vertices/edges on the loop
                    groups.init(2 * nv); // number of loop corners
                    for (size_t i = 0; i < loop.size(); ++i) {
                        const size_t j = (i + 1) % loop.size();
                        const Index lci = 2 * i + 1;
                        const Index lcj = 2 * j + 0;
                        const Index ci = boundary_corners[loop[i]][1];
                        const Index cj = boundary_corners[loop[j]][0];
                        groups.merge(2 * i, 2 * i + 1);
                        if (indices[ci] == indices[cj]) {
                            groups.merge(lci, lcj);
                        }
                    }
                    // Compute average attribute value per group
                    const Index num_groups = groups.extract_disjoint_set_indices(group_color);
                    logger().trace("num groups: {}", num_groups);
                    group_sizes.assign(num_groups, 0);
                    group_means.assign(num_groups * num_coords, 0);
                    for (size_t lc = 0; lc < 2 * loop.size(); ++lc) {
                        const Index idx = indices[boundary_corners[loop[lc / 2]][lc % 2]];
                        const Scalar * val = values.data() + idx * num_coords;
                        const Index repr = group_color[lc];
                        group_sizes[repr]++;
                        for (Index k = 0; k < num_coords; ++k) {
                            group_means[repr * num_coords + k] += val[k];
                        }
                    }
                    // Normalize mean value for each group + allocate new attribute vertices
                    const Index offset = static_cast<Index>(values.size()) / num_coords;
                    for (size_t g = 0; g < num_groups; ++g) {
                        for (Index k = 0; k < num_coords; ++k) {
                            assert(group_sizes[g] > 0);
                            group_means[g * num_coords + k] /= static_cast<Scalar>(group_sizes[g]);
                            values.push_back(group_means[g * num_coords + k]);
                        }
                        assert(static_cast<Index>(values.size()) % num_coords == 0);
                    }
                    // Allocate new attribute indices
                    for (size_t i = 0; i < loop.size(); ++i) {
                        const Index lci = 2 * i + 0;
                        const Index idxi = indices[boundary_corners[loop[i]][0]];
                        const Index idxj = indices[boundary_corners[loop[i]][1]];
                        indices.push_back(idxi);
                        indices.push_back(idxj);
                        indices.push_back(offset + (group_color[lci] + 0) % 3);
                    }
                }
            }
        }
        size_t num_values = values.size() / num_coords;
        size_t num_indices = indices.size() / nvpf;
        AttributeArray values_;
        FacetArray indices_;
        flat_vector_to_eigen(values, values_, num_values, num_coords, Eigen::RowMajor);
        flat_vector_to_eigen(indices, indices_, num_indices, nvpf, Eigen::RowMajor);
        new_mesh->add_indexed_attribute(name);
        new_mesh->import_indexed_attribute(name, std::move(values_), std::move(indices_));
    }

    logger().trace("[close_small_holes] cleanup");
    return new_mesh;
}

} // namespace lagrange
