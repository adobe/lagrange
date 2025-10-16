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

#include <array>
#include <atomic>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <tbb/blocked_range.h>
#include <tbb/enumerable_thread_specific.h>
#include <tbb/parallel_for.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <lagrange/Mesh.h>
#include <lagrange/MeshTrait.h>
#include <lagrange/common.h>
#include <lagrange/legacy/inline.h>
#include <lagrange/utils/tbb.h>

namespace lagrange {
LAGRANGE_LEGACY_INLINE
namespace legacy {

/**
 * Select all facets that intersect the cone/frustrum bounded by 4 planes
 * defined by (n_i, p_i), where n_i is the plane normal and p_i is a point on
 * the plane.
 *
 * When `greedy` is true, return as soon as the first facet is selected.
 *
 * When `greedy` is false, check all facets and store the result in a facet
 * attribute named `is_selected`.
 *
 * @param ni normal vector of plane i.
 * @param pi a point on plane i.
 * @param greedy whether to stop as soon as the first facet is selected.
 * @return whether any facet is selected.
 */
template <typename MeshType, typename Point3D>
bool select_facets_in_frustum(
    MeshType& mesh,
    const Eigen::PlainObjectBase<Point3D>& n0,
    const Eigen::PlainObjectBase<Point3D>& p0,
    const Eigen::PlainObjectBase<Point3D>& n1,
    const Eigen::PlainObjectBase<Point3D>& p1,
    const Eigen::PlainObjectBase<Point3D>& n2,
    const Eigen::PlainObjectBase<Point3D>& p2,
    const Eigen::PlainObjectBase<Point3D>& n3,
    const Eigen::PlainObjectBase<Point3D>& p3,
    bool greedy = false)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input is not a mesh.");
    using AttributeArray = typename MeshType::AttributeArray;
    using Scalar = typename MeshType::Scalar;
    using Index = typename MeshType::Index;

    const auto num_facets = mesh.get_num_facets();
    const auto& vertices = mesh.get_vertices();
    const auto& facets = mesh.get_facets();

    // Per-thread buffers to store intermediate results.
    struct LocalBuffers
    {
        Point3D v0, v1, v2; // Triangle vertices.
        Point3D q0, q1, q2, q3; // Intermediate tet vertices.
        std::array<double, 2> r0, r1, r2; // Triangle projections.
    };
    tbb::enumerable_thread_specific<LocalBuffers>
        temp_vars; // we can safely remove this and just declare these variables in the lambdas (as
                   // long as there's no memory allocation on the heap, this is not necessary)


    auto edge_overlap_with_negative_octant = [](const Point3D& q0, const Point3D& q1) -> bool {
        Scalar t_min = 0, t_max = 1;
        const Point3D e = q0 - q1;
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
        [](const Point3D& q0, const Point3D& q1, const Point3D& q2) -> std::pair<Point3D, Scalar> {
        const Point3D n = {
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

    auto triangle_intersects_negative_axis = [&](const Point3D& q0,
                                                 const Point3D& q1,
                                                 const Point3D& q2,
                                                 const Point3D& n,
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
        [&](const Point3D& q0, const Point3D& q1, const Point3D& q2) -> bool {
        const auto r = compute_plane(q0, q1, q2);
        if (triangle_intersects_negative_axis(q0, q1, q2, r.first, r.second, 0)) return true;
        if (triangle_intersects_negative_axis(q0, q1, q2, r.first, r.second, 1)) return true;
        if (triangle_intersects_negative_axis(q0, q1, q2, r.first, r.second, 2)) return true;
        return false;
    };

    auto tet_overlap_with_negative_octant =
        [&](const Point3D& q0, const Point3D& q1, const Point3D& q2, const Point3D& q3) -> bool {
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

    AttributeArray attr;
    if (!greedy) {
        attr.resize(num_facets, 1);
        attr.setZero();
    }
    std::atomic_bool r{false};

    tbb::parallel_for(
        tbb::blocked_range<Index>(0, num_facets),
        [&](const tbb::blocked_range<Index>& tbb_range) {
            for (auto fi = tbb_range.begin(); fi != tbb_range.end(); fi++) {
                if (tbb_utils::is_cancelled()) break;

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

                v0 = vertices.row(facets(fi, 0));
                v1 = vertices.row(facets(fi, 1));
                v2 = vertices.row(facets(fi, 2));

                q0 = {(v0 - p0).dot(n0), (v1 - p0).dot(n0), (v2 - p0).dot(n0)};
                q1 = {(v0 - p1).dot(n1), (v1 - p1).dot(n1), (v2 - p1).dot(n1)};
                q2 = {(v0 - p2).dot(n2), (v1 - p2).dot(n2), (v2 - p2).dot(n2)};
                q3 = {(v0 - p3).dot(n3), (v1 - p3).dot(n3), (v2 - p3).dot(n3)};

                const auto ri = !tet_overlap_with_negative_octant(q0, q1, q2, q3);

                if (ri) r = true;

                if (!greedy) {
                    attr(fi, 0) = ri;
                }

                if (greedy && ri) {
                    tbb_utils::cancel_group_execution();
                    break;
                }
            }
        });

    if (!greedy) {
        mesh.add_facet_attribute("is_selected");
        mesh.import_facet_attribute("is_selected", attr);
    }
    return r.load();
}

} // namespace legacy
} // namespace lagrange
