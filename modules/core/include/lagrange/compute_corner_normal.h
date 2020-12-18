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
#include <numeric>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <igl/internal_angles.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <lagrange/common.h>
#include <lagrange/Mesh.h>
#include <lagrange/compute_triangle_normal.h>
#include <lagrange/MeshTrait.h>
#include <lagrange/compute_dihedral_angles.h>
#include <lagrange/compute_facet_area.h>

namespace lagrange {

template <typename MeshType>
void compute_corner_normal(
    MeshType& mesh,
    std::function<bool(typename MeshType::Index, typename MeshType::Index)> is_sharp,
    std::function<bool(typename MeshType::Index)> is_cone_vertex)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");

    using AttributeArray = typename MeshType::AttributeArray;
    using Index = typename MeshType::Index;
    using Scalar = typename MeshType::Scalar;

    if (!mesh.has_facet_attribute("normal")) {
        compute_triangle_normal(mesh);
    }
    if (!mesh.is_connectivity_initialized()) {
        mesh.initialize_connectivity();
    }
    if (!mesh.is_edge_data_initialized()) {
        mesh.initialize_edge_data();
    }

    const Index dim = mesh.get_dim();
    const Index num_vertices = mesh.get_num_vertices();
    const Index num_facets = mesh.get_num_facets();
    const Index vertex_per_facet = mesh.get_vertex_per_facet();
    const auto& facets = mesh.get_facets();
    const AttributeArray& facet_normals = mesh.get_facet_attribute("normal");
    // Compute corner angles
    AttributeArray corner_angles;
    {
        igl::internal_angles(mesh.get_vertices(), mesh.get_facets(), corner_angles);
        assert(corner_angles.rows() == facets.rows());
        assert(corner_angles.cols() == facets.cols());
    }


    AttributeArray corner_normals = AttributeArray::Zero(num_facets * vertex_per_facet, dim);

    auto index_of = [](const auto& data, const auto item) -> Index {
        auto itr = std::find(data.begin(), data.end(), item);
        assert(itr != data.end());
        return safe_cast<Index>(itr - data.begin());
    };

    auto get_root = [](const auto& data, Index i) -> Index {
        while (data[i] != i) {
            assert(data[i] < i);
            i = data[i];
        }
        return i;
    };

    auto get_corner_index =
        [vertex_per_facet, &facets](const Index fid, const Index vid) -> Index {
        for (auto i : range(vertex_per_facet)) {
            if (facets(fid, i) == vid) {
                return fid * vertex_per_facet + i;
            }
        }
        LA_ASSERT(false, "Facet does not contain this vertex");
        return 0; // To avoid compiler warning.
    };

    auto compute_corner_normal_around_cone_vertex =
        [&mesh, &get_corner_index, &corner_normals, &facet_normals](Index vi) {
            const auto& adj_facets = mesh.get_facets_adjacent_to_vertex(vi);
            for (const auto fi : adj_facets) {
                const auto ci = get_corner_index(fi, vi);
                corner_normals.row(ci) = facet_normals.row(fi);
            }
        };

    std::vector<Index> facet_ids; // 1-ring union-find array.
    std::vector<Index> e_fids; // per-edge facet id buffer.
    auto compute_corner_normal_around_regular_vertex = [&](Index vi) {
        const auto& adj_facets = mesh.get_facets_adjacent_to_vertex(vi);
        const auto num_adj_facets = adj_facets.size();
        const auto& adj_vertices = mesh.get_vertices_adjacent_to_vertex(vi);

        // Initialize union find array.
        facet_ids.resize(num_adj_facets);
        std::iota(facet_ids.begin(), facet_ids.end(), 0);
        auto get_1_ring_root = [&get_root, &facet_ids](Index idx) {
            return get_root(facet_ids, idx);
        };

        for (auto vj : adj_vertices) {
            if (!is_sharp(vi, vj)) {
                const auto& e_facets = mesh.get_edge_adjacent_facets({vi, vj});
                e_fids.resize(e_facets.size());
                std::transform(
                    e_facets.begin(),
                    e_facets.end(),
                    e_fids.begin(),
                    [&index_of, &adj_facets](Index fid) { return index_of(adj_facets, fid); });
                std::transform(e_fids.begin(), e_fids.end(), e_fids.begin(), get_1_ring_root);
                const auto root_id = *std::min_element(e_fids.begin(), e_fids.end());
                std::for_each(e_fids.begin(), e_fids.end(), [&facet_ids, root_id](const Index fid) {
                    facet_ids[fid] = root_id;
                });
            }
        }

        std::transform(facet_ids.begin(), facet_ids.end(), facet_ids.begin(), get_1_ring_root);

        // Accumulate normal to root corner.
        for (auto i : range(num_adj_facets)) {
            const Index fid = adj_facets[i];
            const Index corner_id = get_corner_index(fid, vi);
            const Index root_fid = adj_facets[facet_ids[i]];
            const Index root_corner_id = get_corner_index(root_fid, vi);
            const Scalar corner_angle =
                corner_angles(corner_id / vertex_per_facet, corner_id % vertex_per_facet);
            if (corner_angle >= 0.f &&
                corner_angle <= 3.14f) { // filter numerical issues due to tiny or huge angels
                corner_normals.row(root_corner_id) += facet_normals.row(fid) * corner_angle;
            }
        }
        // Assign corner normal to each smooth group of the 1-ring.
        for (auto i : range(num_adj_facets)) {
            const Index fid = adj_facets[i];
            const Index root_fid = adj_facets[facet_ids[i]];
            const Index curr_corner_id = get_corner_index(fid, vi);
            const Index root_corner_id = get_corner_index(root_fid, vi);

            corner_normals.row(curr_corner_id) = corner_normals.row(root_corner_id);

            if (corner_normals.row(curr_corner_id).template lpNorm<Eigen::Infinity>() > 1.e-4) {
                corner_normals.row(curr_corner_id).normalize();
            } else {
                corner_normals.row(curr_corner_id).setZero();
            }
        }
    };

    for (auto vi : range(num_vertices)) {
        if (is_cone_vertex(vi)) {
            compute_corner_normal_around_cone_vertex(vi);
        } else {
            compute_corner_normal_around_regular_vertex(vi);
        }
    }

    mesh.add_corner_attribute("normal");
    mesh.import_corner_attribute("normal", corner_normals);
}

/**
 * Compute per-corner normal.  Keep surface smooth everywhere with dihedral
 * angle less than feature_angle_theshold.
 */
template <typename MeshType>
void compute_corner_normal(
    MeshType& mesh,
    const typename MeshType::Scalar feature_angle_threshold,
    const std::vector<typename MeshType::Index>& cone_vertices = {})
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");
    LA_ASSERT(feature_angle_threshold < 4, "This angle is in degrees, must be in radians");
    using Index = typename MeshType::Index;

    if (!mesh.is_edge_data_initialized()) {
        mesh.initialize_edge_data();
    }
    if (!mesh.has_edge_attribute("dihedral_angle")) {
        compute_dihedral_angles(mesh);
    }

    auto is_sharp = [&mesh, feature_angle_threshold](Index vi, Index vj) {
        assert(mesh.has_edge_attribute("dihedral_angle"));
        const auto& dihedral_angles = mesh.get_edge_attribute("dihedral_angle");
        Index eid = mesh.get_edge_index({vi, vj});
        return std::abs(dihedral_angles(eid, 0)) > feature_angle_threshold;
    };

    if (cone_vertices.empty()) {
        compute_corner_normal(mesh, is_sharp, [](Index) { return false; });
    } else {
        std::vector<bool> is_cone(mesh.get_num_vertices(), false);
        std::for_each(cone_vertices.begin(), cone_vertices.end(), [&is_cone](Index vi) {
            is_cone[vi] = true;
        });
        compute_corner_normal(mesh, is_sharp, [&is_cone](Index vi) { return is_cone[vi]; });
    }
}

} // namespace lagrange
