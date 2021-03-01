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

#include <lagrange/Logger.h>
#include <lagrange/common.h>

#include <Eigen/Dense>

#include <fstream>
#include <stack>
#include <string>
#include <vector>

namespace lagrange {

///
/// Chain edges into simple loops by cutting "ears" progressively from the digraph. An ear is
/// defined as a simple cycle with at most 1 vertex of degree > 2. The input digraph can contain
/// "dangling" vertices (vertices with degree_in != degree_out). If the graph cannot be pruned by
/// removing ears, then the list of remaining edges that cannot be pruned is returned in the
/// remaining_edges output variable (e.g., a chain of "8" that loops back to itself).
///
/// @param[in]  edges            #EI x 2 array of oriented edges in the input digraph.
/// @param[out] output           Output list of loops. Each loop is an array of edge indices.
/// @param[out] remaining_edges  #EO x 2 array of oriented edges that could not be pruned.
///
/// @tparam     DerivedE         Input edge matrix type.
/// @tparam     DerivedO         Output edge matrix type.
///
/// @return     True in case of success, False otherwise.
///
template <typename DerivedE, typename DerivedO>
bool chain_edges_into_simple_loops(
    const Eigen::MatrixBase<DerivedE> &edges,
    std::vector<std::vector<typename DerivedE::Scalar>> &output,
    Eigen::PlainObjectBase<DerivedO> &remaining_edges)
{
    using Index = typename DerivedE::Scalar;
    using IndexArray = Eigen::Matrix<Index, Eigen::Dynamic, 1>;

    output.clear();

    if (edges.rows() == 0) {
        // Empty graph
        return true;
    }

    Index num_vertices = edges.maxCoeff() + 1;
    Index num_edges = (Index)edges.rows();

    // Count degree_in and degree_out, and make sure that it matches
    IndexArray degree_in(num_vertices);
    IndexArray degree_out(num_vertices);
    degree_in.setZero();
    degree_out.setZero();
    for (Index e = 0; e < num_edges; ++e) {
        Index v0 = edges(e, 0);
        Index v1 = edges(e, 1);
        degree_out[v0]++;
        degree_in[v1]++;
    }
    if (!(degree_in.array() == degree_out.array()).all()) {
        logger().debug("Input digraph has dangling vertices.");
    }

    // path -> first edge in the path
    std::vector<Index> path_to_first_edge;

    // vertex -> single outgoing edge along path (or INVALID if degree_out is > 1)
    std::vector<Index> vertex_to_outgoing_edge(num_vertices, INVALID<Index>());

    // edge -> next edge along path (or INVALID if last edge along path)
    std::vector<Index> next_edge_along_path(num_edges, INVALID<Index>());

    // Chain edges into paths
    for (Index e = 0; e < num_edges; ++e) {
        Index v0 = edges(e, 0);
        if (degree_out[v0] == 1 && degree_in[v0] == 1) {
            // v0 is a mid-path vertex. There is only one possibility for the next edge along a path
            // going through v0.
            vertex_to_outgoing_edge[v0] = e;
        } else {
            // v0 is a junction vertex. We start one path for each outgoing edge e starting from
            // vertex v0.
            path_to_first_edge.push_back(e);
        }
    }
    // Chain edges together based on the vertex -> outgoing edge mapping we previously computed
    for (Index e = 0; e < num_edges; ++e) {
        Index v1 = edges(e, 1);
        next_edge_along_path[e] = vertex_to_outgoing_edge[v1];
    }

    // Follow each path until we reach the last edge
    std::vector<Index> edge_label(num_edges, INVALID<Index>());
    std::vector<Index> path_to_last_edge(path_to_first_edge.size());
    std::stack<Index> ears;
    std::vector<std::vector<Index>> paths_in(num_vertices);
    std::vector<std::vector<Index>> paths_out(num_vertices);
    std::vector<bool> path_is_pending(path_to_first_edge.size(), false);

    auto first_vertex_in_path = [&](Index a) { return edges(path_to_first_edge[a], 0); };

    auto last_vertex_in_path = [&](Index a) { return edges(path_to_last_edge[a], 1); };

    // For each path that we have started at a junction vertex, follow edges along the path until
    // each edge has been labeled as belonging to the path. Note that our paths do not contain
    // junction vertices by construction. So the only case where the path can contain a cycle is
    // when the path starts and ends at the same vertex.
    for (Index a = 0; a < (Index)path_to_first_edge.size(); ++a) {
        // Follow edges along the path and label them as belonging to the path
        for (Index e = path_to_first_edge[a];
             e != INVALID<Index>() && edge_label[e] == INVALID<Index>();
             e = next_edge_along_path[e]) {
            edge_label[e] = a;
            path_to_last_edge[a] = e;
        }

        const Index v_first = first_vertex_in_path(a);
        const Index v_last = last_vertex_in_path(a);

        // Compute outgoing and ingoing paths per vertices of degree > 1
        paths_out[v_first].push_back(a);
        paths_in[v_last].push_back(a);

        if (v_first == v_last) {
            // Path is an ear (simple loop), will be popped next
            LA_ASSERT_DEBUG(path_is_pending[a] == false);
            path_is_pending[a] = true;
            ears.push(a);
        }
    }

    // For paths which are isolated cycles, there is no "starting vertex" (each vertex has total
    // degree 2). We do an additional pass on each edge and start a new path for each unlabeled
    // edge.
    for (Index e = 0; e < num_edges; ++e) {
        if (edge_label[e] == INVALID<Index>()) {
            // Add a new path starting here
            const Index a = (Index)path_to_first_edge.size();
            path_to_first_edge.emplace_back(e);
            path_to_last_edge.emplace_back(e);
            path_is_pending.push_back(false);

            // Follow edges along the path and record the last edge on the path
            edge_label[e] = a;
            for (Index ei = next_edge_along_path[e];
                 ei != INVALID<Index>() && edge_label[ei] == INVALID<Index>();
                 ei = next_edge_along_path[ei]) {
                edge_label[ei] = a;
                path_to_last_edge[a] = ei;
            }
            LA_ASSERT_DEBUG(next_edge_along_path[path_to_last_edge[a]] == e);
            LA_ASSERT_DEBUG(first_vertex_in_path(a) == last_vertex_in_path(a));
            next_edge_along_path[path_to_last_edge[a]] = INVALID<Index>();

            // Path is an isolated cycle
            LA_ASSERT_DEBUG(path_is_pending[a] == false);
            path_is_pending[a] = true;
            ears.push(a);
        }
    }

    // Swap/pop_back trick to remove an element from a std::vector
    auto remove_from_vector = [](std::vector<Index> &vec, size_t idx) {
        LA_ASSERT_DEBUG(idx < vec.size());
        std::swap(vec[idx], vec.back());
        vec.pop_back();
    };

    Index num_edges_removed = 0;
    std::vector<bool> edge_is_removed(num_edges, false);

    // Pop ears repeatedly
    std::vector<bool> path_is_removed(path_to_first_edge.size(), false);
    while (!ears.empty()) {
        const Index a = ears.top();
        ears.pop();
        LA_ASSERT_DEBUG(!path_is_removed[a]);
        path_is_removed[a] = true;

        // path starts and ends on the same vertex, compute corresponding (simple) loop
        LA_ASSERT_DEBUG(first_vertex_in_path(a) == last_vertex_in_path(a));
        LA_ASSERT_DEBUG(path_to_first_edge[a] != INVALID<Index>());
        std::vector<Index> loop;
        for (Index e = path_to_first_edge[a]; e != INVALID<Index>(); e = next_edge_along_path[e]) {
            loop.push_back(e);
            LA_ASSERT_DEBUG(edge_is_removed[e] == false);
            edge_is_removed[e] = true;
            ++num_edges_removed;
        }
        output.emplace_back(std::move(loop));

        // Remove current path from the in/out paths of the endpoint vertex v
        const Index v = first_vertex_in_path(a);
        LA_ASSERT_DEBUG(v == last_vertex_in_path(a));
        for (size_t i = 0; i < paths_out[v].size();) {
            if (paths_out[v][i] == a) {
                remove_from_vector(paths_out[v], i);
            } else {
                LA_ASSERT_DEBUG(!path_is_removed[paths_out[v][i]]);
                ++i;
            }
        }
        for (size_t i = 0; i < paths_in[v].size();) {
            if (paths_in[v][i] == a) {
                remove_from_vector(paths_in[v], i);
            } else {
                LA_ASSERT_DEBUG(!path_is_removed[paths_in[v][i]]);
                ++i;
            }
        }

        // If there are only 1 remaining in/out paths, join them
        if (paths_in[v].size() == 1 && paths_out[v].size() == 1) {
            const Index a_in = paths_in[v].front();
            const Index a_out = paths_out[v].front();
            LA_ASSERT_DEBUG(last_vertex_in_path(a_in) == v);
            LA_ASSERT_DEBUG(first_vertex_in_path(a_out) == v);
            if (a_in != a_out) {
                // If the paths are different, then join them
                LA_ASSERT_DEBUG(path_to_first_edge[a_in] != INVALID<Index>());
                LA_ASSERT_DEBUG(path_to_last_edge[a_in] != INVALID<Index>());
                LA_ASSERT_DEBUG(path_to_first_edge[a_out] != INVALID<Index>());
                LA_ASSERT_DEBUG(path_to_last_edge[a_out] != INVALID<Index>());
                LA_ASSERT_DEBUG(next_edge_along_path[path_to_last_edge[a_in]] == INVALID<Index>());
                LA_ASSERT_DEBUG(
                    edges(path_to_last_edge[a_in], 1) == edges(path_to_first_edge[a_out], 0));

                // Replace a_out by a_in in last_vertex_in_path(a_out)
                for (auto &ai : paths_in[last_vertex_in_path(a_out)]) {
                    if (ai == a_out) {
                        ai = a_in;
                    }
                }

                // Update chain to joint a_in --> a_out
                next_edge_along_path[path_to_last_edge[a_in]] = path_to_first_edge[a_out];
                path_to_last_edge[a_in] = path_to_last_edge[a_out];

                // Cleaning up
                path_to_first_edge[a_out] = INVALID<Index>();
                path_to_last_edge[a_out] = INVALID<Index>();
                path_is_removed[a_out] = true;
                paths_in[v].clear();
                paths_out[v].clear();
            }
            const Index v_first = first_vertex_in_path(a_in);
            const Index v_last = last_vertex_in_path(a_in);
            LA_ASSERT_DEBUG(!path_is_removed[a_in]);
            if (v_first == v_last && !path_is_pending[a_in]) {
                path_is_pending[a_in] = true;
                ears.push(a_in);
            }
        }
    }

    if (num_edges_removed != num_edges) {
        logger().warn(
            "Removing ears didn't result in an empty graph, number of edges remaining: {}",
            num_edges - num_edges_removed);
        Index true_num_removed = 0;
        for (auto b : edge_is_removed) {
            if (b) {
                ++true_num_removed;
            }
        }
        LA_ASSERT_DEBUG(true_num_removed == num_edges_removed);
        remaining_edges.resize(num_edges - num_edges_removed, 2);
        for (Index e = 0, cnt = 0; e < num_edges; ++e) {
            if (!edge_is_removed[e]) {
                LA_ASSERT_DEBUG(cnt < remaining_edges.rows());
                remaining_edges.row(cnt++) = edges.row(e);
            }
        }
        return false;
    }

    return true;
}

} // namespace lagrange
