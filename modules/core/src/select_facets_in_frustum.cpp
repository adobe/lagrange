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

#include <lagrange/select_facets_in_frustum.h>

#include <lagrange/Attribute.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/internal/find_attribute_utils.h>
#include <lagrange/utils/tbb.h>
#include <lagrange/views.h>

#include <tbb/blocked_range.h>
#include <tbb/enumerable_thread_specific.h>
#include <tbb/parallel_for.h>

#include <atomic>


namespace lagrange {

template <typename Scalar, typename Index>
bool select_facets_in_frustum(
    SurfaceMesh<Scalar, Index>& mesh,
    const Frustum<Scalar>& frustum,
    const FrustumSelectionOptions& options)
{
    using Vector3 = typename Eigen::Vector3<Scalar>;
    const Vector3 n0(frustum.planes[0].normal.data());
    const Vector3 n1(frustum.planes[1].normal.data());
    const Vector3 n2(frustum.planes[2].normal.data());
    const Vector3 n3(frustum.planes[3].normal.data());
    const Vector3 p0(frustum.planes[0].point.data());
    const Vector3 p1(frustum.planes[1].point.data());
    const Vector3 p2(frustum.planes[2].point.data());
    const Vector3 p3(frustum.planes[3].point.data());

    const Index num_facets = mesh.get_num_facets();

    // Per-thread buffers to store intermediate results.
    struct LocalBuffers
    {
        Vector3 v0, v1, v2; // Triangle vertices.
        Vector3 q0, q1, q2, q3; // Intermediate tet vertices.
        std::array<double, 2> r0, r1, r2; // Triangle projections.
    };
    tbb::enumerable_thread_specific<LocalBuffers> temp_vars;

    auto edge_overlap_with_negative_octant = [](const Vector3& q0, const Vector3& q1) -> bool {
        Scalar t_min = 0, t_max = 1;
        const Vector3 e = {q0[0] - q1[0], q0[1] - q1[1], q0[2] - q1[2]};
        if (e[0] > 0) {
            t_max = std::min(t_max, -q1[0] / e[0]);
        } else if (e[0] < 0) {
            t_min = std::max(t_min, -q1[0] / e[0]);
        } else {
            if (q1[0] >= 0) return false;
        }

        if (e[1] > 0) {
            t_max = std::min(t_max, -q1[1] / e[1]);
        } else if (e[1] < 0) {
            t_min = std::max(t_min, -q1[1] / e[1]);
        } else {
            if (q1[1] >= 0) return false;
        }

        if (e[2] > 0) {
            t_max = std::min(t_max, -q1[2] / e[2]);
        } else if (e[2] < 0) {
            t_min = std::max(t_min, -q1[2] / e[2]);
        } else {
            if (q1[2] >= 0) return false;
        }

        return t_max >= t_min;
    };

    auto compute_plane =
        [](const Vector3& q0, const Vector3& q1, const Vector3& q2) -> std::pair<Vector3, Scalar> {
        const Vector3 n = {
            q0[2] * (q2[1] - q1[1]) + q1[2] * (q0[1] - q2[1]) + q2[2] * (q1[1] - q0[1]),
            q0[0] * (q2[2] - q1[2]) + q1[0] * (q0[2] - q2[2]) + q2[0] * (q1[2] - q0[2]),
            q0[1] * (q2[0] - q1[0]) + q1[1] * (q0[0] - q2[0]) + q2[1] * (q1[0] - q0[0])};
        const Scalar c = (n.dot(q0) + n.dot(q1) + n.dot(q2)) / 3;
        return {n, c};
    };

    // Compute the orientation of 2D traingle (v0, v1, O), where O = (0, 0).
    auto orient2D_inexact = [](const std::array<double, 2>& v0,
                               const std::array<double, 2>& v1) -> int {
        const auto r = v0[0] * v1[1] - v0[1] * v1[0];
        if (r > 0) return 1;
        if (r < 0) return -1;
        return 0;
    };

    auto triangle_intersects_negative_axis = [&](const Vector3& q0,
                                                 const Vector3& q1,
                                                 const Vector3& q2,
                                                 const Vector3& n,
                                                 const Scalar c,
                                                 const int axis) -> bool {
        auto& r0 = temp_vars.local().r0;
        auto& r1 = temp_vars.local().r1;
        auto& r2 = temp_vars.local().r2;

        r0[0] = q0[(axis + 1) % 3];
        r0[1] = q0[(axis + 2) % 3];
        r1[0] = q1[(axis + 1) % 3];
        r1[1] = q1[(axis + 2) % 3];
        r2[0] = q2[(axis + 1) % 3];
        r2[1] = q2[(axis + 2) % 3];

        auto o01 = orient2D_inexact(r0, r1);
        auto o12 = orient2D_inexact(r1, r2);
        auto o20 = orient2D_inexact(r2, r0);
        if (o01 == o12 && o01 == o20) {
            if (o01 == 0) {
                // Triangle projection is degenerate.
                // Note that the case where axis is coplanar with the triangle
                // is treated as no intersection (which is debatale).
                return false;
            } else {
                // Triangle projection contains the origin.
                // Check axis intercept.
                return (c < 0 && n[axis] > 0) || (c > 0 && n[axis] < 0);
            }
        } else {
            // Note that we treat the case where the axis intersect triangle at
            // its boundary as no intersection.
            return false;
        }
    };

    auto triangle_intersects_negative_axes =
        [&](const Vector3& q0, const Vector3& q1, const Vector3& q2) -> bool {
        const auto r = compute_plane(q0, q1, q2);
        if (triangle_intersects_negative_axis(q0, q1, q2, r.first, r.second, 0)) return true;
        if (triangle_intersects_negative_axis(q0, q1, q2, r.first, r.second, 1)) return true;
        if (triangle_intersects_negative_axis(q0, q1, q2, r.first, r.second, 2)) return true;
        return false;
    };

    auto tet_overlap_with_negative_octant =
        [&](const Vector3& q0, const Vector3& q1, const Vector3& q2, const Vector3& q3) -> bool {
        // Check 1: Check if any tet vertices is in negative octant.
        if ((q0.array() < 0).all()) return true;
        if ((q1.array() < 0).all()) return true;
        if ((q2.array() < 0).all()) return true;
        if ((q3.array() < 0).all()) return true;

        // Check 2: Check if any tet edges cross the negative octant.
        if (edge_overlap_with_negative_octant(q0, q1)) return true;
        if (edge_overlap_with_negative_octant(q0, q2)) return true;
        if (edge_overlap_with_negative_octant(q0, q3)) return true;
        if (edge_overlap_with_negative_octant(q1, q2)) return true;
        if (edge_overlap_with_negative_octant(q1, q3)) return true;
        if (edge_overlap_with_negative_octant(q2, q3)) return true;

        // Check 3: Check if -X, -Y or -Z axis intersect the tet.
        if (triangle_intersects_negative_axes(q0, q1, q2)) return true;
        if (triangle_intersects_negative_axes(q1, q2, q3)) return true;
        if (triangle_intersects_negative_axes(q2, q3, q0)) return true;
        if (triangle_intersects_negative_axes(q3, q0, q1)) return true;

        // All check failed iff tet does not intersect the negative octant.
        return false;
    };

    // AttributeArray attr;
    lagrange::span<uint8_t> attr_ref;
    if (!options.greedy) {
        AttributeId id = internal::find_or_create_attribute<uint8_t>(
            mesh,
            options.output_attribute_name,
            Facet,
            AttributeUsage::Scalar,
            /* number of channels */ 1,
            /* this should already trigger writing the memory and therefore copy on write */
            internal::ResetToDefault::Yes);
        auto& attr =
            mesh.template ref_attribute<uint8_t>(id); // this may also trigger copy on write
        la_debug_assert(static_cast<Index>(attr.get_num_elements()) == num_facets);
        attr_ref = attr.ref_all();
    }

    std::atomic_bool r{false};

    const auto& vertex_positions = vertex_view(mesh);
    tbb::parallel_for(
        tbb::blocked_range<Index>(0, num_facets),
        [&](const tbb::blocked_range<Index>& tbb_range) {
            for (auto fi = tbb_range.begin(); fi != tbb_range.end(); fi++) {
                if (tbb_utils::is_cancelled()) break;

                const auto facet_vertices = mesh.get_facet_vertices(fi);

                // Triangle (v0, v1, v2) intersect the cone defined by 4 planes
                // iff the tetrahedron (q0, q1, q2, q3) does not intersect the negative octant.
                // This can be proved by Farkas' lemma.

                auto& v0 = temp_vars.local().v0;
                auto& v1 = temp_vars.local().v1;
                auto& v2 = temp_vars.local().v2;

                auto& q0 = temp_vars.local().q0;
                auto& q1 = temp_vars.local().q1;
                auto& q2 = temp_vars.local().q2;
                auto& q3 = temp_vars.local().q3;

                v0 = vertex_positions.row(facet_vertices[0]).template head<3>();
                v1 = vertex_positions.row(facet_vertices[1]).template head<3>();
                v2 = vertex_positions.row(facet_vertices[2]).template head<3>();

                q0 = {(v0 - p0).dot(n0), (v1 - p0).dot(n0), (v2 - p0).dot(n0)};
                q1 = {(v0 - p1).dot(n1), (v1 - p1).dot(n1), (v2 - p1).dot(n1)};
                q2 = {(v0 - p2).dot(n2), (v1 - p2).dot(n2), (v2 - p2).dot(n2)};
                q3 = {(v0 - p3).dot(n3), (v1 - p3).dot(n3), (v2 - p3).dot(n3)};

                const auto ri = !tet_overlap_with_negative_octant(q0, q1, q2, q3);

                if (ri) r = true;

                if (!attr_ref.empty() /* same as !options.greedy */) {
                    attr_ref[fi] = uint8_t(ri);
                }

                if (options.greedy && ri) {
                    tbb_utils::cancel_group_execution();
                    break;
                }
            }
        });

    return r.load();
}

#define LA_X_select_facets_in_frustum(_, Scalar, Index) \
    template LA_CORE_API bool select_facets_in_frustum( \
        SurfaceMesh<Scalar, Index>& mesh,               \
        const Frustum<Scalar>& frustum,                 \
        const FrustumSelectionOptions& options);
LA_SURFACE_MESH_X(select_facets_in_frustum, 0)

} // namespace lagrange
