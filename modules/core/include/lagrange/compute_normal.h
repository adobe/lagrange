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

#include <numeric>
#include <vector>

#include <lagrange/DisjointSets.h>
#include <lagrange/Mesh.h>
#include <lagrange/chain_corners_around_edges.h>
#include <lagrange/chain_corners_around_vertices.h>
#include <lagrange/common.h>
#include <lagrange/compute_triangle_normal.h>
#include <lagrange/corner_to_edge_mapping.h>
#include <lagrange/utils/geometry3d.h>

namespace lagrange {

template <typename MeshType>
void compute_normal(
    MeshType& mesh,
    const typename MeshType::Scalar feature_angle_threshold,
    const std::vector<typename MeshType::Index>& cone_vertices = {})
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");
    using Scalar = typename MeshType::Scalar;
    using Index = typename MeshType::Index;
    using FacetArray = typename MeshType::FacetArray;
    using AttributeArray = typename MeshType::AttributeArray;
    using IndexArray = Eigen::Matrix<Index, Eigen::Dynamic, 1>;
    using RowVector3r = Eigen::Matrix<Scalar, 1, 3>;

    LA_ASSERT(mesh.get_vertex_per_facet() == 3, "Only triangle meshes are supported for this.");
    if (!mesh.has_facet_attribute("normal")) {
        compute_triangle_normal(mesh);
    }

    // Compute edge information
    logger().trace("Corner to edge mapping");
    IndexArray c2e;
    corner_to_edge_mapping(mesh.get_facets(), c2e);
    logger().trace("Chain corners around edges");
    IndexArray e2c;
    IndexArray next_corner_around_edge;
    chain_corners_around_edges(mesh.get_facets(), c2e, e2c, next_corner_around_edge);
    logger().trace("Chain corners around vertices");
    IndexArray v2c;
    IndexArray next_corner_around_vertex;
    chain_corners_around_vertices(
        mesh.get_num_vertices(),
        mesh.get_facets(),
        v2c,
        next_corner_around_vertex);

    const auto& vertices = mesh.get_vertices();
    const auto& facets = mesh.get_facets();
    const auto& facet_normals = mesh.get_facet_attribute("normal");
    const auto nvpf = mesh.get_vertex_per_facet();
    const auto num_vertices = mesh.get_num_vertices();
    const auto num_corners = mesh.get_num_facets() * nvpf;

    std::vector<bool> is_cone_vertex(num_vertices, false);
    for (auto vi : cone_vertices) {
        is_cone_vertex[vi] = true;
    }

    // Assumes fi and fj are adjacent facets.
    auto is_edge_smooth = [&](Index fi, Index fj) {
        const RowVector3r ni = facet_normals.row(fi);
        const RowVector3r nj = facet_normals.row(fj);
        const auto theta = angle_between(ni, nj);
        return theta < feature_angle_threshold;
    };

    // Check if two face vertices are collapsed
    auto is_face_degenerate = [&](Index f) {
        for (Index lv = 0; lv < nvpf; ++lv) {
            if (facets(f, lv) == facets(f, (lv + 1) % nvpf)) {
                return true;
            }
        }
        return false;
    };

    // STEP 1: For each vertex v, iterate over each pair of incident facet (fi, fj) that share a
    // common edge eij.
    logger().trace("Loop to unify corner indices");
    DisjointSets<Index> unified_indices(num_corners);
    tbb::parallel_for(Index(0), num_vertices, [&](Index v) {
        for (Index ci = v2c[v]; ci != INVALID<Index>(); ci = next_corner_around_vertex[ci]) {
            Index eij = c2e[ci];
            Index fi = ci / nvpf;
            Index lvi = ci % nvpf;
            Index vi = facets(fi, lvi);
            assert(vi == v);
            if (is_cone_vertex[vi]) continue;
            if (is_face_degenerate(fi)) continue;

            for (Index cj = e2c[eij]; cj != INVALID<Index>(); cj = next_corner_around_edge[cj]) {
                Index fj = cj / nvpf;
                Index lvj = cj % nvpf;
                if (fi == fj) continue;
                Index vj = facets(fj, lvj);
                if (vi != vj) {
                    lvj = (lvj + 1) % nvpf;
                    vj = facets(fj, lvj);
                    assert(vi == vj);
                }

                if (is_edge_smooth(fi, fj)) {
                    unified_indices.merge(ci, fj * nvpf + lvj);
                }
            }
        }
    });

    // STEP 2: Perform averaging and reindex attribute
    logger().trace("Compute new indices");
    std::vector<Index> repr(num_corners, INVALID<Index>());
    Index num_indices = 0;
    for (Index n = 0; n < num_corners; ++n) {
        Index r = unified_indices.find(n);
        if (repr[r] == INVALID<Index>()) {
            repr[r] = num_indices++;
        }
        repr[n] = repr[r];
    }
    logger().trace("Compute offsets");
    std::vector<Index> indices(num_corners);
    std::vector<Index> offsets(num_indices + 1, 0);
    for (Index c = 0; c < num_corners; ++c) {
        offsets[repr[c] + 1]++;
    }
    for (Index r = 1; r <= num_indices; ++r) {
        offsets[r] += offsets[r - 1];
    }
    {
        // Bucket sort for corner indices
        std::vector<Index> counter = offsets;
        for (Index c = 0; c < num_corners; ++c) {
            indices[counter[repr[c]]++] = c;
        }
    }

    logger().trace("Project and average normals");
    FacetArray normal_indices(mesh.get_num_facets(), 3);
    AttributeArray normal_values(num_indices, mesh.get_dim());
    normal_values.setZero();
    tbb::parallel_for(Index(0), Index(offsets.size() - 1), [&](Index r) {
        for (Index i = offsets[r]; i < offsets[r + 1]; ++i) {
            Index c = indices[i];
            Index f = c / nvpf;
            Index lv = c % nvpf;
            assert(repr[c] == r);

            RowVector3r n = facet_normals.row(f);

            // Compute weight as the angle between the (projected) edges
            RowVector3r v0 = vertices.row(facets(f, lv));
            RowVector3r v1 = vertices.row(facets(f, (lv + 1) % nvpf));
            RowVector3r v2 = vertices.row(facets(f, (lv + 2) % nvpf));
            RowVector3r e01 = v1 - v0;
            RowVector3r e02 = v2 - v0;
            Scalar w = angle_between(e01, e02);

            normal_values.row(r) += n * w;
            normal_indices(f, lv) = r;
        }
    });
    logger().trace("Normalize vectors");
    tbb::parallel_for(Index(0), Index(normal_values.rows()), [&](Index c) {
        normal_values.row(c).template head<3>().stableNormalize();
    });

    mesh.add_indexed_attribute("normal");
    mesh.set_indexed_attribute("normal", normal_values, normal_indices);
}

} // namespace lagrange
