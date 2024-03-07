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
#include <lagrange/chain_edges.h>
#include <lagrange/chain_edges_into_simple_loops.h>
#include <lagrange/testing/common.h>
#include <lagrange/utils/chain_edges.h>
#include <lagrange/utils/warning.h>
#include <catch2/benchmark/catch_benchmark.hpp>

#include <fstream>
#include <vector>

TEST_CASE("utils/chain_edges", "[core][utils]")
{
    using namespace lagrange;
    using Index = int32_t;

    SECTION("Case 1")
    {
        Index edges[]{0, 1, 1, 2, 2, 0};

        SECTION("directed")
        {
            auto r = lagrange::chain_directed_edges<Index>(edges);
            REQUIRE(r.loops.size() == 1);
            REQUIRE(r.chains.size() == 0);
            REQUIRE(r.loops[0].size() == 3);
        }
        SECTION("undirected")
        {
            auto r = lagrange::chain_undirected_edges<Index>(edges, ChainEdgesOptions());
            REQUIRE(r.loops.size() == 1);
            REQUIRE(r.chains.size() == 0);
            REQUIRE(r.loops[0].size() == 3);
        }
    }

    SECTION("Case 2")
    {
        Index edges[]{0, 1, 1, 2, 2, 0, 3, 4};

        auto r = lagrange::chain_directed_edges<Index>(edges);
        REQUIRE(r.loops.size() == 1);
        REQUIRE(r.chains.size() == 1);
        REQUIRE(r.loops[0].size() == 3);
        REQUIRE(r.chains[0].size() == 2);
    }

    SECTION("Case 3")
    {
        Index edges[]{0, 1, 1, 2, 2, 0, 3, 4, 4, 0};

        auto r = lagrange::chain_directed_edges<Index>(edges);
        REQUIRE(r.loops.size() == 1);
        REQUIRE(r.chains.size() == 1);
        REQUIRE(r.loops[0].size() == 3);
        REQUIRE(r.chains[0].size() == 3);
    }
    SECTION("Eigen")
    {
        Index edges[]{0, 1, 1, 2, 2, 0, 0, 3, 3, 4, 4, 0};

        auto r = lagrange::chain_directed_edges<Index>(edges);
        REQUIRE(r.loops.size() == 2);
        REQUIRE(r.chains.size() == 0);
        REQUIRE(r.loops[0].size() == 3);
        REQUIRE(r.loops[1].size() == 3);
    }
    SECTION("Non-manifold")
    {
        /*
           2   4   6
           /\  /\  /\
          /__\/__\/__\
         0   1   3    5
        */
        Index edges[]{0, 1, 1, 2, 2, 0, 1, 3, 3, 4, 4, 1, 3, 5, 5, 6, 6, 3};

        SECTION("directed")
        {
            auto r = lagrange::chain_directed_edges<Index>(edges);
            REQUIRE(r.loops.size() == 3);
            REQUIRE(r.chains.size() == 0);
            REQUIRE(r.loops[0].size() == 3);
            REQUIRE(r.loops[1].size() == 3);
            REQUIRE(r.loops[2].size() == 3);
        }
        SECTION("undirected")
        {
            ChainEdgesOptions opt;
            auto r = lagrange::chain_undirected_edges<Index>(edges, opt);
            REQUIRE(r.loops.size() == 3);
            REQUIRE(r.chains.size() == 0);
            REQUIRE(r.loops[0].size() == 3);
            REQUIRE(r.loops[1].size() == 3);
            REQUIRE(r.loops[2].size() == 3);
        }
    }
    SECTION("pound sign")
    {
        //     10 11
        //   7 |   | 8
        // 6 --+---+-- 9
        //     |   |
        // 2 --+---+-- 5
        //   3 |   | 4
        //     0   1
        Index edges[]{0, 3, 1, 4, 2, 3, 3, 4, 4, 5, 7, 3, 4, 8, 6, 7, 8, 7, 8, 9, 7, 10, 8, 11};
        SECTION("undirected")
        {
            auto r = lagrange::chain_undirected_edges<Index>(edges, ChainEdgesOptions());
            REQUIRE(r.loops.size() == 1);
            REQUIRE(r.chains.size() == 8);
        }
        SECTION("directed")
        {
            auto r = lagrange::chain_directed_edges<Index>(edges);
            REQUIRE(r.loops.size() == 1);
            REQUIRE(r.chains.size() == 8);
        }
    }
    SECTION("plus sign")
    {
        //       4
        //       |
        //       | 0
        // 1 ----+---- 3
        //       |
        //       |
        //       2
        Index edges[]{0, 1, 0, 2, 0, 3, 0, 4};

        SECTION("undirected")
        {
            auto r = lagrange::chain_undirected_edges<Index>(edges, ChainEdgesOptions());
            REQUIRE(r.loops.size() == 0);
            REQUIRE(r.chains.size() == 4);
        }
        SECTION("directed")
        {
            auto r = lagrange::chain_directed_edges<Index>(edges, ChainEdgesOptions());
            REQUIRE(r.loops.size() == 0);
            REQUIRE(r.chains.size() == 4);
        }
    }

    SECTION("Bug")
    {
        lagrange::fs::path data_path = testing::get_data_path("open/core/chain_edges_data1.txt");
        std::ifstream fin(data_path.string());
        std::vector<Index> edges;
        {
            Index v;
            while (fin >> v) {
                edges.push_back(v);
            }
        }
        fin.close();

        auto r = lagrange::chain_undirected_edges<Index>(edges, ChainEdgesOptions());
        REQUIRE(r.loops.size() > 0);
        REQUIRE(r.chains.size() > 0);
    }

    SECTION("Bug2")
    {
        lagrange::fs::path data_path = testing::get_data_path("open/core/chain_edges_data2.txt");
        std::ifstream fin(data_path.string());
        std::vector<Index> edges;
        {
            Index v;
            while (fin >> v) {
                edges.push_back(v);
            }
        }
        fin.close();

        ChainEdgesOptions opt;
        opt.output_edge_index = true;
        auto r = lagrange::chain_undirected_edges<Index>(edges, opt);
        REQUIRE(r.loops.size() > 0);
        REQUIRE(r.chains.size() > 0);
        size_t total_edges = 0;
        std::for_each(r.loops.begin(), r.loops.end(), [&total_edges](const auto& loop) {
            total_edges += loop.size();
        });
        std::for_each(r.chains.begin(), r.chains.end(), [&total_edges](const auto& chain) {
            total_edges += chain.size();
        });
        REQUIRE(total_edges == span<Index>(edges).size() / 2);
    }
}

TEST_CASE("chain_edges benchmark", "[core][utils][!benchmark]")
{
    LA_IGNORE_DEPRECATION_WARNING_BEGIN
    using namespace lagrange;
    using Index = uint32_t;

    constexpr Index n = 1e6;
    Eigen::Matrix<Index, Eigen::Dynamic, 2, Eigen::RowMajor> edges(n, 2);
    for (Index i = 0; i < n; i++) {
        edges(i, 0) = i;
        edges(i, 1) = i + 1;
    }
    edges(n - 1, 1) = 0;

    {
        // Correctness check for chain_undirected_edges
        auto r =
            lagrange::chain_undirected_edges<Index>({edges.data(), n * 2}, ChainEdgesOptions());
        REQUIRE(r.loops.size() == 1);
        REQUIRE(r.chains.size() == 0);
    }

    {
        // Correctness check for legacy::chain_edges
        auto chains = lagrange::chain_edges<Index>(edges.rowwise());
        REQUIRE(chains.size() == 1);
    }

    {
        // Correctness check for chain_edges_into_simple_loops
        std::vector<std::vector<Index>> loops;
        Eigen::Matrix<Index, Eigen::Dynamic, 2, Eigen::RowMajor> chains;

        REQUIRE(lagrange::chain_edges_into_simple_loops(edges, loops, chains));

        REQUIRE(loops.size() == 1);
        REQUIRE(chains.size() == 0);
    }

    BENCHMARK("chain_directed_edges")
    {
        lagrange::ChainEdgesOptions opt;
        opt.output_edge_index = true;
        return lagrange::chain_directed_edges<Index>({edges.data(), n * 2}, opt);
    };
    BENCHMARK("chain_edges")
    {
        return lagrange::chain_edges<Index>(edges.rowwise());
    };
    BENCHMARK("chain_edges_into_simple_loops")
    {
        std::vector<std::vector<Index>> loops;
        Eigen::Matrix<Index, Eigen::Dynamic, 2, Eigen::RowMajor> chains;
        return lagrange::chain_edges_into_simple_loops(edges, loops, chains);
    };
    LA_IGNORE_DEPRECATION_WARNING_END
}

