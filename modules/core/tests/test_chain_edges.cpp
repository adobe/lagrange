/*
 * Copyright 2022 Adobe. All rights reserved.
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

#include <lagrange/chain_edges.h>
#include <lagrange/utils/warning.h>

#include <array>
#include <vector>

TEST_CASE("chain_edges", "[core]")
{
    LA_IGNORE_DEPRECATION_WARNING_BEGIN
    using namespace lagrange;

    auto get_num_edges_in_chains = [](const auto& chains) -> size_t {
        size_t num_edges = 0;
        for (const auto& chain : chains) {
            num_edges += chain.size() - 1;
        }
        return num_edges;
    };

    SECTION("Single loop")
    {
        std::vector<std::array<size_t, 2>> edges{{0, 1}, {1, 2}, {2, 0}};
        auto chains = chain_edges<size_t>(edges, true);
        REQUIRE(chains.size() == 1);
        REQUIRE(chains[0].size() == 4);
    }

    SECTION("Double loop")
    {
        std::vector<std::array<size_t, 2>> edges{{0, 1}, {1, 2}, {2, 0}, {3, 4}, {4, 5}, {5, 3}};
        auto chains = chain_edges<size_t>(edges, true);
        REQUIRE(chains.size() == 2);
        REQUIRE(chains[0].size() == 4);
        REQUIRE(chains[1].size() == 4);
    }

    SECTION("Single chain")
    {
        std::vector<std::array<size_t, 2>> edges{{0, 1}, {1, 2}, {2, 3}};
        auto chains = chain_edges<size_t>(edges, true);
        REQUIRE(chains.size() == 1);
        REQUIRE(chains[0].size() == 4);
    }

    SECTION("Chain and loop")
    {
        std::vector<std::array<size_t, 2>> edges{{0, 1}, {1, 2}, {2, 3}, {4, 5}, {5, 6}, {6, 4}};
        auto chains = chain_edges<size_t>(edges, true);
        REQUIRE(chains.size() == 2);
        REQUIRE(chains[0].size() == 4);
        REQUIRE(chains[1].size() == 4);
    }

    SECTION("Non-manifold")
    {
        std::vector<std::array<size_t, 2>> edges{{0, 1}, {1, 2}, {2, 0}, {2, 3}};
        auto chains = chain_edges<size_t>(edges, true);
        REQUIRE(get_num_edges_in_chains(chains) == edges.size());
    }

    SECTION("empty")
    {
        std::vector<std::array<size_t, 2>> edges;
        auto chains = chain_edges<size_t>(edges, true);
        REQUIRE(chains.empty());
    }

    SECTION("undirected simple loop")
    {
        std::vector<std::array<size_t, 2>> edges{{0, 1}, {1, 2}, {0, 2}};
        auto chains = chain_undirected_edges<size_t>(edges, true);
        REQUIRE(chains.size() == 1);
        REQUIRE(chains[0].size() == 4);
    }

    SECTION("undirected simple chain")
    {
        std::vector<std::array<size_t, 2>> edges{{0, 1}, {0, 2}};
        auto chains = chain_undirected_edges<size_t>(edges, true);
        REQUIRE(chains.size() == 1);
        REQUIRE(chains[0].size() == 3);
    }

    SECTION("loop with tail")
    {
        std::vector<std::array<size_t, 2>> edges{{0, 1}, {0, 2}, {1, 2}, {2, 3}};
        auto chains = chain_undirected_edges<size_t>(edges, true);
        REQUIRE(chains.size() == 2);
        REQUIRE(chains[0].size() + chains[1].size() == 6);
        REQUIRE(get_num_edges_in_chains(chains) == edges.size());
    }
    LA_IGNORE_DEPRECATION_WARNING_END
}
