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
#include <lagrange/mesh_cleanup/split_obtuse_triangles.h>

#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/cast_attribute.h>
#include <lagrange/foreach_attribute.h>
#include <lagrange/internal/internal_angles.h>
#include <lagrange/internal/interpolate_attribute_row.h>
#include <lagrange/internal/split_edges.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/invalid.h>
#include <lagrange/utils/scope_guard.h>
#include <lagrange/utils/span.h>
#include <lagrange/views.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <Eigen/Dense>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <cmath>
#include <limits>
#include <tuple>
#include <vector>

namespace lagrange {

template <typename Scalar, typename Index>
size_t split_obtuse_triangles(SurfaceMesh<Scalar, Index>& mesh, SplitObtuseTrianglesOptions options)
{
    la_runtime_assert(mesh.is_triangle_mesh(), "Input mesh is not a triangle mesh.");

    constexpr std::string_view internal_active_region_attribute_name =
        "@__active_region_internal_obtuse__";

    const Scalar max_angle = static_cast<Scalar>(options.max_angle);
    la_runtime_assert(
        std::isfinite(max_angle) && max_angle > Scalar(0),
        "max_angle must be finite and positive.");
    if (options.max_iterations == 0) {
        constexpr Scalar pi_3 = static_cast<Scalar>(lagrange::internal::pi / 3.0);
        la_runtime_assert(
            max_angle > pi_3,
            "max_angle <= pi/3 can never be satisfied; use a finite max_iterations to avoid an "
            "infinite loop.");
    }

    const size_t max_iter =
        (options.max_iterations == 0) ? std::numeric_limits<size_t>::max() : options.max_iterations;

    size_t total_split = 0;

    // Buffers reused across iterations to avoid per-iteration allocations.
    struct EdgeSplit
    {
        Index v0 = invalid<Index>();
        Index v1 = invalid<Index>();
        Scalar t = Scalar(0.5);
        Scalar angle = Scalar(0);
        bool marked = false;
    };
    Eigen::Matrix<Scalar, Eigen::Dynamic, 3> angles;
    std::vector<EdgeSplit> edge_splits;
    std::vector<Scalar> new_coords;
    std::vector<std::tuple<Index, Index, Scalar>> vertex_sources;
    std::vector<Index> edge_to_new_vertex;

    bool converged = false;
    for (size_t iter = 0; iter < max_iter; ++iter) {
        logger().debug("[split_obtuse_triangles] iteration {}", iter);
        mesh.initialize_edges();

        AttributeId internal_active_attr_id = invalid<AttributeId>();
        const uint8_t* active_data = nullptr;
        if (!options.active_region_attribute.empty()) {
            la_runtime_assert(
                mesh.has_attribute(options.active_region_attribute),
                "active_region_attribute not found in mesh.");
            la_runtime_assert(
                mesh.get_attribute_base(options.active_region_attribute).get_element_type() ==
                    AttributeElement::Facet,
                "active_region_attribute must be a facet attribute.");
            la_runtime_assert(
                mesh.get_attribute_base(options.active_region_attribute).get_num_channels() == 1,
                "active_region_attribute must be a scalar (single-channel) attribute.");
            internal_active_attr_id = cast_attribute<uint8_t>(
                mesh,
                options.active_region_attribute,
                internal_active_region_attribute_name);
            active_data = attribute_vector_view<uint8_t>(mesh, internal_active_attr_id).data();
        }

        // Ensure the temporary internal attribute is removed when leaving the iteration,
        // even if the body throws.
        auto active_region_cleanup = make_scope_guard([&] {
            if (internal_active_attr_id != invalid<AttributeId>()) {
                mesh.delete_attribute(internal_active_region_attribute_name);
            }
        });

        auto is_facet_active = [&](Index fid) {
            if (active_data == nullptr) return true;
            return static_cast<bool>(active_data[fid]);
        };

        // Compute interior angles: angles(f, d) = angle at corner d of facet f.
        internal::internal_angles(vertex_view(mesh), facet_view(mesh), angles);

        const Index num_edges = mesh.get_num_edges();
        const Index num_facets = mesh.get_num_facets();
        const Index dim = mesh.get_dimension();
        const Index num_input_vertices = mesh.get_num_vertices();

        edge_splits.assign(num_edges, EdgeSplit{});

        auto vertices = vertex_view(mesh);
        auto facets = facet_view(mesh);

        for (Index f = 0; f < num_facets; ++f) {
            if (!is_facet_active(f)) continue;

            // Find local corner with the largest interior angle.
            Index d_star = 0;
            for (Index d = 1; d < 3; ++d) {
                if (angles(f, d) > angles(f, d_star)) d_star = d;
            }
            const Scalar max_a = angles(f, d_star);
            if (!(max_a > max_angle)) continue;

            // Edge opposite the obtuse corner: between corners (d_star+1) and (d_star+2),
            // i.e. local edge index (d_star + 1) % 3.
            const Index local_edge = (d_star + 1) % 3;
            const Index eid = mesh.get_edge(f, local_edge);

            if (edge_splits[eid].marked && edge_splits[eid].angle >= max_a) continue;

            const Index v_obtuse = facets(f, d_star);
            const Index v_a = facets(f, (d_star + 1) % 3);
            const Index v_b = facets(f, (d_star + 2) % 3);

            auto a = vertices.row(v_a);
            auto b = vertices.row(v_b);
            auto p = vertices.row(v_obtuse);
            const auto ba = (b - a).eval();
            const Scalar l2 = ba.squaredNorm();
            if (l2 <= Scalar(0)) continue; // Degenerate edge.

            // Skip collinear/degenerate facets: if the obtuse vertex lies on the opposite
            // edge, splitting cannot reduce the angle and would loop forever under
            // unbounded iteration.
            // Compute t first, then the perpendicular residual — avoids catastrophic
            // cancellation between two large squared norms.
            const auto pa = (p - a).eval();
            const Scalar t_raw = pa.dot(ba) / l2;
            const Scalar dist_sq = (pa - t_raw * ba).squaredNorm();
            if (dist_sq <= std::numeric_limits<Scalar>::epsilon() * l2) continue;

            constexpr Scalar clamp_eps = std::numeric_limits<Scalar>::epsilon() * Scalar(16);
            const Scalar t = !std::isfinite(t_raw)           ? Scalar(0.5)
                             : t_raw < clamp_eps             ? clamp_eps
                             : t_raw > Scalar(1) - clamp_eps ? Scalar(1) - clamp_eps
                                                             : t_raw;

            edge_splits[eid].v0 = v_a;
            edge_splits[eid].v1 = v_b;
            edge_splits[eid].t = t;
            edge_splits[eid].angle = max_a;
            edge_splits[eid].marked = true;
        }

        // Count marked edges.
        Index num_marked = 0;
        for (const auto& es : edge_splits) {
            if (es.marked) ++num_marked;
        }
        if (num_marked == 0) {
            converged = true;
            break;
        }

        // Allocate new vertices at split positions.
        new_coords.clear();
        new_coords.reserve(static_cast<size_t>(num_marked) * dim);
        vertex_sources.clear();
        vertex_sources.reserve(num_marked);

        edge_to_new_vertex.assign(num_edges, invalid<Index>());
        Index next_vertex_id = num_input_vertices;
        for (Index e = 0; e < num_edges; ++e) {
            if (!edge_splits[e].marked) continue;
            const Scalar t = edge_splits[e].t;
            const Index v0 = edge_splits[e].v0;
            const Index v1 = edge_splits[e].v1;
            auto p = ((Scalar(1) - t) * vertices.row(v0) + t * vertices.row(v1)).eval();
            new_coords.insert(new_coords.end(), p.data(), p.data() + dim);
            vertex_sources.emplace_back(v0, v1, t);
            edge_to_new_vertex[e] = next_vertex_id++;
        }

        mesh.add_vertices(num_marked, {new_coords.data(), new_coords.size()});

        // Interpolate vertex attributes for newly added vertices.
        par_foreach_named_attribute_write<AttributeElement::Vertex>(
            mesh,
            [&](std::string_view name, auto&& attr) {
                using ValueType = typename std::decay_t<decltype(attr)>::ValueType;
                if (mesh.attr_name_is_reserved(name)) return;
                auto data = matrix_ref(attr);
                static_assert(
                    std::is_same_v<ValueType, typename std::decay_t<decltype(data)>::Scalar>);
                for (size_t i = 0; i < vertex_sources.size(); ++i) {
                    const auto [v0, v1, t] = vertex_sources[i];
                    internal::interpolate_attribute_row(
                        data,
                        num_input_vertices + static_cast<Index>(i),
                        v0,
                        v1,
                        t);
                }
            });

        auto facets_to_remove = internal::split_edges(
            mesh,
            function_ref<span<Index>(Index)>([&](Index eid) -> span<Index> {
                if (!edge_splits[eid].marked) return span<Index>();
                return span<Index>(&edge_to_new_vertex[eid], 1);
            }),
            function_ref<bool(Index)>([](Index) { return true; }));

        total_split += facets_to_remove.size();
        mesh.remove_facets(facets_to_remove);
    }

    if (!converged && options.max_iterations > 0) {
        logger().warn(
            "[split_obtuse_triangles] did not converge after {} iterations.",
            options.max_iterations);
    }

    return total_split;
}

#define LA_X_split_obtuse_triangles(_, Scalar, Index)                  \
    template LA_CORE_API size_t split_obtuse_triangles<Scalar, Index>( \
        SurfaceMesh<Scalar, Index>&,                                   \
        SplitObtuseTrianglesOptions);
LA_SURFACE_MESH_X(split_obtuse_triangles, 0)

} // namespace lagrange
