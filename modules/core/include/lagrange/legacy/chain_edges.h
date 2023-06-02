/*
 * Copyright 2018 Adobe. All rights reserved.
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

#include <list>
#include <unordered_set>
#include <utility>
#include <vector>

#include <lagrange/Edge.h>
#include <lagrange/common.h>
#include <lagrange/legacy/inline.h>
#include <lagrange/utils/range.h>
#include <lagrange/utils/warning.h>

namespace lagrange {
LAGRANGE_LEGACY_INLINE
namespace legacy {
/**
 * Chain directed edges into either simple linear chain or simple loops.
 *
 * A simple use case is to input the rim edges around a vertex.  This method
 * will return the boundary loops of the 1-ring neighborhood.  If the vertex
 * is locally manifold, only a single chain will be returned.
 *
 * @param[in]  edges       The set of input edges.
 * @param[in]  close_loop  Whether to mark closed loops by setting the first and
 *                         last vertex to be the same.
 *
 * @returns The set of edge chains/loops.
 *
 * @note If the edges forms a complex graphs with nodes of valence more
 * than 2, the extracted chains may not be simple.
 */
template <typename Index, typename Allocator = std::list<EdgeType<Index>>>
std::vector<std::list<Index>> chain_edges(const Allocator& edges, bool close_loop = false)
{
    using EdgeIterator = typename Allocator::const_iterator;
    using VertexEdgeAdjList = std::unordered_map<Index, std::vector<EdgeIterator>>;

    using Chain = std::list<Index>;
    std::vector<Chain> chains;
    VertexEdgeAdjList next;
    VertexEdgeAdjList prev;
    size_t num_edges = std::distance(edges.begin(), edges.end());
    next.reserve(num_edges);
    prev.reserve(num_edges);
    std::vector<bool> visited(num_edges, false);

    auto insert_or_append = [&](VertexEdgeAdjList& adj_list, Index key, EdgeIterator value) {
        auto itr = adj_list.find(key);
        if (itr == adj_list.end()) {
            adj_list.insert({key, {std::move(value)}});
        } else {
            itr->second.push_back(std::move(value));
        }
    };

    for (auto itr = edges.begin(); itr != edges.end(); ++itr) {
        const auto& e = *itr;
        insert_or_append(next, e[0], itr);
        insert_or_append(prev, e[1], itr);
    }

    /**
     * Extend a chain of vertices from the end based on adjacency list
     * stored in next.
     */
    auto grow_chain_forward = [&](Chain& chain) -> void {
        // Extend chain from the end.
        assert(chain.size() > 0);
        while (true) {
            const auto curr_v = chain.back();
            const auto itr = next.find(curr_v);
            if (itr != next.end()) {
                const auto& next_edges = itr->second;
                if (next_edges.size() != 1) break;

                auto e_itr = next_edges.front();
                auto eid = std::distance(edges.begin(), e_itr);
                if (visited[eid]) break;

                const auto& e = *e_itr;
                la_debug_assert(e[0] == curr_v);
                chain.push_back(e[1]);
                visited[eid] = true;
            } else {
                break;
            }
        }
    };

    /**
     * Extend a chain of vertices from the front based on adjacency list
     * stored in prev.
     */
    auto grow_chain_backward = [&](Chain& chain) -> void {
        // Extend chain from the beginning.
        assert(chain.size() > 0);
        while (true) {
            const auto curr_v = chain.front();
            const auto itr = prev.find(curr_v);
            if (itr != prev.end()) {
                const auto& prev_edges = itr->second;
                if (prev_edges.size() != 1) break;

                auto e_itr = prev_edges.front();
                auto eid = std::distance(edges.begin(), e_itr);
                if (visited[eid]) break;

                const auto& e = *e_itr;
                la_debug_assert(e[1] == curr_v);
                chain.push_front(e[0]);
                visited[eid] = true;
            } else {
                break;
            }
        }
    };

    size_t eid = 0;
    for (auto itr = edges.begin(); itr != edges.end(); ++itr, eid++) {
        if (visited[eid]) continue;

        const auto& e = *itr;
        Chain chain;
        chain.push_back(e[0]);
        chain.push_back(e[1]);
        visited[eid] = true;

        grow_chain_forward(chain);
        grow_chain_backward(chain);

        if (!close_loop && chain.back() == chain.front()) {
            chain.pop_back();
        }
        chains.push_back(std::move(chain));
    }
    return chains;
}

namespace _detail {

// Eigen integer matrix with 2 columns, one edge per row
template <typename EdgeArray>
size_t get_num_edges(const EdgeArray& edges)
{
    return static_cast<size_t>(edges.rows());
}

// std::vector of edges
template <typename EdgeType>
size_t get_num_edges(const std::vector<EdgeType>& edges)
{
    return edges.size();
}

// Eigen integer matrix with 2 columns, one edge per row
template <typename VertexIndex, typename EdgeArray, typename EdgeIndex>
std::pair<VertexIndex, VertexIndex> get_edge_endpoints(EdgeArray& edges, EdgeIndex edge_index)
{
    return std::make_pair(
        static_cast<VertexIndex>(edges(static_cast<Eigen::Index>(edge_index), 0)),
        static_cast<VertexIndex>(edges(static_cast<Eigen::Index>(edge_index), 1)));
}

// std::vector of edges
template <typename VertexIndex, typename EdgeType, typename EdgeIndex>
std::pair<VertexIndex, VertexIndex> get_edge_endpoints(
    const std::vector<EdgeType>& edges,
    EdgeIndex edge_index)
{
    const auto& e = edges[static_cast<size_t>(edge_index)];
    return std::make_pair(static_cast<VertexIndex>(e[0]), static_cast<VertexIndex>(e[1]));
}

} // namespace _detail

/**
 * Chain undirected edges into chains and loops.
 *
 * @param[in]  edges       The set of input undirected edges. Can be a `std::vector` of 2D integer
 *                         vectors, or an Eigen integer array with 2 columns (one edge per row).
 * @param[in]  close_loop  Whether to mark closed loops by setting the first and
 *                         last vertex to be the same.
 *
 * @returns The set of edge chains/loops.
 *
 * @note Any vertices with more than 2 connected edges will serve as stopping vertices for the chain
 * growing algorithm.
 */
template <typename Index, typename Allocator = std::vector<EdgeType<Index>>>
std::vector<std::vector<Index>> chain_undirected_edges(
    const Allocator& edges,
    bool close_loop = false)
{
    using VertexEdgeAdjList = std::unordered_map<Index, std::vector<Index>>;

    using Chain = std::vector<Index>;
    std::vector<Chain> chains;
    VertexEdgeAdjList adj_list;
    size_t num_edges = _detail::get_num_edges(edges);
    adj_list.reserve(num_edges);
    std::vector<bool> visited(num_edges, false);

    auto add_adj_connection = [&](Index ei) {
        auto v = _detail::get_edge_endpoints<Index>(edges, ei);

        auto itr = adj_list.find(v.first);
        if (itr != adj_list.end()) {
            itr->second.push_back(ei);
        } else {
            adj_list.insert({v.first, {ei}});
        }

        itr = adj_list.find(v.second);
        if (itr != adj_list.end()) {
            itr->second.push_back(ei);
        } else {
            adj_list.insert({v.second, {ei}});
        }
    };

    LA_IGNORE_RANGE_LOOP_ANALYSIS_BEGIN
    for (auto ei : range(num_edges)) {
        add_adj_connection(ei);
    }
    LA_IGNORE_RANGE_LOOP_ANALYSIS_END

    auto grow_chain_forward = [&](Chain& chain) {
        Index curr_v = chain.back();

        while (true) {
            const auto& adj_edges = adj_list[curr_v];
            if (adj_edges.size() != 2) break;

            bool updated = false;
            for (auto ei : adj_edges) {
                if (visited[ei]) continue;

                auto v = _detail::get_edge_endpoints<Index>(edges, ei);
                if (v.first == curr_v) {
                    chain.push_back(v.second);
                } else {
                    la_debug_assert(v.second == curr_v);
                    chain.push_back(v.first);
                }
                curr_v = chain.back();
                visited[ei] = true;
                updated = true;
            }
            if (!updated) break;
        }
    };

    auto grow_chain_backward = [&](Chain& chain) {
        std::reverse(chain.begin(), chain.end());
        grow_chain_forward(chain);
        std::reverse(chain.begin(), chain.end());
    };

    for (auto ei : range(num_edges)) {
        if (visited[ei]) continue;

        auto v = _detail::get_edge_endpoints<Index>(edges, ei);
        visited[ei] = true;
        Chain chain;
        chain.reserve(num_edges);
        chain.push_back(v.first);
        chain.push_back(v.second);
        grow_chain_forward(chain);
        grow_chain_backward(chain);

        if (!close_loop && chain.front() == chain.back()) {
            chain.pop_back();
        }
        chains.push_back(std::move(chain));
    }

    return chains;
}

} // namespace legacy
} // namespace lagrange
