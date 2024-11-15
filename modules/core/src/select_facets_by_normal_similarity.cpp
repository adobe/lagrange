/*
 * Copyright 2024 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */

#include <lagrange/select_facets_by_normal_similarity.h>

#include <lagrange/Attribute.h>
#include <lagrange/Attributes.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/compute_facet_normal.h>
#include <lagrange/internal/find_attribute_utils.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/invalid.h>
#include <lagrange/views.h>

namespace lagrange {

template <typename Scalar, typename Index>
AttributeId select_facets_by_normal_similarity(
    SurfaceMesh<Scalar, Index>& mesh,
    const Index seed_facet_id,
    const SelectFacetsByNormalSimilarityOptions& options)
{
    // ===================================================
    //     Helper typedef and lambdas
    // ===================================================

    using Vector3D = typename Eigen::Vector3<Scalar>;

    const Index num_facets = mesh.get_num_facets();
    const Scalar flood_error_limit = static_cast<Scalar>(options.flood_error_limit);
    const Scalar flood_second_to_first_order_limit_ratio =
        static_cast<Scalar>(options.flood_second_to_first_order_limit_ratio);

    // Check if a face is selectable
    std::function<bool(const Index)> is_facet_selectable;
    if (options.is_facet_selectable_attribute_name.has_value()) {
        AttributeId id = internal::find_attribute<uint8_t>(
            mesh,
            options.is_facet_selectable_attribute_name.value(),
            Facet,
            AttributeUsage::Scalar,
            /* number of channels */ 1);
        la_debug_assert(id != invalid_attribute_id());
        lagrange::span<const uint8_t> attr_view =
            mesh.template get_attribute<uint8_t>(id).get_all();
        is_facet_selectable = [attr_view, num_facets](const Index facet_id) -> bool {
            la_runtime_assert(facet_id < num_facets);
            return static_cast<bool>(attr_view[facet_id]);
        };
    } else {
        is_facet_selectable = [](const Index) -> bool { return true; };
    }

    // The energy between two normals
    auto normal_direction_error = [](const Vector3D& n1, const Vector3D& n2) -> Scalar {
        // We assume normals from compute_facet_normal are normalized
        return static_cast<Scalar>(1.0) - std::abs(n1.dot(n2));
    };

    // ===================================================
    //     Let's begin
    //     current code just copied from
    //     Segmentation/SuperTriangulation/SingleFloodFromSeed()
    // ===================================================

    la_runtime_assert(
        mesh.is_triangle_mesh(),
        "At the moment, select_facets_by_normal_similarity only supports triangle meshes.");

    AttributeId id;
    if (mesh.has_attribute(options.facet_normal_attribute_name)) {
        id = mesh.get_attribute_id(options.facet_normal_attribute_name);
    } else {
        FacetNormalOptions fn_options;
        fn_options.output_attribute_name = options.facet_normal_attribute_name;
        id = compute_facet_normal(mesh, fn_options);
    }
    auto facet_normals = attribute_matrix_view<Scalar>(mesh, id);
    const Vector3D seed_n = facet_normals.row(seed_facet_id);

    // This would be the final value that will be returned
    AttributeId selected_facet_id = internal::find_or_create_attribute<uint8_t>(
        mesh,
        options.output_attribute_name,
        Facet,
        AttributeUsage::Scalar,
        /* number of channels */ 1,
        internal::ResetToDefault::Yes);
    lagrange::span<uint8_t> is_facet_selected =
        mesh.template ref_attribute<uint8_t>(selected_facet_id).ref_all();

    // DFS search utility
    std::vector<bool> is_facet_processed(num_facets, false);
    std::deque<Index> search_queue;

    //
    // (0): Add the seed neighbors to the queue to initialize the queue
    //
    is_facet_processed[seed_facet_id] = true;
    is_facet_selected[seed_facet_id] = true;
    {
        auto process_neighboring_facet = [&](const Index ne_fid) {
            const Vector3D ne_n = facet_normals.row(ne_fid);

            if ((!is_facet_processed[ne_fid]) && is_facet_selectable(ne_fid)) {
                Scalar error = normal_direction_error(seed_n, ne_n);
                if (error < flood_error_limit) {
                    is_facet_selected[ne_fid] = true;
                    search_queue.push_back(ne_fid);
                }
            }
        };
        mesh.initialize_edges(); // required to loop over edge neighbors
        mesh.foreach_facet_around_facet(seed_facet_id, process_neighboring_facet);
    } // end of step 0

    //
    // (1): Run through all neighbors, process them, and push them into a queue
    // for further flood
    //
    while (!search_queue.empty()) {
        Index fid = invalid<Index>(); // set to invalid before assignment
        if (options.search_type == SelectFacetsByNormalSimilarityOptions::SearchType::DFS) {
            // depth first: first in, first out
            fid = search_queue.front();
            search_queue.pop_front();
        } else {
            // breadth first: last in, first out
            fid = search_queue.back();
            search_queue.pop_back();
        }

        const Vector3D center_n =
            facet_normals.row(fid); // center_n must be similar to seed_n, fid must be selected
        auto process_neighboring_facet = [&](const Index ne_fid) {
            const Vector3D ne_n = facet_normals.row(ne_fid);

            if ((!is_facet_processed[ne_fid]) && is_facet_selectable(ne_fid)) {
                const Scalar error_1 = normal_direction_error(seed_n, ne_n);
                const Scalar error_2 = normal_direction_error(center_n, ne_n);
                is_facet_processed[ne_fid] = true;

                if ((error_1 < flood_error_limit) && (error_2 < flood_error_limit)) {
                    // center_n ~ ne_n ~ seed_n
                    is_facet_selected[ne_fid] = true;
                    search_queue.push_back(ne_fid);
                } else if (error_2 < flood_error_limit * flood_second_to_first_order_limit_ratio) {
                    // center_n ~ ne_n !~ seed_n
                    // Second order approximation
                    is_facet_selected[ne_fid] = true;
                    search_queue.push_back(ne_fid);
                }
            } // end of is neighbour selectable
        }; // end of loops over neighbours
        mesh.foreach_facet_around_facet(fid, process_neighboring_facet);
    } // end of DFS/BFS stack

    //
    // Smooth the selection by selecting more facets
    // We select additional facets if more than half of its neighboring facets agree
    //
    for (Index st_iter_2 = 0; st_iter_2 < static_cast<Index>(options.num_smooth_iterations);
         st_iter_2++) {
        // Go through boundary faces - check spikes
        for (Index i = 0; i < num_facets; i++) {
            // We consider adding facet i if it wasn't already selected and is selectable
            if (!is_facet_selected[i] && is_facet_selectable(i)) {
                const Vector3D self_n = facet_normals.row(i);
                Index agreeing_neighbors = 0;
                Index num_neighbors = 0;
                auto record_neighbor_votes = [&](const Index ne_fid) {
                    num_neighbors++;
                    if (is_facet_selected[ne_fid]) { // neighbor can only vote if its selected
                        const Vector3D ne_n = facet_normals.row(ne_fid);
                        if (normal_direction_error(self_n, ne_n) < flood_error_limit) {
                            agreeing_neighbors++;
                        }
                    }
                };
                mesh.foreach_facet_around_facet(i, record_neighbor_votes);
                // an n-polygon can have <= n neighboring facets (could be a boundary facet)
                la_debug_assert(num_neighbors <= mesh.get_facet_size(i));

                if (agreeing_neighbors > num_neighbors / 2) {
                    is_facet_selected[i] = true;
                }
            }
        } // end of loop over faces
    } // end of for(num_smooth_iterations)


    return selected_facet_id;

} // select_faces_by_normal_similarity

#define LA_X_select_facets_by_normal_similarity(_, Scalar, Index)        \
    template LA_CORE_API AttributeId select_facets_by_normal_similarity( \
        SurfaceMesh<Scalar, Index>& mesh,                                \
        const Index seed_facet_id,                                       \
        const SelectFacetsByNormalSimilarityOptions& options);
LA_SURFACE_MESH_X(select_facets_by_normal_similarity, 0)

} // namespace lagrange
