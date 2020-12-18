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
#include <lagrange/testing/common.h>

#include <vector>

#include <lagrange/Mesh.h>
#include <lagrange/common.h>
#include <lagrange/compute_bordered_components.h>
#include <lagrange/create_mesh.h>
#include <lagrange/utils/range.h>

using namespace lagrange;

namespace {

template <typename Index>
void verify_one_to_many_mapping(
    const std::vector<Index>& a_to_b,
    const std::vector<std::vector<Index>>& b_to_as)
{
    std::vector<bool> is_a_visited(a_to_b.size(), false);
    for (const auto b : range(b_to_as.size())) {
        for (const auto a : b_to_as[b]) {
            CHECK(b == a_to_b[a]);
            is_a_visited[a] = true;
        }
    }
    for (const auto a : range(a_to_b.size())) {
        CHECK(is_a_visited[a]);
    }
}
} // namespace

TEST_CASE("ComputeBorderedComponents", "[components][border]")
{
    using Scalar = float;
    using Index = std::uint32_t;
    using FacetArray = Eigen::Matrix<Index, Eigen::Dynamic, 3>;
    using VertexArray = Eigen::Matrix<Scalar, Eigen::Dynamic, 2>;
    using MeshType = Mesh<VertexArray, FacetArray>;

    FacetArray facets;
    VertexArray vertices;
    std::unique_ptr<MeshType> mesh;


    auto verify_answer = [&](ComputeBorderedComponentsOutput<Index> comps,
                             std::vector<Index> component_sizes_ref) -> void {
        REQUIRE((Index)comps.facet_to_component.size() == mesh->get_num_facets());
        verify_one_to_many_mapping(comps.facet_to_component, comps.component_to_facets);
        //
        std::vector<Index> component_sizes;
        for (auto i : range(comps.component_to_facets.size())) {
            component_sizes.push_back(safe_cast<Index>(comps.component_to_facets[i].size()));
        }
        std::sort(component_sizes.begin(), component_sizes.end());
        std::sort(component_sizes_ref.begin(), component_sizes_ref.end());
        REQUIRE(component_sizes_ref == component_sizes_ref);
    };

    SECTION("manifold cross")
    {
        // 0 ---- 1
        // | \  / |
        // |  4   |
        // | /  \ |
        // 3----- 2
        vertices.setZero(4, 2); // actual value does not matter
        facets.setZero(4, 3);
        facets.row(0) << 3, 2, 4;
        facets.row(1) << 1, 2, 4;
        facets.row(2) << 1, 4, 0;
        facets.row(3) << 3, 4, 0;
        mesh = create_mesh(vertices, facets);
        mesh->initialize_edge_data();

        SECTION("all edges passable")
        {
            std::vector<bool> is_passable(mesh->get_num_edges(), true);
            const auto comps = compute_bordered_components(*mesh, is_passable);
            verify_answer(comps, {4});
        }

        SECTION("no edge passable")
        {
            std::vector<bool> is_passable(mesh->get_num_edges(), false);
            const auto comps = compute_bordered_components(*mesh, is_passable);
            verify_answer(comps, {1, 1, 1, 1});
        }

        SECTION("one edge not passable")
        {
            std::vector<bool> is_passable(mesh->get_num_edges(), true);
            is_passable[mesh->get_edge_index({1, 4})] = false;
            const auto comps = compute_bordered_components(*mesh, is_passable);
            verify_answer(comps, {4});
        }

        SECTION("two edges not passable")
        {
            std::vector<bool> is_passable(mesh->get_num_edges(), true);
            is_passable[mesh->get_edge_index({1, 4})] = false;
            is_passable[mesh->get_edge_index({0, 4})] = false;
            const auto comps = compute_bordered_components(*mesh, is_passable);
            verify_answer(comps, {2, 2});
        }

        SECTION("three edges not passable")
        {
            std::vector<bool> is_passable(mesh->get_num_edges(), true);
            is_passable[mesh->get_edge_index({1, 4})] = false;
            is_passable[mesh->get_edge_index({0, 4})] = false;
            is_passable[mesh->get_edge_index({2, 4})] = false;
            const auto comps = compute_bordered_components(*mesh, is_passable);
            verify_answer(comps, {1, 1, 2});
        }
    } // manifold cross

    SECTION("non-manifold cross")
    {
        // I had to draw 3 and 4 two times because of ascii art
        // shortcomings :D
        // 0 ---- 1------5---3
        // | \  / |      |
        // |  4   |      4
        // | /  \ |
        // 3----- 2
        vertices.setZero(5, 2); // actual value does not matter
        facets.setZero(6, 3);
        facets.row(0) << 3, 2, 4;
        facets.row(1) << 1, 2, 4;
        facets.row(2) << 1, 4, 0;
        facets.row(3) << 3, 4, 0;
        facets.row(4) << 5, 1, 0;
        facets.row(5) << 5, 4, 3;
        mesh = create_mesh(vertices, facets);
        mesh->initialize_edge_data();

        SECTION("all edges passable")
        {
            std::vector<bool> is_passable(mesh->get_num_edges(), true);
            const auto comps = compute_bordered_components(*mesh, is_passable);
            verify_answer(comps, {6});
        }

        SECTION("no edge passable")
        {
            std::vector<bool> is_passable(mesh->get_num_edges(), false);
            const auto comps = compute_bordered_components(*mesh, is_passable);
            verify_answer(comps, {1, 1, 1, 1, 1, 1});
        }

        SECTION("two edge not passable ")
        {
            std::vector<bool> is_passable(mesh->get_num_edges(), true);
            is_passable[mesh->get_edge_index({1, 4})] = false;
            is_passable[mesh->get_edge_index({3, 4})] = false;
            const auto comps = compute_bordered_components(*mesh, is_passable);
            verify_answer(comps, {2, 2, 2});
        }

        SECTION("two edges not passable")
        {
            std::vector<bool> is_passable(mesh->get_num_edges(), true);
            is_passable[mesh->get_edge_index({1, 4})] = false;
            is_passable[mesh->get_edge_index({0, 4})] = false;
            const auto comps = compute_bordered_components(*mesh, is_passable);
            verify_answer(comps, {2, 2});
        }

        SECTION("three edges not passable")
        {
            std::vector<bool> is_passable(mesh->get_num_edges(), true);
            is_passable[mesh->get_edge_index({1, 4})] = false;
            is_passable[mesh->get_edge_index({0, 4})] = false;
            is_passable[mesh->get_edge_index({2, 4})] = false;
            const auto comps = compute_bordered_components(*mesh, is_passable);
            verify_answer(comps, {1, 1, 2});
        }
    } // non-manifold cross
}
