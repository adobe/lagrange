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

#include <cassert>
#include <deque>
#include <vector>

#include <lagrange/MeshTrait.h>
#include <lagrange/common.h>
#include <lagrange/utils/range.h>

namespace lagrange {

template <typename Index>
struct ComputeBorderedComponentsOutput
{
    // maps the facet index to the component
    std::vector<Index> facet_to_component;
    // maps the component to its involved facets
    std::vector<std::vector<Index>> component_to_facets;
};

// Given a mesh and a binary list of whether certain edges are passable or blockers,
// Divides the mesh into different components enclosed by these non-passable edges
// (and of course the boundaries). There is no requirement on being manifold, but
// the edge data must be initialized (otherwise, how did you compute is_edge_passable
// in the first place).
//
// INPUT:
//   mesh
//   is_edge_passable (index -> bool)
//
// RETURNS:
//   ComputeBorderedComponentsOutput
//
// NOTE: Unlike Components.h that uses connectivity, this version uses the edge data
//       I am not sure if we can consolidate this two parts of the code (TODO: or can we?)
template <typename MeshType>
ComputeBorderedComponentsOutput<typename MeshType::Index> compute_bordered_components(
    const MeshType& mesh,
    const std::vector<bool>& is_edge_passable)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "First argument must be Lagrange::Mesh<>.");
    using Index = typename MeshType::Index;

    // We just check it, since the use must have called it in order to populate is_edge_passable.
    la_runtime_assert(mesh.is_edge_data_initialized(), "Edge data is not initialized");
    la_runtime_assert(safe_cast<Index>(is_edge_passable.size()) == mesh.get_num_edges());

    std::vector<Index> facet_component_ids(mesh.get_num_facets(), INVALID<Index>());

    auto perform_bfs = [&](const Index seed_id, const Index component_id) {
        assert(facet_component_ids[seed_id] == INVALID<Index>());
        std::deque<Index> search_queue;
        search_queue.push_back(seed_id);
        while (!search_queue.empty()) {
            const auto candidate_id = search_queue.back();
            search_queue.pop_back();
            if (facet_component_ids[candidate_id] == INVALID<Index>()) {
                facet_component_ids[candidate_id] = component_id;
                for (Index ci : range(mesh.get_vertex_per_facet())) {
                    const auto edge_id = mesh.get_edge(candidate_id, ci);
                    const bool is_passable = is_edge_passable[edge_id];
                    if (is_passable) {
                        mesh.foreach_facets_around_edge(edge_id, [&](Index f) {
                            if (f != candidate_id) { // Not a mandatory check.
                                search_queue.push_back(f);
                            }
                        });
                    } // passable
                } // edges
            } // have not been visited
        } // loop over queue
    }; // perform_bfs()


    Index num_components = 0;
    for (auto i : range(mesh.get_num_facets())) {
        if (facet_component_ids[i] == INVALID<Index>()) {
            perform_bfs(i, num_components);
            ++num_components;
        }
    }
    la_runtime_assert(
        num_components > 0 || mesh.get_num_facets() == 0,
        "Extracted " + std::to_string(num_components) + " comps out of " +
            std::to_string(mesh.get_num_facets()) + " facets");

    std::vector<std::vector<Index>> component_to_facets(num_components);
    std::vector<Index> num_component_facets(num_components, 0);
    for (auto i : range(mesh.get_num_facets())) {
        const auto component_id = facet_component_ids[i];
        ++num_component_facets[component_id];
    }
    for (auto i : range(num_components)) {
        component_to_facets[i].reserve(num_component_facets[i]);
    }
    for (auto i : range(mesh.get_num_facets())) {
        const auto component_id = facet_component_ids[i];
        component_to_facets[component_id].push_back(i);
    }


    ComputeBorderedComponentsOutput<Index> output;
    output.facet_to_component.swap(facet_component_ids);
    output.component_to_facets.swap(component_to_facets);

    return output;
}

} // namespace lagrange
