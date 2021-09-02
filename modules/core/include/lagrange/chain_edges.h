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

#include <list>
#include <unordered_set>
#include <vector>

#include <lagrange/Edge.h>
#include <lagrange/common.h>
#include <lagrange/utils/warning.h>

namespace lagrange {
/**
 * Chain edges into either simple linear chain or simple loops.
 * A simple use case is to input the rim edges around a vertex.  This method
 * will return the boundary loops of the 1-ring neighborhood.  If the vertex
 * is locally manifold, only a single chain will be returned.
 *
 * Note that if the edges forms a complex graphs with nodes of valence more
 * than 2, the extracted chains may not be simple.
 */
template <typename Index, typename Allocator = std::list<EdgeType<Index>>>
std::vector<std::list<Index>> chain_edges(const Allocator& edges)
{
    using VertexAdjList = std::unordered_map<Index, Index>;

    using Chain = std::list<Index>;
    std::vector<Chain> chains;
    VertexAdjList next;
    VertexAdjList prev;
    next.reserve(edges.size());
    prev.reserve(edges.size());
    std::unordered_set<Index> visited;
    visited.reserve(edges.size() * 2);

LA_IGNORE_RANGE_LOOP_ANALYSIS_BEGIN
    for (const auto& e : edges) {
        next[e[0]] = e[1];
        prev[e[1]] = e[0];
    }
LA_IGNORE_RANGE_LOOP_ANALYSIS_END

    /**
     * A more readable handy method.
     */
    auto has_visited = [&visited](Index id) -> bool { return visited.find(id) != visited.end(); };

    /**
     * Extend a chain of vertices from the end based on adjacency list
     * stored in next.
     */
    auto grow_chain_forward = [&visited, &next, &has_visited](Chain& chain) -> void {
        // Extend chain from the end.
        assert(chain.size() > 0);
        while (true) {
            const auto curr_v = chain.back();
            const auto itr = next.find(curr_v);
            if (itr != next.end() && !has_visited(itr->second)) {
                chain.push_back(itr->second);
                visited.insert(itr->second);
            } else {
                break;
            }
        }
    };

    /**
     * Extend a chain of vertices from the front based on adjacency list
     * stored in prev.
     */
    auto grow_chain_backward = [&visited, &prev, &has_visited](Chain& chain) -> void {
        // Extend chain from the beginning.
        assert(chain.size() > 0);
        while (true) {
            const auto curr_v = chain.front();
            const auto itr = prev.find(curr_v);
            if (itr != prev.end() && !has_visited(itr->second)) {
                chain.push_front(itr->second);
                visited.insert(itr->second);
            } else {
                break;
            }
        }
    };

LA_IGNORE_RANGE_LOOP_ANALYSIS_BEGIN
    for (const auto& e : edges) {
        if (!has_visited(e[0])) {
            chains.emplace_back();
            auto& chain = chains.back();
            chain.push_back(e[0]);
            visited.insert(e[0]);

            grow_chain_forward(chain);
            grow_chain_backward(chain);
        }
    }
LA_IGNORE_RANGE_LOOP_ANALYSIS_END
    return chains;
}
} // namespace lagrange
