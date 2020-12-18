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
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <lagrange/corner_to_edge_mapping.h>

#include <lagrange/Logger.h>
#include <lagrange/utils/la_assert.h>
#include <lagrange/utils/safe_cast.h>

#include <tbb/parallel_sort.h>
#include <Eigen/Dense>

#include <algorithm>
#include <utility>
#include <vector>
////////////////////////////////////////////////////////////////////////////////

namespace lagrange {

template <typename DerivedF, typename DerivedC>
Eigen::Index corner_to_edge_mapping(
    const Eigen::MatrixBase<DerivedF>& F,
    Eigen::PlainObjectBase<DerivedC>& C2E)
{
    using Index = typename DerivedF::Scalar;

    struct UnorientedEdge
    {
        Index v1;
        Index v2;
        Index corner;

        UnorientedEdge(Index x, Index y, Index c)
            : v1(std::min(x, y))
            , v2(std::max(x, y))
            , corner(c)
        {}

        auto key() const { return std::make_pair(v1, v2); }

        bool operator<(const UnorientedEdge& e) const { return key() < e.key(); }

        bool operator!=(const UnorientedEdge& e) const { return key() != e.key(); }
    };

    Index vert_per_facet = safe_cast<Index>(F.cols());

    // Sort unoriented edges
    std::vector<UnorientedEdge> edges;
    edges.reserve(F.rows() * F.cols());
    for (Index f = 0; f < (Index) F.rows(); ++f) {
        for (Index lv = 0; lv < (Index) F.cols(); ++lv) {
            auto v1 = F(f, lv);
            auto v2 = F(f, (lv + 1) % vert_per_facet);
            edges.emplace_back(v1, v2, f * vert_per_facet + lv);
        }
    }
    logger().trace("[corner_to_edge_mapping] sorting edges");
    tbb::parallel_sort(edges.begin(), edges.end());
    logger().trace("[corner_to_edge_mapping] sorting done");

    // Assign unique edge ids
    C2E.resize(F.rows() * F.cols());
    Index num_edges = 0;
    for (auto itBegin = edges.begin(); itBegin != edges.end();) {
        // First the first edge after itBegin that has a different key
        auto itEnd = std::find_if(itBegin, edges.end(), [&](auto e) { return (e != *itBegin); });
        for (auto it = itBegin; it != itEnd; ++it) {
            C2E(it->corner) = num_edges;
        }
        ++num_edges;
        itBegin = itEnd;
    }
    logger().trace("[corner_to_edge_mapping] cleanup");

    return num_edges;
}

} // namespace lagrange
