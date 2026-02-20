/*
 * Copyright 2026 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */

#include <lagrange/bvh/remove_interior_shells.h>

#include <lagrange/Attribute.h>
#include <lagrange/ExactPredicatesShewchuk.h>
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMesh.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/bvh/AABB.h>
#include <lagrange/bvh/api.h>
#include <lagrange/compute_components.h>
#include <lagrange/extract_submesh.h>
#include <lagrange/utils/assert.h>
#include <lagrange/views.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <tbb/parallel_for.h>
#include <tbb/enumerable_thread_specific.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <spdlog/fmt/ranges.h>
#include <Eigen/Geometry>

#include <optional>
#include <stack>

namespace lagrange::bvh {

namespace {

// Intersect a 3D segment with a plane Z=c. While not exact, this will always produce watertight
// polygons for closed shells. It is possible that floating point rounding causes 2D polygons to
// intersect if they are *very* close to begin with. But in practice this should not be an issue.
Eigen::Vector2d
segment_plane_intersection(const Eigen::Vector3d& a, const Eigen::Vector3d& b, double z)
{
    if (a.z() < b.z()) {
        const double t = (z - b.z()) / (a.z() - b.z());
        return (t * a + (1.0 - t) * b).head<2>();
    } else {
        const double t = (z - a.z()) / (b.z() - a.z());
        return (t * b + (1.0 - t) * a).head<2>();
    }
}

std::optional<std::array<Eigen::Vector2d, 2>> facet_plane_intersection(
    const Eigen::RowVector3d& a,
    const Eigen::RowVector3d& b,
    const Eigen::RowVector3d& c,
    double z)
{
    const int sa = (a.z() > z ? 1 : -1);
    const int sb = (b.z() > z ? 1 : -1);
    const int sc = (c.z() > z ? 1 : -1);

    if (sa * sb < 0 && sb * sc < 0) {
        const auto u = segment_plane_intersection(a, b, z);
        const auto v = segment_plane_intersection(b, c, z);
        return {{u, v}};
    } else if (sb * sc < 0 && sc * sa < 0) {
        const auto u = segment_plane_intersection(b, c, z);
        const auto v = segment_plane_intersection(c, a, z);
        return {{u, v}};
    } else if (sc * sa < 0 && sa * sb < 0) {
        const auto u = segment_plane_intersection(c, a, z);
        const auto v = segment_plane_intersection(a, b, z);
        return {{u, v}};
    } else {
        // No intersection with this triangle
        return std::nullopt;
    }
}

} // namespace

template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> remove_interior_shells(const SurfaceMesh<Scalar, Index>& mesh_)
{
    // Only implemented for triangles meshes for now
    la_runtime_assert(
        mesh_.is_triangle_mesh(),
        "remove_interior_shells: Input mesh must be a triangle mesh.");
    la_runtime_assert(mesh_.get_dimension() == 3, "remove_interior_shells: Input mesh must be 3D.");

    // 1. Compute connected components and facet groups
    logger().debug("[remove_interior_shells] Computing connected components...");
    SurfaceMesh<Scalar, Index> mesh = mesh_;
    ComponentOptions comp_options;
    comp_options.connectivity_type = ComponentOptions::ConnectivityType::Vertex;
    size_t num_components = compute_components(mesh, comp_options);
    auto component_indices =
        mesh.template get_attribute<Index>(comp_options.output_attribute_name).get_all();

    // 2. Pick representative vertex for each component
    logger().debug("[remove_interior_shells] Computing representative vertices...");
    auto vertices = vertex_view(mesh);
    auto facets = facet_view(mesh);
    std::vector<Index> representative_vertices(num_components, invalid<Index>());
    std::vector<Eigen::Vector3d> representative_positions(num_components);
    for (Index f = 0; f < mesh.get_num_facets(); f++) {
        Index comp_id = component_indices[f];
        if (representative_vertices[comp_id] == invalid<Index>()) {
            Index v0 = mesh.get_facet_vertex(f, 0);
            representative_vertices[comp_id] = v0;
            representative_positions[comp_id] = vertices.row(v0).template cast<double>();
        }
    }

    // 3. Build AABB tree
    logger().debug("[remove_interior_shells] Building repr BVH...");
    using Tree = AABB<Scalar, 2>;
    using Box2d = typename Tree::Box;
    AABB<Scalar, 2> bvh;
    std::vector<Box2d> boxes(num_components);
    for (Index i = 0; i < num_components; i++) {
        Index v = representative_vertices[i];
        boxes[i] = Box2d(vertices.row(v).template tail<2>()); // YZ coords
    }
    bvh.build(boxes);

    // 4. Iterate over triangles in parallel
    //
    // For each triangle, we find all representative vertices that lie within its Z bounds,
    // and cast a ray from each representative vertex to +X direction, counting intersections.
    //
    // We use the standard "point in polygon" ray casting algorithm to determine which
    // representative vertices are inside which shells. We use exact predicates to ensure
    // robustness.
    //
    logger().debug("[remove_interior_shells] Casting rays...");
    struct Data
    {
        std::vector<uint32_t> intersecting_vertices;
    };
    tbb::enumerable_thread_specific<Data> data;
    ExactPredicatesShewchuk pred;
    Eigen::MatrixX<std::atomic_char> is_inside_of(num_components, num_components);
    std::fill_n(is_inside_of.data(), is_inside_of.size(), 0);
    using Box3d = Eigen::AlignedBox<Scalar, 3>;
    tbb::parallel_for(
        tbb::blocked_range<Index>(0, mesh.get_num_facets()),
        [&](const tbb::blocked_range<Index>& r) {
            auto& loc = data.local();
            for (Index f = r.begin(); f != r.end(); ++f) {
                const Index f_comp = component_indices[f];
                Box3d bbox;
                bbox.extend(vertices.row(facets(f, 0)).transpose().template head<3>());
                bbox.extend(vertices.row(facets(f, 1)).transpose().template head<3>());
                bbox.extend(vertices.row(facets(f, 2)).transpose().template head<3>());
                Box2d query(bbox.min().template tail<2>(), bbox.max().template tail<2>());
                bvh.intersect(query, loc.intersecting_vertices);
                for (uint32_t i : loc.intersecting_vertices) {
                    if (f_comp == static_cast<Index>(i)) {
                        continue; // Skip triangles from the same component
                    }
                    double vz = representative_positions[i].z();
                    auto res = facet_plane_intersection(
                        vertices.row(facets(f, 0)).template cast<double>(),
                        vertices.row(facets(f, 1)).template cast<double>(),
                        vertices.row(facets(f, 2)).template cast<double>(),
                        vz);
                    if (!res.has_value()) {
                        continue; // No intersection with this triangle
                    }
                    auto [pi, pj] = res.value();
                    auto v_pos = representative_positions[i].template head<2>();
                    if ((pi.y() > v_pos.y()) == (pj.y() > v_pos.y())) {
                        continue; // No intersection with horizontal ray
                    }
                    // Test whether v is to the *left* of the edge pi-pj
                    short o = pred.orient2D(pi.data(), pj.data(), v_pos.data());
                    if (pj.y() > pi.y()) {
                        // Predicates tell us about clockwise orientation of (pi, pj, v).
                        // We need to adjust the result depending on whether yi < yj.
                        o = -o;
                    }
                    if (o > 0) {
                        is_inside_of(i, f_comp).fetch_xor(1);
                    }
                }
            }
        });

    // 6. Iteratively prune shells that are inside other shells
    //
    // - Build adjacency lists of which shells contain which other shells
    // - We need to visit shells from the outermost to the innermost.
    // - When visiting a shell, we count the number of (visited) exterior shells that contain it.
    //   - If this number is odd, mark the shell as interior.
    //   - If this number is even, mark the shell as exterior.
    //   - Add any contained shell to the queue for shells to visit
    logger().debug("[remove_interior_shells] Pruning shells...");
    std::vector<bool> is_exterior(num_components, false);
    {
        // Build adjacency lists of which shells contain which other shells
        std::vector<std::vector<size_t>> contains(num_components);
        std::vector<std::vector<size_t>> contained_by(num_components);
        tbb::parallel_for(size_t(0), num_components, [&](size_t i) {
            for (size_t j = 0; j < num_components; ++j) {
                if (is_inside_of(i, j)) {
                    contained_by[i].push_back(j);
                }
                if (is_inside_of(j, i)) {
                    contains[i].push_back(j);
                }
            }
        });
        // Start traversal from outermost shells (not contained by any other)
        std::vector<bool> marked(num_components, false);
        std::stack<size_t> pending;
        for (size_t i = 0; i < num_components; ++i) {
            if (contained_by[i].empty()) {
                pending.push(i);
                marked[i] = true;
                is_exterior[i] = true;
            }
        }
        // Graph traversal
        while (!pending.empty()) {
            size_t curr = pending.top();
            pending.pop();
            // Mark current shell as exterior based on the number of exterior shells that contain it
            bool exterior = true;
            for (size_t j : contained_by[curr]) {
                if (is_exterior[j]) {
                    la_debug_assert(marked[j]);
                    exterior = !exterior;
                }
            }
            if (exterior) {
                is_exterior[curr] = true;
            }
            // Visit contained shells next
            for (size_t j : contains[curr]) {
                if (!marked[j]) {
                    pending.push(j);
                    marked[j] = true;
                }
            }
        }
    }

    // 7. Combine meshes from non-pruned exterior shells
    logger().debug("[remove_interior_shells] Selecting facets...");
    size_t remaining_components = num_components;
    for (Index i = 0; i < num_components; i++) {
        if (!is_exterior[i]) {
            remaining_components--;
        }
    }
    std::vector<Index> selected_facets;
    selected_facets.reserve(mesh.get_num_facets());
    for (Index f = 0; f < mesh.get_num_facets(); ++f) {
        if (is_exterior[component_indices[f]]) {
            selected_facets.push_back(f);
        }
    }

    logger().debug(
        "[remove_interior_shells] Removed {} interior shells. Remaining components: {}",
        num_components - remaining_components,
        remaining_components);

    return extract_submesh<Scalar, Index>(mesh, selected_facets);
}

#define LA_X_remove_interior_shells(_, Scalar, Index)                                     \
    template LA_BVH_API SurfaceMesh<Scalar, Index> remove_interior_shells<Scalar, Index>( \
        const SurfaceMesh<Scalar, Index>&);
LA_SURFACE_MESH_X(remove_interior_shells, 0)

} // namespace lagrange::bvh
