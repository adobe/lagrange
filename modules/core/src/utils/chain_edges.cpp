/*
 * Copyright 2023 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/AttributeTypes.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/chain_edges.h>
#include <lagrange/utils/invalid.h>
#include <lagrange/utils/span.h>

#include <array>
#include <vector>

namespace lagrange {

namespace detail {

template <typename Index>
Index num_edges(const span<const Index>& edges)
{
    return static_cast<Index>(edges.size() / 2);
}

template <typename Index>
Index num_vertices(const span<const Index>& edges)
{
    auto itr = std::max_element(edges.begin(), edges.end());
    if (itr == edges.end()) return 0;
    return *itr + 1;
}

template <typename Index>
span<const Index> get_edge(const span<const Index>& edges, Index i)
{
    return edges.subspan(i * 2, 2);
}

} // namespace detail


template <typename Index>
ChainEdgesResult<Index> chain_directed_edges(
    const span<const Index> edges,
    const ChainEdgesOptions& options)
{
    const Index num_edges = detail::num_edges<Index>(edges);
    const Index num_vertices = detail::num_vertices(edges);

    std::vector<Index> degree_in(num_vertices, 0);
    std::vector<Index> degree_out(num_vertices, 0);

    for (Index i = 0; i < num_edges; i++) {
        auto e = detail::get_edge(edges, i);
        degree_in[e[1]]++;
        degree_out[e[0]]++;
    }

    // path -> first edge in the path.
    std::vector<Index> path_to_first_edge;
    path_to_first_edge.reserve(num_edges);

    // vertex -> single outgoing edge along path (or INVALID if degree_out is > 1)
    std::vector<Index> vertex_to_outgoing_edge(num_vertices, invalid<Index>());

    // edge -> next edge along path (or INVALID if last edge along path)
    std::vector<Index> next_edge_along_path(num_edges, invalid<Index>());

    for (Index i = 0; i < num_edges; i++) {
        auto e = detail::get_edge(edges, i);
        auto v0 = e[0];

        if (degree_out[v0] == 1 && degree_in[v0] == 1) {
            // v0 is a mid-path vertex. There is only one possibility for next edge along a path
            // going through it.
            vertex_to_outgoing_edge[e[0]] = i;
        } else {
            // v0 is a junction vertex. We start one path for each outgoing edge e from vertex
            // v0.
            path_to_first_edge.push_back(i);
        }
    }
    for (Index i = 0; i < num_edges; i++) {
        auto e = detail::get_edge(edges, i);
        auto v1 = e[1];
        next_edge_along_path[i] = vertex_to_outgoing_edge[v1];
    }

    ChainEdgesResult<Index> result;
    auto& loops = result.loops;
    auto& chains = result.chains;

    auto get_first_vertex = [&](const std::vector<Index>& chain) {
        auto e = detail::get_edge(edges, chain.front());
        return e[0];
    };
    auto get_last_vertex = [&](const std::vector<Index>& chain) {
        auto e = detail::get_edge(edges, chain.back());
        return e[1];
    };

    std::vector<size_t> piece_indices(num_edges, invalid<size_t>());
    std::vector<std::vector<Index>> pieces;

    auto grow_chain = [&](Index e) {
        std::vector<Index> chain;
        while (e != invalid<Index>() && piece_indices[e] == invalid<size_t>()) {
            chain.push_back(e);
            piece_indices[e] = pieces.size();
            e = next_edge_along_path[e];
        }
        return chain;
    };

    for (auto e : path_to_first_edge) {
        la_debug_assert(e != invalid<Index>());
        la_debug_assert(piece_indices[e] == invalid<size_t>());
        pieces.push_back(grow_chain(e));
    }

    auto form_loops_and_chains = [&]() {
        bool changed = false;
        const size_t num_pieces = pieces.size();
        do {
            changed = false;
            // Extract simple loops.
            for (size_t i = 0; i < num_pieces; i++) {
                auto& piece = pieces[i];
                if (piece.empty()) continue;
                auto v_first = get_first_vertex(piece);
                auto v_last = get_last_vertex(piece);

                if (v_first == v_last) {
                    loops.push_back(std::move(piece));
                    piece.clear();
                    degree_out[v_first]--;
                    degree_in[v_first]--;
                    changed = true;
                } else {
                    if (degree_out[v_first] + degree_in[v_first] == 1 ||
                        degree_out[v_last] + degree_in[v_last] == 1) {
                        chains.push_back(std::move(piece));
                        piece.clear();
                        degree_out[v_first]--;
                        degree_in[v_last]--;
                        changed = true;
                    }
                }
            }

            // Update outgoing edge for updated degree 1 vertices.
            for (size_t i = 0; i < num_pieces; i++) {
                const auto& piece = pieces[i];
                if (piece.empty()) continue;
                auto v_first = get_first_vertex(piece);

                if (degree_out[v_first] == 1 &&
                    vertex_to_outgoing_edge[v_first] == invalid<Index>()) {
                    vertex_to_outgoing_edge[v_first] = piece.front();
                }
            }

            // Extract complex loops with multiple pieces.
            for (size_t i = 0; i < num_pieces; i++) {
                auto& piece = pieces[i];
                if (piece.empty()) continue;
                auto v_last = get_last_vertex(piece);

                while (degree_in[v_last] == 1 && degree_out[v_last] == 1) {
                    auto next_edge = vertex_to_outgoing_edge[v_last];
                    la_debug_assert(next_edge != invalid<Index>());
                    auto j = piece_indices[next_edge];
                    la_debug_assert(j != invalid<size_t>());
                    if (j == i) break;

                    auto& next_piece = pieces[j];
                    piece.insert(piece.end(), next_piece.begin(), next_piece.end());
                    next_piece.clear();
                    v_last = get_last_vertex(piece);
                }
            }
        } while (changed);
        for (size_t i = 0; i < num_pieces; i++) {
            auto& piece = pieces[i];
            if (piece.empty()) continue;
            chains.push_back(std::move(piece));
        }
    };
    form_loops_and_chains();

    // The unvisited edges are either disconnected simple chains or simple loops.

    // Extract simple chains.
    for (Index i = 0; i < num_edges; i++) {
        if (piece_indices[i] != invalid<size_t>()) continue;
        auto e = detail::get_edge(edges, i);
        Index v0 = e[0];
        if (degree_in[v0] == 0 && degree_out[v0] == 1) {
            chains.push_back(grow_chain(i));
        }
    }

    // Extract simple loops.
    for (Index i = 0; i < num_edges; i++) {
        if (piece_indices[i] != invalid<size_t>()) continue;
        loops.push_back(grow_chain(i));
    }

    if (!options.output_edge_index) {
        // Switch edge index to vertex index.
        // Chain's size will be increased by 1.
        for (auto& chain : chains) {
            Index next_vertex = invalid<Index>();
            for (auto& entry : chain) {
                auto e = detail::get_edge(edges, entry);
                entry = e[0];
                next_vertex = e[1];
            }
            chain.push_back(next_vertex);
        }
        for (auto& loop : loops) {
            for (auto& entry : loop) {
                auto e = detail::get_edge(edges, entry);
                entry = e[0];
            }
            if (options.close_loop_with_identical_vertices) {
                loop.push_back(loop.front());
            }
        }
    }

    return result;
}

template <typename Index>
ChainEdgesResult<Index> chain_undirected_edges(
    const span<const Index> edges,
    const ChainEdgesOptions& options)
{
    const Index num_edges = detail::num_edges<Index>(edges);
    const Index num_vertices = detail::num_vertices(edges);

    std::vector<Index> degree(num_vertices, 0);
    for (Index i = 0; i < num_edges; i++) {
        auto e = detail::get_edge(edges, i);
        degree[e[1]]++;
        degree[e[0]]++;
    }
    std::vector<Index> path_to_first_edge;
    path_to_first_edge.reserve(num_edges);
    std::vector<std::array<Index, 2>> vertex_to_outgoing_edge(
        num_vertices,
        {invalid<Index>(), invalid<Index>()});

    auto register_edge = [&](Index v, Index e) {
        auto& out_edges = vertex_to_outgoing_edge[v];
        if (out_edges[0] == invalid<Index>() || out_edges[0] == e) {
            out_edges[0] = e;
        } else {
            la_debug_assert(out_edges[1] == invalid<Index>() || out_edges[1] == e);
            out_edges[1] = e;
        }
    };

    for (Index i = 0; i < num_edges; i++) {
        auto e = detail::get_edge(edges, i);
        if (degree[e[0]] == 2) {
            register_edge(e[0], i);
        }
        if (degree[e[1]] == 2) {
            register_edge(e[1], i);
        }

        if (degree[e[0]] != 2 || degree[e[1]] != 2) {
            path_to_first_edge.push_back(i);
        }
    }

    ChainEdgesResult<Index> result;
    auto& loops = result.loops;
    auto& chains = result.chains;

    auto get_first_vertex = [&](const std::vector<Index>& chain) {
        la_debug_assert(chain.size() > 0);
        auto e0 = detail::get_edge(edges, chain.front());
        if (chain.size() == 1) return e0[0];
        auto e1 = detail::get_edge(edges, chain[1]);
        if (e0[0] == e1[0] || e0[0] == e1[1])
            return e0[1];
        else {
            la_debug_assert(e0[1] == e1[0] || e0[1] == e1[1]);
            return e0[0];
        }
    };

    auto get_last_vertex = [&](const std::vector<Index>& chain) {
        la_debug_assert(chain.size() > 0);
        auto e0 = detail::get_edge(edges, chain.back());
        if (chain.size() == 1) return e0[1];
        auto e1 = detail::get_edge(edges, chain[chain.size() - 2]);
        if (e0[0] == e1[0] || e0[0] == e1[1])
            return e0[1];
        else {
            la_debug_assert(e0[1] == e1[0] || e0[1] == e1[1]);
            return e0[0];
        }
    };

    auto get_next_edge = [&](Index e, Index v) -> std::array<Index, 2> {
        auto& out_edges = vertex_to_outgoing_edge[v];
        if (out_edges[0] == invalid<Index>() && out_edges[1] == invalid<Index>())
            return {invalid<Index>(), invalid<Index>()};

        if (out_edges[0] == e) {
            e = out_edges[1];
        } else {
            la_debug_assert(out_edges[1] == e);
            e = out_edges[0];
        }

        auto edge = detail::get_edge(edges, e);
        if (edge[0] == v) {
            return {e, edge[1]};
        } else {
            la_debug_assert(edge[1] == v);
            return {e, edge[0]};
        }
    };

    std::vector<size_t> piece_indices(num_edges, invalid<size_t>());
    std::vector<std::vector<Index>> pieces;

    auto grow_chain = [&](Index e, Index v) {
        std::vector<Index> chain;
        while (e != invalid<Index>() && piece_indices[e] == invalid<size_t>()) {
            chain.push_back(e);
            piece_indices[e] = pieces.size();
            auto r = get_next_edge(e, v);
            e = r[0];
            v = r[1];
        }
        return chain;
    };

    for (auto ei : path_to_first_edge) {
        if (piece_indices[ei] != invalid<size_t>()) continue;
        auto e = detail::get_edge(edges, ei);
        std::vector<Index> chain;
        if (degree[e[0]] != 2) {
            chain = grow_chain(ei, e[1]);
        } else {
            chain = grow_chain(ei, e[0]);
        }
        pieces.push_back(std::move(chain));
    }

    auto form_loops_and_chains = [&]() {
        bool changed = false;
        const size_t num_pieces = pieces.size();
        do {
            changed = false;
            // Extract simple ears and hanging chains.
            for (size_t i = 0; i < num_pieces; i++) {
                auto& piece = pieces[i];
                if (piece.empty()) continue;
                auto v_first = get_first_vertex(piece);
                auto v_last = get_last_vertex(piece);

                if (v_first == v_last) {
                    loops.push_back(std::move(piece));
                    piece.clear();
                    degree[v_first] -= 2;
                    changed = true;
                } else {
                    if (degree[v_first] == 1 || degree[v_last] == 1) {
                        chains.push_back(std::move(piece));
                        piece.clear();
                        degree[v_first]--;
                        degree[v_last]--;
                        changed = true;
                    }
                }
            }

            // Update outgoing edge for updated degree 1 vertices.
            for (size_t i = 0; i < num_pieces; i++) {
                const auto& piece = pieces[i];
                if (piece.empty()) continue;
                auto v_first = get_first_vertex(piece);
                auto v_last = get_last_vertex(piece);

                if (degree[v_first] == 2) {
                    register_edge(v_first, piece.front());
                }
                if (degree[v_last] == 2) {
                    register_edge(v_last, piece.back());
                }
            }

            // Extract complex loops with multiple pieces.
            for (size_t i = 0; i < num_pieces; i++) {
                auto& piece = pieces[i];
                if (piece.empty()) continue;
                auto v_last = get_last_vertex(piece);

                while (degree[v_last] == 2) {
                    auto next_edge = get_next_edge(piece.back(), v_last)[0];
                    la_debug_assert(next_edge != invalid<Index>());
                    auto j = piece_indices[next_edge];
                    la_debug_assert(j != invalid<size_t>());
                    if (j == i) break;

                    auto& next_piece = pieces[j];
                    if (next_edge == next_piece.front()) {
                        piece.insert(piece.end(), next_piece.begin(), next_piece.end());
                    } else {
                        la_debug_assert(next_edge == next_piece.back());
                        piece.insert(piece.end(), next_piece.cbegin(), next_piece.cend());
                    }
                    next_piece.clear();
                    v_last = get_last_vertex(piece);
                }
            }
        } while (changed);
        for (size_t i = 0; i < num_pieces; i++) {
            auto& piece = pieces[i];
            if (piece.empty()) continue;
            chains.push_back(std::move(piece));
        }
    };
    form_loops_and_chains();

    // Only disconnected simple loops are left.
    // Extract simple loops.
    for (Index i = 0; i < num_edges; i++) {
        if (piece_indices[i] != invalid<size_t>()) continue;
        auto e = detail::get_edge(edges, i);
        Index v0 = e[0];
        loops.push_back(grow_chain(i, v0));
    }

    if (!options.output_edge_index) {
        // Switch edge index to vertex index.
        // Chain's size will be increased by 1.
        for (auto& chain : chains) {
            Index curr_vertex = get_first_vertex(chain);
            for (auto& entry : chain) {
                auto e = detail::get_edge(edges, entry);
                entry = curr_vertex;
                curr_vertex = curr_vertex == e[0] ? e[1] : e[0];
            }
            chain.push_back(curr_vertex);
        }
        for (auto& loop : loops) {
            Index curr_vertex = get_first_vertex(loop);
            for (auto& entry : loop) {
                auto e = detail::get_edge(edges, entry);
                entry = curr_vertex;
                curr_vertex = curr_vertex == e[0] ? e[1] : e[0];
            }
            if (options.close_loop_with_identical_vertices) {
                loop.push_back(loop.front());
            }
        }
    }

    return result;
}

#define LA_X_chain_edges(_, Index)                                  \
    template ChainEdgesResult<Index> chain_directed_edges<Index>(   \
        const span<const Index> edges,                              \
        const ChainEdgesOptions& options);                          \
    template ChainEdgesResult<Index> chain_undirected_edges<Index>( \
        const span<const Index> edges,                              \
        const ChainEdgesOptions& options);
LA_ATTRIBUTE_INDEX_X(chain_edges, 0)

} // namespace lagrange
