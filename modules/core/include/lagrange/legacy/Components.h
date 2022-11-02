/*
 * Copyright 2017 Adobe. All rights reserved.
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

#include <queue>
#include <vector>

#include <lagrange/Connectivity.h>
#include <lagrange/MeshGeometry.h>
#include <lagrange/common.h>
#include <lagrange/legacy/inline.h>
#include <lagrange/utils/safe_cast.h>

namespace lagrange {
LAGRANGE_LEGACY_INLINE
namespace legacy {

template <typename GeometryType>
class Components
{
public:
    using Index = typename GeometryType::Index;
    using IndexList = typename Connectivity<GeometryType>::IndexList;

    void initialize(GeometryType& geometry, Connectivity<GeometryType>* conn = nullptr)
    {
        std::unique_ptr<Connectivity<GeometryType>> temporary_conn_ptr = nullptr;

        if (!conn) {
            temporary_conn_ptr = compute_connectivity<GeometryType>(geometry);
            conn = temporary_conn_ptr.get();
        }

        m_components.clear();
        const auto num_facets = geometry.get_num_facets();
        std::vector<bool> visited(num_facets, false);
        for (Index i = 0; i < num_facets; i++) {
            if (visited[i]) continue;
            m_components.emplace_back();
            auto& comp = m_components.back();

            std::queue<Index> Q;
            Q.push(i);
            visited[i] = true;
            while (!Q.empty()) {
                Index f = Q.front();
                Q.pop();
                comp.push_back(f);

                const auto& adj_facets = conn->get_facets_adjacent_to_facet(f);
                for (const Index& adj_f : adj_facets) {
                    if (!visited[adj_f]) {
                        Q.push(adj_f);
                        visited[adj_f] = true;
                    }
                }
            }
        }

        m_per_facet_component_ids.resize(num_facets);
        Index comp_id = 0;
        for (const auto& comp : m_components) {
            for (const auto fid : comp) {
                m_per_facet_component_ids[fid] = comp_id;
            }
            comp_id++;
        }
    }

    const std::vector<IndexList>& get_components() const { return m_components; }

    const IndexList& get_per_facet_component_ids() const { return m_per_facet_component_ids; }

    Index get_num_components() const { return safe_cast<Index>(m_components.size()); }

protected:
    std::vector<IndexList> m_components;
    IndexList m_per_facet_component_ids;
};

template <typename GeometryType>
std::unique_ptr<Components<GeometryType>> compute_components(
    const GeometryType& geometry,
    Connectivity<GeometryType>* conn = nullptr)
{
    auto ptr = std::make_unique<Components<GeometryType>>();

    ptr->initialize(geometry, conn);
    return ptr;
}

} // namespace legacy
} // namespace lagrange
