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

#pragma once

#include <lagrange/SurfaceMesh.h>
#include <lagrange/foreach_attribute.h>
#include <lagrange/utils/invalid.h>
#include <lagrange/utils/safe_cast.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_contains.hpp>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <algorithm>
#include <array>
#include <set>
#include <type_traits>
#include <unordered_set>
#include <vector>

namespace lagrange::testing {

// Check whether the function f : X --> Y restricted to the elements that maps onto Y, is
// surjective. Index.e. each element of [first, last) has an antecedent through f.
template <typename Index>
bool is_restriction_surjective(lagrange::span<const Index> func, Index first, Index last)
{
    using namespace lagrange;
    std::vector<Index> inv(last - first, invalid<Index>());
    for (Index x = 0; x < Index(func.size()); ++x) {
        const Index y = func[x];
        if (y >= first && y < last) {
            inv[y - first] = x;
        }
    }
    // No element in the range [first, last) has no predecessor through the function
    return std::none_of(inv.begin(), inv.end(), [&](Index y) { return y == invalid<Index>(); });
}

// Check whether the function f : X --> Y restricted to the elements that maps onto Y, is injective.
// Index.e. any two elements that maps onto [first, last) map to different values in [first, last).
template <typename Index>
bool is_restriction_injective(lagrange::span<const Index> func, Index first, Index last)
{
    using namespace lagrange;
    std::vector<Index> inv(last - first, invalid<Index>());
    for (Index x = 0; x < Index(func.size()); ++x) {
        const Index y = func[x];
        if (y >= first && y < last) {
            // Just found another x' that maps to the same value y
            if (inv[y - first] != invalid<Index>() && inv[y - first] != x) {
                return false;
            }
            inv[y - first] = x;
        }
    }
    return true;
}

// Check whether elements map within [first, last)
template <typename Index>
bool is_in_range(lagrange::span<const Index> func, Index first, Index last)
{
    return std::all_of(func.begin(), func.end(), [&](Index y) { return first <= y && y < last; });
}

template <typename Index>
bool is_in_range_or_invalid(lagrange::span<const Index> func, Index first, Index last)
{
    return std::all_of(func.begin(), func.end(), [&](Index y) {
        return (first <= y && y < last) || y == lagrange::invalid<Index>();
    });
}

template <typename Index>
bool is_surjective(lagrange::span<const Index> func, Index first, Index last)
{
    return is_in_range(func, first, last) && is_restriction_surjective(func, first, last);
}

template <typename Index>
bool is_injective(lagrange::span<const Index> func, Index first, Index last)
{
    return is_in_range(func, first, last) && is_restriction_injective(func, first, last);
}

template <typename MeshType>
void check_mesh(const MeshType& mesh)
{
    using namespace lagrange;
    using Index = typename MeshType::Index;
    const Index nv = mesh.get_num_vertices();
    const Index nf = mesh.get_num_facets();
    const Index nc = mesh.get_num_corners();
    const Index ne = mesh.get_num_edges();

    // Ensure that (V, F) is well-formed
    for (Index f = 0; f < nf; ++f) {
        const Index c0 = mesh.get_facet_corner_begin(f);
        const Index c1 = mesh.get_facet_corner_end(f);
        REQUIRE(c0 < c1);
        for (Index c = c0; c < c1; ++c) {
            const Index v = mesh.get_corner_vertex(c);
            REQUIRE(mesh.get_corner_facet(c) == f);
            REQUIRE((v >= 0 && v < nv));
        }
    }

    // Ensure that each attribute has the correct number of elements
    seq_foreach_attribute_read<AttributeElement::Vertex>(mesh, [&](auto&& attr) {
        REQUIRE(attr.get_num_elements() == nv);
    });
    seq_foreach_attribute_read<AttributeElement::Facet>(mesh, [&](auto&& attr) {
        REQUIRE(attr.get_num_elements() == nf);
    });
    seq_foreach_attribute_read<AttributeElement::Corner>(mesh, [&](auto&& attr) {
        REQUIRE(attr.get_num_elements() == nc);
    });

    // Ensure that each element index is in range (or an invalid index)
    seq_foreach_attribute_read(mesh, [&](auto&& attr) {
        using AttributeType = std::decay_t<decltype(attr)>;
        using ValueType = typename AttributeType::ValueType;
        Index n = 0;
        auto usage = attr.get_usage();
        if (usage == AttributeUsage::VertexIndex) {
            n = nv;
        } else if (usage == AttributeUsage::FacetIndex) {
            n = nf;
        } else if (usage == AttributeUsage::CornerIndex) {
            n = nc;
        } else if (usage == AttributeUsage::EdgeIndex) {
            n = ne;
        } else {
            return;
        }
        REQUIRE(std::is_same_v<ValueType, Index>);
        if constexpr (std::is_same_v<ValueType, Index>) {
            if constexpr (AttributeType::IsIndexed) {
                REQUIRE(is_in_range_or_invalid<ValueType>(attr.values().get_all(), 0, n));
            } else {
                REQUIRE(is_in_range_or_invalid<ValueType>(attr.get_all(), 0, n));
            }
        } else {
            LA_IGNORE(n);
        }
    });

    // Ensure that only hybrid meshes have c <--> f attributes
    if (mesh.is_hybrid()) {
        REQUIRE(mesh.attr_id_facet_to_first_corner() != invalid_attribute_id());
        REQUIRE(mesh.attr_id_corner_to_facet() != invalid_attribute_id());
    } else {
        REQUIRE(mesh.is_regular());
        REQUIRE(mesh.attr_id_facet_to_first_corner() == invalid_attribute_id());
        REQUIRE(mesh.attr_id_corner_to_facet() == invalid_attribute_id());
    }

    // Ensure that edge and connectivity information is well-formed
    if (mesh.has_edges()) {
        // Check that all facet edges have a single corresponding edge in the global indexing
        auto c2e = mesh.template get_attribute<Index>(mesh.attr_id_corner_to_edge()).get_all();
        auto e2c =
            mesh.template get_attribute<Index>(mesh.attr_id_edge_to_first_corner()).get_all();
        auto v2c =
            mesh.template get_attribute<Index>(mesh.attr_id_vertex_to_first_corner()).get_all();
        auto next_around_edge =
            mesh.template get_attribute<Index>(mesh.attr_id_next_corner_around_edge()).get_all();
        auto next_around_vertex =
            mesh.template get_attribute<Index>(mesh.attr_id_next_corner_around_vertex()).get_all();
        REQUIRE(is_surjective<Index>(c2e, 0, ne));
        REQUIRE(is_injective<Index>(e2c, 0, nc));
        REQUIRE(is_in_range_or_invalid<Index>(v2c, 0, nc));
        REQUIRE(is_restriction_injective<Index>(
            v2c,
            0,
            nc)); // may have isolated vertices that map to invalid<>()

        // Make sure that e2c contains the name number of edges as the mesh
        std::set<std::pair<Index, Index>> edges;
        for (Index f = 0; f < nf; ++f) {
            for (Index lv0 = 0, s = mesh.get_facet_size(f); lv0 < s; ++lv0) {
                const Index lv1 = (lv0 + 1) % s;
                const Index v0 = mesh.get_facet_vertex(f, lv0);
                const Index v1 = mesh.get_facet_vertex(f, lv1);
                edges.emplace(std::min(v0, v1), std::max(v0, v1));
            }
        }
        std::vector<std::array<Index, 2>> mesh_edges;
        for (Index e = 0; e < ne; ++e) {
            auto v = mesh.get_edge_vertices(e);
            mesh_edges.push_back({v[0], v[1]});
        }
        REQUIRE(edges.size() == e2c.size());
        // Make sure we don't have edges that are not in the mesh?
        edges.clear();
        for (auto c : e2c) {
            const Index f = mesh.get_corner_facet(c);
            const Index first_corner = mesh.get_facet_corner_begin(f);
            const Index s = mesh.get_facet_size(f);
            const Index lv0 = c - first_corner;
            const Index lv1 = (lv0 + 1) % s;
            const Index v0 = mesh.get_facet_vertex(f, lv0);
            const Index v1 = mesh.get_facet_vertex(f, lv1);
            edges.emplace(std::min(v0, v1), std::max(v0, v1));
        }
        REQUIRE(edges.size() == e2c.size());
        // Make sure that every corner points to an edge and back to the same vertex or the other
        // end vertex of the edge.
        for (Index f = 0; f < nf; ++f) {
            const Index first_corner = mesh.get_facet_corner_begin(f);
            for (Index lv0 = 0, s = mesh.get_facet_size(f); lv0 < s; ++lv0) {
                const Index v0 = mesh.get_facet_vertex(f, lv0);
                const Index c = first_corner + lv0;
                const Index e = c2e[c];
                const Index c_other = e2c[e];
                const Index v_other = mesh.get_corner_vertex(c_other);

                // v0 and v_other should be the end points of an edge.
                REQUIRE_THAT(mesh_edges[e], Catch::Matchers::Contains(v0));
                REQUIRE_THAT(mesh_edges[e], Catch::Matchers::Contains(v_other));
            }
        }
        // Check that for every vertex / every edge, the "chain" of corners around it touches
        // all the incident corners exactly once (no duplicate).
        std::vector<std::unordered_set<Index>> corners_around_vertex(nv);
        std::vector<std::unordered_set<Index>> corners_around_edge(ne);
        std::vector<std::unordered_set<Index>> facets_around_vertex(nv);
        std::vector<std::unordered_set<Index>> facets_around_edge(ne);
        for (Index f = 0; f < nf; ++f) {
            const Index first_corner = mesh.get_facet_corner_begin(f);
            for (Index lv = 0, s = mesh.get_facet_size(f); lv < s; ++lv) {
                const Index v = mesh.get_facet_vertex(f, lv);
                const Index c = first_corner + lv;
                const Index e = c2e[c];
                REQUIRE(mesh.get_edge(f, lv) == e);
                REQUIRE(mesh.get_corner_edge(c) == e);
                REQUIRE(corners_around_vertex[v].count(c) == 0);
                REQUIRE(corners_around_edge[e].count(c) == 0);
                corners_around_vertex[v].insert(c);
                corners_around_edge[e].insert(c);
                facets_around_vertex[v].insert(f);
                facets_around_edge[e].insert(f);
            }
        }
        std::unordered_set<Index> corners_around;
        std::unordered_set<Index> facets_around;
        for (Index v = 0; v < nv; ++v) {
            corners_around.clear();
            facets_around.clear();
            const Index c0 = v2c[v];
            REQUIRE(mesh.get_first_corner_around_vertex(v) == c0);
            REQUIRE(mesh.get_one_corner_around_vertex(v) == c0);
            for (Index ci = c0; ci != invalid<Index>(); ci = next_around_vertex[ci]) {
                REQUIRE(mesh.get_next_corner_around_vertex(ci) == next_around_vertex[ci]);
                REQUIRE(corners_around_vertex[v].count(ci));
                REQUIRE(corners_around.count(ci) == 0);
                corners_around.insert(ci);
            }
            mesh.foreach_corner_around_vertex(v, [&](Index c) {
                REQUIRE(corners_around.count(c));
            });
            mesh.foreach_facet_around_vertex(v, [&](Index f) {
                REQUIRE(facets_around_vertex[v].count(f));
                facets_around.insert(f);
            });
            REQUIRE(corners_around.size() == corners_around_vertex[v].size());
            REQUIRE(facets_around.size() == facets_around_vertex[v].size());
            REQUIRE(
                corners_around.size() ==
                safe_cast<size_t>(mesh.count_num_corners_around_vertex(v)));
        }
        for (Index e = 0; e < ne; ++e) {
            corners_around.clear();
            facets_around.clear();
            const Index c0 = e2c[e];
            REQUIRE(mesh.get_first_corner_around_edge(e) == c0);
            REQUIRE(mesh.get_one_corner_around_edge(e) == c0);
            for (Index ci = c0; ci != invalid<Index>(); ci = next_around_edge[ci]) {
                REQUIRE(mesh.get_next_corner_around_edge(ci) == next_around_edge[ci]);
                REQUIRE(corners_around_edge[e].count(ci));
                REQUIRE(corners_around.count(ci) == 0);
                corners_around.insert(ci);
            }
            mesh.foreach_corner_around_edge(e, [&](Index c) { REQUIRE(corners_around.count(c)); });
            Index first_facet = invalid<Index>();
            mesh.foreach_facet_around_edge(e, [&](Index f) {
                REQUIRE(facets_around_edge[e].count(f));
                facets_around.insert(f);
                if (first_facet == invalid<Index>()) {
                    first_facet = f;
                }
            });
            REQUIRE(mesh.get_one_facet_around_edge(e) == first_facet);
            REQUIRE(corners_around.size() == corners_around_edge[e].size());
            REQUIRE(facets_around.size() == facets_around_edge[e].size());
            REQUIRE(
                corners_around.size() == safe_cast<size_t>(mesh.count_num_corners_around_edge(e)));
            if (mesh.is_boundary_edge(e)) {
                REQUIRE(corners_around.size() == 1);
            }
        }
    }
}

} // namespace lagrange::testing
