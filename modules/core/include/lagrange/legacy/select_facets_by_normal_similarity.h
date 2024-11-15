/*
 * Copyright 2019 Adobe. All rights reserved.
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

#include <lagrange/common.h>
#include <lagrange/compute_triangle_normal.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/range.h>

#include <limits>

namespace lagrange {
LAGRANGE_LEGACY_INLINE
namespace legacy {

/**
 * Given a seed vertex, selects faces around it based on the change
 * in triangle normals.
 */

//
// The parameters for the function
//
template <typename MeshType>
struct SelectFacetsByNormalSimilarityParameters
{
    using Scalar = typename MeshType::Scalar;
    using Index = typename MeshType::Index;

    //
    // This are input and have to be set
    //
    // Increasing this would select a larger region
    Scalar flood_error_limit = std::numeric_limits<Scalar>::max();
    // This tries to smooth the selection boundary (reduce ears)
    bool should_smooth_boundary = true;

    //
    // These parameters have default values
    // Default value comes from segmentation/Constants.h
    //
    // If this is not empty, then only faces are considered which are marked as true
    std::vector<bool> is_facet_selectable = std::vector<bool>();
    // Internal param...
    Scalar flood_second_to_first_order_limit_ratio = 1.0 / 6.0;
    // Number of iterations for smoothing the boundary
    Index num_smooth_iterations = 3;
    // Use DFS or BFS search
    enum class SearchType { BFS = 0, DFS = 1 };
    SearchType search_type = SearchType::DFS;

    // Set selectable facets using a vector of indices
    void set_selectable_facets(MeshType& mesh_ref, const std::vector<Index>& selectable_facets)
    {
        if (selectable_facets.empty()) {
            is_facet_selectable.clear();
        } else {
            is_facet_selectable.resize(mesh_ref.get_num_facets(), false);
            for (auto facet_id : range_sparse(mesh_ref.get_num_facets(), selectable_facets)) {
                is_facet_selectable[facet_id] = true;
            }
        }
    }

}; // SelectFaceByNormalSimilarityParameters

//
// Perform the selection
// mesh, input: the mesh
// seed_facet_id, input: the index of the seed
// parameters, input: the parameters above
// returns a vector that contains true for facets that are selected
//
template <typename MeshType>
std::vector<bool> select_facets_by_normal_similarity(
    MeshType& mesh,
    const typename MeshType::Index seed_facet_id,
    const SelectFacetsByNormalSimilarityParameters<MeshType>& parameters)
{
    // ===================================================
    //     Helper typedef and lambdas
    // ===================================================

    using VertexType = typename MeshType::VertexType;
    using Index = typename MeshType::Index;
    using Scalar = typename MeshType::Scalar;
    using IndexList = typename MeshType::IndexList;
    using AttributeArray = typename MeshType::AttributeArray;
    using Parameters = SelectFacetsByNormalSimilarityParameters<MeshType>;

    // Check if a face is selectable
    auto is_facet_selectable = [&mesh, &parameters](const Index facet_id) -> bool {
        if (parameters.is_facet_selectable.empty()) {
            return true;
        } else {
            la_runtime_assert(facet_id < mesh.get_num_facets());
            return parameters.is_facet_selectable[facet_id];
        }
    };

    // The energy between two normals
    auto normal_direction_error = [](const VertexType& n1, const VertexType& n2) -> Scalar {
        return static_cast<Scalar>(1.0) - std::abs(n1.dot(n2));
    };

    // ===================================================
    //     Let's begin
    //     current code just copied from
    //     Segmentation/SuperTriangulation/SingleFloodFromSeed()
    // ===================================================

    if (mesh.get_vertex_per_facet() != 3) {
        throw std::runtime_error("Input mesh must be a triangle mesh.");
    }

    // We need this things
    if (!mesh.is_connectivity_initialized()) {
        mesh.initialize_connectivity();
    }
    if (!mesh.has_facet_attribute("normal")) {
        compute_triangle_normal(mesh);
    }

    // This would be the final value that will be returned
    std::vector<bool> is_facet_selected(mesh.get_num_facets(), false);

    // Normals (and the seed normal)
    const AttributeArray& facet_normals = mesh.get_facet_attribute("normal");
    const VertexType seed_n = facet_normals.row(seed_facet_id);

    // DFS search utility
    std::vector<bool> is_facet_processed(mesh.get_num_facets(), false);
    std::deque<Index> search_queue;

    //
    // (0): Add the seed neighbors to the queue to initialize the queue
    //
    is_facet_processed[seed_facet_id] = true;
    is_facet_selected[seed_facet_id] = true;
    {
        const IndexList& facets_adjacent_to_seed = mesh.get_facets_adjacent_to_facet(seed_facet_id);
        for (Index j = 0; j < static_cast<Index>(facets_adjacent_to_seed.size()); j++) {
            const Index ne_fid = facets_adjacent_to_seed[j];
            const VertexType ne_n = facet_normals.row(ne_fid);

            if ((!is_facet_processed[ne_fid]) && is_facet_selectable(ne_fid)) {
                Scalar error = normal_direction_error(seed_n, ne_n);
                if (error < parameters.flood_error_limit) {
                    is_facet_selected[ne_fid] = true;
                    search_queue.push_back(ne_fid);
                }
            }
        } // End of for over neighbours of seed
    } // end of step 0

    //
    // (1): Run through all neighbors, process them, and push them into a queue
    // for further flood
    //
    while (!search_queue.empty()) {
        Index fid = invalid<Index>();
        if (parameters.search_type == Parameters::SearchType::DFS) {
            fid = search_queue.front();
            search_queue.pop_front();
        } else {
            fid = search_queue.back();
            search_queue.pop_back();
        }

        const VertexType center_n = facet_normals.row(fid);


        const IndexList& facets_adjacent_to_candidate = mesh.get_facets_adjacent_to_facet(fid);
        for (Index j = 0; j < static_cast<Index>(facets_adjacent_to_candidate.size()); j++) {
            const Index ne_fid = facets_adjacent_to_candidate[j];
            const VertexType ne_n = facet_normals.row(ne_fid);

            if ((!is_facet_processed[ne_fid]) && is_facet_selectable(ne_fid)) {
                const Scalar error_1 = normal_direction_error(seed_n, ne_n);
                const Scalar error_2 = normal_direction_error(center_n, ne_n);
                is_facet_processed[ne_fid] = true;

                if ((error_1 < parameters.flood_error_limit) &&
                    (error_2 < parameters.flood_error_limit)) {
                    is_facet_selected[ne_fid] = true;
                    search_queue.push_back(ne_fid);
                } else if (
                    error_2 < parameters.flood_error_limit *
                                  parameters.flood_second_to_first_order_limit_ratio) {
                    // Second order approximation
                    is_facet_selected[ne_fid] = true;
                    search_queue.push_back(ne_fid);
                }
            } // end of is neighbour selectable
        } // end of loops over neighbours
    } // end of dfs stack

    //
    // Smooth the selection
    //
    if (parameters.should_smooth_boundary) {
        for (Index st_iter_2 = 0; st_iter_2 < parameters.num_smooth_iterations; st_iter_2++) {
            // Go through boundary faces - check spikes
            for (Index i = 0; i < mesh.get_num_facets(); i++) {
                const IndexList& facets_adjacent_to_candidate =
                    mesh.get_facets_adjacent_to_facet(i);

                if (is_facet_selectable(i) && (facets_adjacent_to_candidate.size() > 2)) {
                    const bool select_flag = is_facet_selected[i];
                    Index neighbor_diff_flag = 0;

                    const Index ne_fid_0 = facets_adjacent_to_candidate[0];
                    const bool ne_select_0 = is_facet_selected[ne_fid_0];
                    if (select_flag != ne_select_0) neighbor_diff_flag++;

                    const Index ne_fid_1 = facets_adjacent_to_candidate[1];
                    const bool ne_select_1 = is_facet_selected[ne_fid_1];
                    if (select_flag != ne_select_1) neighbor_diff_flag++;

                    const Index ne_fid_2 = facets_adjacent_to_candidate[2];
                    const bool ne_select_2 = is_facet_selected[ne_fid_2];
                    if (select_flag != ne_select_2) neighbor_diff_flag++;

                    if (neighbor_diff_flag > 1) {
                        const VertexType self_n = facet_normals.row(i);
                        const VertexType ne_n_0 = facet_normals.row(ne_fid_0);
                        const VertexType ne_n_1 = facet_normals.row(ne_fid_1);
                        const VertexType ne_n_2 = facet_normals.row(ne_fid_2);

                        const Scalar e_0 = normal_direction_error(self_n, ne_n_0);
                        const Scalar e_1 = normal_direction_error(self_n, ne_n_1);
                        const Scalar e_2 = normal_direction_error(self_n, ne_n_2);

                        const bool e_0_ok = e_0 < parameters.flood_error_limit;
                        const bool e_1_ok = e_1 < parameters.flood_error_limit;
                        const bool e_2_ok = e_2 < parameters.flood_error_limit;

                        if (ne_select_0 && ne_select_1 && (e_0_ok || e_1_ok)) {
                            is_facet_selected[i] = true;
                        } else if (ne_select_1 && ne_select_2 && (e_1_ok || e_2_ok)) {
                            is_facet_selected[i] = true;
                        } else if (ne_select_2 && ne_select_0 && (e_2_ok || e_0_ok)) {
                            is_facet_selected[i] = true;
                        }
                    }
                } // end of non selected facet that has two selectable neighbours
            } // end of loop over faces
        } // end of for(num_smooth_iterations)
    } // end of should smooth boundary


    return is_facet_selected;

} // select_faces_by_normal_similarity



} // namespace legacy
} // namespace lagrange
