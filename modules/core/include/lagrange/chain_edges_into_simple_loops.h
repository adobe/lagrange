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
/// defined as a simple cycle with at most 1 vertex of degree > 2. The input digraph is assume to
/// not contain any "dangling" branch (vertices with degree_in != degree_out). Otherwise an empty
/// result is returned. If the graph cannot be pruned by removing ears, then the list of remaining
/// edges that cannot be pruned is returned in the remaining_edges output variable (e.g., a chain of
/// "8" that loops back to itself).
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
        logger().warn("Input digraph has dangling vertices.");
        return false;
    }

    // arc -> first edge in the arc
    std::vector<Index> arc_to_first_edge;

    // vertex -> single outgoing edge (or INVALID if degree_out is > 1)
    std::vector<Index> vertex_to_outgoing_edge(num_vertices, INVALID<Index>());

    // edge -> next edge along arc (or INVALID if last edge along arc)
    std::vector<Index> next_edge_along_arc(num_edges, INVALID<Index>());

    // Chain edges into arcs
    for (Index e = 0; e < num_edges; ++e) {
        Index v0 = edges(e, 0);
        if (degree_out[v0] > 1) {
            // New arc starting here
            arc_to_first_edge.push_back(e);
        } else {
            vertex_to_outgoing_edge[v0] = e;
        }
    }
    for (Index e = 0; e < num_edges; ++e) {
        Index v1 = edges(e, 1);
        next_edge_along_arc[e] = vertex_to_outgoing_edge[v1];
    }

    // Follow each arc until we reach the last edge
    std::vector<bool> edge_is_in_arc(num_edges, false);
    std::vector<Index> arc_to_last_edge(arc_to_first_edge.size());
    std::stack<Index> ears;
    std::vector<std::vector<Index>> arcs_in(num_vertices);
    std::vector<std::vector<Index>> arcs_out(num_vertices);
    std::vector<bool> arc_is_pending(arc_to_first_edge.size(), false);

    auto first_vertex_in_arc = [&](Index a) { return edges(arc_to_first_edge[a], 0); };

    auto last_vertex_in_arc = [&](Index a) { return edges(arc_to_last_edge[a], 1); };

    for (Index a = 0; a < (Index)arc_to_first_edge.size(); ++a) {
        // Follow edges along the arc
        for (Index e = arc_to_first_edge[a]; e != INVALID<Index>(); e = next_edge_along_arc[e]) {
            arc_to_last_edge[a] = e;
            edge_is_in_arc[e] = true;
        }

        const Index v_first = first_vertex_in_arc(a);
        const Index v_last = last_vertex_in_arc(a);

        // Compute outgoing and ingoing arcs per vertices of degree > 1
        arcs_out[v_first].push_back(a);
        arcs_in[v_last].push_back(a);

        if (v_first == v_last) {
            // Arc is an ear, will be popped next
            LA_ASSERT_DEBUG(arc_is_pending[a] == false);
            arc_is_pending[a] = true;
            ears.push(a);
        }
    }

    // For arcs which are already simple loops, there was no `arc_to_first_edge`.
    // We do an additional pass on all edges to make sure we didn't miss anyone.
    for (Index e = 0; e < num_edges; ++e) {
        if (!edge_is_in_arc[e]) {
            // Add a new arc starting here
            const Index a = (Index)arc_to_first_edge.size();
            arc_to_first_edge.emplace_back(e);
            arc_is_pending.push_back(false);

            // Follow edges along the arc and tag last edge on the arc
            arc_to_last_edge.emplace_back(e);
            edge_is_in_arc[e] = true;
            for (Index ei = next_edge_along_arc[e]; ei != e; ei = next_edge_along_arc[ei]) {
                arc_to_last_edge[a] = ei;
                edge_is_in_arc[ei] = true;
            }
            LA_ASSERT_DEBUG(next_edge_along_arc[arc_to_last_edge[a]] == e);
            LA_ASSERT_DEBUG(first_vertex_in_arc(a) == last_vertex_in_arc(a));
            next_edge_along_arc[arc_to_last_edge[a]] = INVALID<Index>();

            // This arc is an ear
            LA_ASSERT_DEBUG(arc_is_pending[a] == false);
            arc_is_pending[a] = true;
            ears.push(a);
        }
    }

    // Swap/pop_back trick to remove an element from a std::vector
    auto remove_from_vector = [](std::vector<Index> &vec, size_t idx) {
        LA_ASSERT_DEBUG(idx < vec.size());
        std::swap(vec[idx], vec.back());
        vec.pop_back();
    };

    // Pop ears repeatedly
    Index num_edges_removed = 0;
    std::vector<bool> edge_is_removed(num_edges, false);
    std::vector<bool> arc_is_removed(arc_to_first_edge.size(), false);
    while (!ears.empty()) {
        const Index a = ears.top();
        ears.pop();
        LA_ASSERT_DEBUG(!arc_is_removed[a]);
        arc_is_removed[a] = true;

        // Arc starts and ends on the same vertex, compute corresponding (simple) loop
        LA_ASSERT_DEBUG(first_vertex_in_arc(a) == last_vertex_in_arc(a));
        LA_ASSERT_DEBUG(arc_to_first_edge[a] != INVALID<Index>());
        std::vector<Index> loop;
        for (Index e = arc_to_first_edge[a]; e != INVALID<Index>(); e = next_edge_along_arc[e]) {
            loop.push_back(e);
            LA_ASSERT_DEBUG(edge_is_removed[e] == false);
            edge_is_removed[e] = true;
            ++num_edges_removed;
        }
        output.emplace_back(std::move(loop));

        // Remove current arc from the in/out arcs of the endoint vertex v
        const Index v = first_vertex_in_arc(a);
        LA_ASSERT_DEBUG(v == last_vertex_in_arc(a));
        for (size_t i = 0; i < arcs_out[v].size();) {
            if (arcs_out[v][i] == a) {
                remove_from_vector(arcs_out[v], i);
            } else {
                LA_ASSERT_DEBUG(!arc_is_removed[arcs_out[v][i]]);
                ++i;
            }
        }
        for (size_t i = 0; i < arcs_in[v].size();) {
            if (arcs_in[v][i] == a) {
                remove_from_vector(arcs_in[v], i);
            } else {
                LA_ASSERT_DEBUG(!arc_is_removed[arcs_in[v][i]]);
                ++i;
            }
        }
        LA_ASSERT_DEBUG(arcs_in[v].size() == arcs_out[v].size());

        // If there are only 1 remaining in/out arcs, join them
        if (arcs_in[v].size() == 1) {
            const Index a_in = arcs_in[v].front();
            const Index a_out = arcs_out[v].front();
            LA_ASSERT_DEBUG(last_vertex_in_arc(a_in) == v);
            LA_ASSERT_DEBUG(first_vertex_in_arc(a_out) == v);
            if (a_in != a_out) {
                // If the arcs are different, then join them
                LA_ASSERT_DEBUG(arc_to_first_edge[a_in] != INVALID<Index>());
                LA_ASSERT_DEBUG(arc_to_last_edge[a_in] != INVALID<Index>());
                LA_ASSERT_DEBUG(arc_to_first_edge[a_out] != INVALID<Index>());
                LA_ASSERT_DEBUG(arc_to_last_edge[a_out] != INVALID<Index>());
                LA_ASSERT_DEBUG(next_edge_along_arc[arc_to_last_edge[a_in]] == INVALID<Index>());
                LA_ASSERT_DEBUG(
                    edges(arc_to_last_edge[a_in], 1) == edges(arc_to_first_edge[a_out], 0));

                // Replace a_out by a_in in last_vertex_in_arc(a_out)
                for (auto &ai : arcs_in[last_vertex_in_arc(a_out)]) {
                    if (ai == a_out) {
                        ai = a_in;
                    }
                }

                // Update chain to joint a_in --> a_out
                next_edge_along_arc[arc_to_last_edge[a_in]] = arc_to_first_edge[a_out];
                arc_to_last_edge[a_in] = arc_to_last_edge[a_out];

                // Cleaning up
                arc_to_first_edge[a_out] = INVALID<Index>();
                arc_to_last_edge[a_out] = INVALID<Index>();
                arc_is_removed[a_out] = true;
                arcs_in[v].clear();
                arcs_out[v].clear();
            }
            const Index v_first = first_vertex_in_arc(a_in);
            const Index v_last = last_vertex_in_arc(a_in);
            LA_ASSERT_DEBUG(!arc_is_removed[a_in]);
            if (v_first == v_last && !arc_is_pending[a_in]) {
                arc_is_pending[a_in] = true;
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
