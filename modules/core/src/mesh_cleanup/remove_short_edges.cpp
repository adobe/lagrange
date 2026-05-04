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

#include <lagrange/Attribute.h>
#include <lagrange/AttributeFwd.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/compute_dihedral_angles.h>
#include <lagrange/compute_edge_lengths.h>
#include <lagrange/compute_facet_normal.h>
#include <lagrange/internal/constants.h>
#include <lagrange/mesh_cleanup/remove_isolated_vertices.h>
#include <lagrange/mesh_cleanup/remove_short_edges.h>
#include <lagrange/mesh_cleanup/remove_topologically_degenerate_facets.h>
#include <lagrange/remap_vertices.h>
#include <lagrange/utils/DisjointSets.h>
#include <lagrange/utils/SmallVector.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/geometry3d.h>
#include <lagrange/utils/invalid.h>
#include <lagrange/utils/scope_guard.h>
#include <lagrange/views.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <tbb/parallel_for.h>
#include <tbb/parallel_sort.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace lagrange {

namespace {

///
/// Compute vertex importance for edge collapse decisions.
/// Boundary vertices are more important than interior vertices.
/// Among boundary vertices, those with larger boundary angles are more important.
/// Among interior vertices, those with larger max absolute dihedral angles are more important.
///
template <typename Scalar, typename Index>
AttributeId compute_vertex_importance_for_collapse(
    SurfaceMesh<Scalar, Index>& mesh,
    AttributeId dihedral_id,
    std::string_view output_attribute_name)
{
    mesh.initialize_edges();
    const Index num_vertices = mesh.get_num_vertices();

    // Create output attribute
    auto importance_id = mesh.template create_attribute<Scalar>(
        output_attribute_name,
        AttributeElement::Vertex,
        AttributeUsage::Scalar,
        1);
    auto importance = attribute_vector_ref<Scalar>(mesh, importance_id);

    auto edge_dihedrals = attribute_vector_view<Scalar>(mesh, dihedral_id);
    auto vertices = vertex_view(mesh);

    // Compute importance for each vertex
    tbb::parallel_for(Index(0), num_vertices, [&](Index v) {
        SmallVector<Index, 2> boundary_edges;

        // Check if vertex is on boundary
        bool on_boundary = false;
        mesh.foreach_edge_around_vertex_with_duplicates(v, [&](Index e) {
            if (mesh.is_boundary_edge(e)) {
                boundary_edges.push_back(e);
                on_boundary = true;
            }
        });

        if (on_boundary) {
            // Boundary vertex: importance = 1000 + boundary_edge_angle

            Scalar boundary_angle;
            if (boundary_edges.size() == 2) {
                // Get the other vertices of the two boundary edges
                auto [e0_v0, e0_v1] = mesh.get_edge_vertices(boundary_edges[0]);
                auto [e1_v0, e1_v1] = mesh.get_edge_vertices(boundary_edges[1]);
                Index v_other0 = (e0_v0 == v) ? e0_v1 : e0_v0;
                Index v_other1 = (e1_v0 == v) ? e1_v1 : e1_v0;

                // Compute angle between the two boundary edges
                Eigen::Vector3<Scalar> vec0 = vertices.row(v_other0) - vertices.row(v);
                Eigen::Vector3<Scalar> vec1 = vertices.row(v_other1) - vertices.row(v);
                boundary_angle = angle_between(vec0, vec1);
            } else {
                // For vertices with != 2 boundary edges, use a default large angle
                boundary_angle = Scalar(lagrange::internal::pi);
            }

            importance[v] = Scalar(1000) + boundary_angle;
        } else {
            // Interior vertex: importance = sum_abs_dihedral
            Scalar sum_abs_dihedral = 0;
            mesh.foreach_edge_around_vertex_with_duplicates(v, [&](Index e) {
                Scalar abs_dihedral = std::abs(edge_dihedrals[e]);
                sum_abs_dihedral += abs_dihedral;
            });

            importance[v] = sum_abs_dihedral;
        }
    });

    return importance_id;
}

///
/// Check whether collapsing an edge (removing @p remove, keeping @p keep) would rotate any
/// 1-ring facet's normal by more than the allowed threshold.
///
/// @param mesh             Input mesh (edges must be initialized).
/// @param keep             Vertex to retain after the collapse.
/// @param remove           Vertex to eliminate after the collapse.
/// @param cos_threshold    cos(max_normal_deviation_angle). The collapse is rejected for a facet
///                         when dot(n_before, n_after) < cos_threshold * |n_before| * |n_after|.
/// @param facet_normal_id  Attribute id of the per-facet normal attribute. When valid, the
///                         stored (unit) normals are used as n_before, giving well-defined
///                         directions even for nearly-degenerate facets. When invalid, n_before
///                         is computed on the fly via Newell's method.
///
/// @returns true if the collapse is safe, false if it should be skipped.
///
template <typename Scalar, typename Index>
bool is_collapse_normal_valid(
    const SurfaceMesh<Scalar, Index>& mesh,
    Index keep,
    Index remove,
    Scalar cos_threshold,
    AttributeId facet_normal_id)
{
    auto vertices = vertex_view(mesh);
    const Eigen::Vector3<Scalar> keep_pos = vertices.row(keep);
    bool valid = true;

    const bool use_normal_attr = facet_normal_id != invalid<AttributeId>();

    mesh.foreach_facet_around_vertex(remove, [&](Index fi) {
        if (!valid) return;
        const Index num_corners = mesh.get_facet_size(fi);
        if (num_corners < 3) return;

        // Facets that also contain 'keep' become degenerate after the collapse and
        // are cleaned up by remove_topologically_degenerate_facets — skip them.
        for (Index lv = 0; lv < num_corners; ++lv) {
            if (mesh.get_facet_vertex(fi, lv) == keep) return;
        }

        // n_before: use the stored facet normal when available (well-defined even for
        // nearly-degenerate geometry), otherwise fall back to Newell's method.
        Eigen::Vector3<Scalar> n_before;
        if (use_normal_attr) {
            n_before = attribute_matrix_view<Scalar>(mesh, facet_normal_id).row(fi);
        } else {
            n_before = Eigen::Vector3<Scalar>::Zero();
            for (Index lv = 0; lv < num_corners; ++lv) {
                const Index va = mesh.get_facet_vertex(fi, lv);
                const Index vb = mesh.get_facet_vertex(fi, (lv + 1) % num_corners);
                n_before += Eigen::Vector3<Scalar>(vertices.row(va))
                                .cross(Eigen::Vector3<Scalar>(vertices.row(vb)));
            }
            // Skip facets with no meaningful normal.
            if (n_before.norm() < Scalar(1e-10)) return;
        }

        // n_after: always simulated via Newell's method with 'remove' moved to keep_pos.
        Eigen::Vector3<Scalar> n_after = Eigen::Vector3<Scalar>::Zero();
        for (Index lv = 0; lv < num_corners; ++lv) {
            const Index va = mesh.get_facet_vertex(fi, lv);
            const Index vb = mesh.get_facet_vertex(fi, (lv + 1) % num_corners);
            const Eigen::Vector3<Scalar> a =
                (va == remove) ? keep_pos : Eigen::Vector3<Scalar>(vertices.row(va));
            const Eigen::Vector3<Scalar> b =
                (vb == remove) ? keep_pos : Eigen::Vector3<Scalar>(vertices.row(vb));
            n_after += a.cross(b);
        }

        const Scalar len_after = n_after.norm();
        // Skip facets that become degenerate after the collapse since their normal directions are
        // not meaningful.
        if (len_after < Scalar(1e-10)) return;

        // Reject if the normal rotates beyond the allowed angle.
        // Written as a product to avoid normalizing the vectors.
        const Scalar len_before = n_before.norm();
        if (n_before.dot(n_after) < cos_threshold * len_before * len_after) {
            valid = false;
        }
    });

    return valid;
}

} // namespace

template <typename Scalar, typename Index>
void remove_short_edges(SurfaceMesh<Scalar, Index>& mesh, const RemoveShortEdgesOptions& options)
{
    // Pre-allocate reusable buffers to avoid repeated allocations in inner loops
    DisjointSets<Index> vertex_map;
    std::vector<bool> vertex_processed;
    std::vector<bool>
        facet_touched; // tracks which facets already have one vertex updated this batch
    std::vector<std::pair<Scalar, Index>> short_edges; // (length, edge_id)
    std::vector<Index> index_map;

    AttributeId edge_length_id = invalid<AttributeId>();
    AttributeId dihedral_id = invalid<AttributeId>();
    AttributeId importance_id = invalid<AttributeId>();
    AttributeId facet_normal_id = invalid<AttributeId>();
    bool importance_existed = false;
    std::string temp_importance_name;

    // Check if user-provided importance attribute exists
    if (!options.vertex_importance_attribute_name.empty() &&
        mesh.has_attribute(options.vertex_importance_attribute_name)) {
        importance_id = mesh.get_attribute_id(options.vertex_importance_attribute_name);
        importance_existed = true;

        // Validate attribute
        const auto& attr = mesh.get_attribute_base(importance_id);
        if (attr.get_element_type() != AttributeElement::Vertex) {
            throw std::runtime_error("Vertex importance attribute must be a per-vertex attribute");
        }
    } else {
        // Compute dihedral angles (reuses attribute if it exists)
        DihedralAngleOptions dihedral_options;
        dihedral_options.keep_facet_normals = false;
        dihedral_id = compute_dihedral_angles(mesh, dihedral_options);

        if (options.vertex_importance_attribute_name.empty()) {
            temp_importance_name = "@vertex_collapse_importance";
        } else {
            temp_importance_name = std::string(options.vertex_importance_attribute_name);
        }

        importance_id =
            compute_vertex_importance_for_collapse(mesh, dihedral_id, temp_importance_name);
    }

    const Scalar cos_normal_threshold =
        static_cast<Scalar>(std::cos(options.max_normal_deviation_angle));

    // Compute per-facet normals for the normal-flip guard (reuses attribute if it exists).
    // Skipped when the guard is disabled (cos_threshold == -1, i.e. angle == pi).
    if (cos_normal_threshold > Scalar(-1)) {
        facet_normal_id = compute_facet_normal(mesh);
    }

    // Ensure all temporary attributes are deleted on exit, regardless of how the loop terminates.
    auto cleanup = make_scope_guard([&] {
        if (edge_length_id != invalid<AttributeId>()) {
            mesh.delete_attribute(edge_length_id);
        }
        if (facet_normal_id != invalid<AttributeId>()) {
            mesh.delete_attribute(facet_normal_id);
        }
        if (dihedral_id != invalid<AttributeId>()) {
            mesh.delete_attribute(dihedral_id);
        }
        if (importance_id != invalid<AttributeId>() && !importance_existed) {
            mesh.delete_attribute(importance_id);
        }
    });

    while (true) {
        mesh.initialize_edges();
        const Index num_vertices = mesh.get_num_vertices();
        const Index num_edges = mesh.get_num_edges();

        if (num_edges == 0) break;

        // Compute edge lengths (reuses attribute if it exists)
        edge_length_id = compute_edge_lengths(mesh);

        // The facet normal should haven been propogated to the updated mesh.
        if (cos_normal_threshold > Scalar(-1)) {
            la_debug_assert(facet_normal_id != invalid<AttributeId>());
        }
        auto edge_lengths = attribute_vector_view<Scalar>(mesh, edge_length_id);

        // Find all short edges first
        // Reserve capacity to avoid reallocations
        short_edges.clear();
        short_edges.reserve(num_edges / 10); // Estimate ~10% short edges
        for (Index eid = 0; eid < num_edges; eid++) {
            if (edge_lengths[eid] <= static_cast<Scalar>(options.threshold)) {
                short_edges.emplace_back(edge_lengths[eid], eid);
            }
        }

        if (short_edges.empty()) break;

        // Access importance values
        auto vertex_importance = attribute_vector_view<Scalar>(mesh, importance_id);

        // Sort by length (ascending) - shorter edges have higher priority
        tbb::parallel_sort(short_edges.begin(), short_edges.end());

        // Process edges: mark vertices for collapse and build vertex mapping
        const Index num_facets = mesh.get_num_facets();
        vertex_processed.assign(num_vertices, false);
        facet_touched.assign(num_facets, false);
        vertex_map.init(num_vertices);

        bool has_collapse = false;
        for (const auto& [length, eid] : short_edges) {
            auto [v0, v1] = mesh.get_edge_vertices(eid);

            // Skip degenerate edges
            if (v0 == v1) continue;

            // Only collapse if neither vertex has participated in a collapse this iteration
            if (!vertex_processed[v0] && !vertex_processed[v1]) {
                // Special case: both vertices on boundary, but edge is interior
                // This would create a topologically invalid collapse, so skip it
                bool edge_boundary = mesh.is_boundary_edge(eid);
                if (!edge_boundary) {
                    // Check if both vertices are on boundary
                    bool v0_boundary = false;
                    bool v1_boundary = false;
                    mesh.foreach_edge_around_vertex_with_duplicates(v0, [&](Index e) {
                        if (mesh.is_boundary_edge(e)) v0_boundary = true;
                    });
                    mesh.foreach_edge_around_vertex_with_duplicates(v1, [&](Index e) {
                        if (mesh.is_boundary_edge(e)) v1_boundary = true;
                    });

                    if (v0_boundary && v1_boundary) {
                        continue; // Skip this collapse
                    }
                }

                // Use importance to decide which vertex to keep
                Index keep, remove;
                if (vertex_importance[v0] >= vertex_importance[v1]) {
                    keep = v0;
                    remove = v1;
                } else {
                    keep = v1;
                    remove = v0;
                }

                // Batch-conflict guard: skip if any surviving facet around 'remove'
                // already has a vertex being updated by an earlier collapse this batch.
                // This ensures at most one vertex update per facet per batch iteration,
                // preventing combined flips invisible to the per-edge normal check.
                bool has_facet_conflict = false;
                mesh.foreach_facet_around_vertex(remove, [&](Index fi) {
                    if (has_facet_conflict) return;
                    // Facets that contain 'keep' become degenerate — skip them.
                    for (Index lv = 0; lv < mesh.get_facet_size(fi); ++lv) {
                        if (mesh.get_facet_vertex(fi, lv) == keep) return;
                    }
                    if (facet_touched[fi]) has_facet_conflict = true;
                });
                if (has_facet_conflict) continue;

                // Normal-flip guard: skip the collapse if it would rotate any 1-ring
                // facet's normal by more than max_normal_deviation_angle.
                if (!is_collapse_normal_valid(
                        mesh,
                        keep,
                        remove,
                        cos_normal_threshold,
                        facet_normal_id)) {
                    continue;
                }

                vertex_map.merge(keep, remove);
                vertex_processed[v0] = true;
                vertex_processed[v1] = true;
                has_collapse = true;

                // Mark surviving facets around 'remove' as touched so no second
                // collapse modifies them within this batch.
                mesh.foreach_facet_around_vertex(remove, [&](Index fi) {
                    for (Index lv = 0; lv < mesh.get_facet_size(fi); ++lv) {
                        if (mesh.get_facet_vertex(fi, lv) == keep) return;
                    }
                    facet_touched[fi] = true;
                });
            }
        }

        if (!has_collapse) break;

        // Update positions to precisely control the remapped vertex locations after collapse
        auto vertices = vertex_ref(mesh);
        for (Index vi = 0; vi < num_vertices; vi++) {
            Index v_rep = vertex_map.find(vi);
            vertices.row(vi) = vertices.row(v_rep);
        }

        // Compact vertex mapping to ensure contiguous vertex IDs after collapse
        index_map.assign(num_vertices, invalid<Index>());
        vertex_map.extract_disjoint_set_indices(index_map);

        // Batch collapse using remap_vertices
        remap_vertices(mesh, {index_map.data(), index_map.size()});
    }

    // Clean up topologically degenerate facets and isolated vertices
    remove_topologically_degenerate_facets(mesh);
    remove_isolated_vertices(mesh);
}

template <typename Scalar, typename Index>
void remove_short_edges(SurfaceMesh<Scalar, Index>& mesh, Scalar threshold)
{
    RemoveShortEdgesOptions options;
    options.threshold = static_cast<double>(threshold);
    remove_short_edges(mesh, options);
}

#define LA_X_remove_short_edges(_, Scalar, Index)                \
    template LA_CORE_API void remove_short_edges<Scalar, Index>( \
        SurfaceMesh<Scalar, Index>&,                             \
        Scalar);                                                 \
    template LA_CORE_API void remove_short_edges<Scalar, Index>( \
        SurfaceMesh<Scalar, Index>&,                             \
        const RemoveShortEdgesOptions&);
LA_SURFACE_MESH_X(remove_short_edges, 0)

} // namespace lagrange
