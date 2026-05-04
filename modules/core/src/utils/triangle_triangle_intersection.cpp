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
#include <lagrange/utils/triangle_triangle_intersection.h>

#include <lagrange/ExactPredicatesShewchuk.h>
#include <lagrange/utils/assert.h>

#include <algorithm>
#include <cmath>

namespace lagrange {

namespace internal {

namespace {

/// Check if the 2D triangle (a,b,c) is degenerate
inline bool is_degenerate_2d(
    const double a[2],
    const double b[2],
    const double c[2],
    const ExactPredicatesShewchuk& pred)
{
    return pred.orient2D(a, b, c) == 0;
}

/// Check if the 3D triangle (a,b,c) is degenerate
bool is_degenerate_3d(
    const double a[3],
    const double b[3],
    const double c[3],
    const ExactPredicatesShewchuk& pred)
{
    double a_xy[2] = {a[0], a[1]}, b_xy[2] = {b[0], b[1]}, c_xy[2] = {c[0], c[1]};
    double a_yz[2] = {a[1], a[2]}, b_yz[2] = {b[1], b[2]}, c_yz[2] = {c[1], c[2]};
    double a_zx[2] = {a[2], a[0]}, b_zx[2] = {b[2], b[0]}, c_zx[2] = {c[2], c[0]};

    auto o_xy = pred.orient2D(a_xy, b_xy, c_xy);
    auto o_yz = pred.orient2D(a_yz, b_yz, c_yz);
    auto o_zx = pred.orient2D(a_zx, b_zx, c_zx);

    return o_xy == 0 && o_yz == 0 && o_zx == 0;
}

/// Compute the dominant axis for projection in the coplanar case, based on the bounding box of the
/// two triangles.
///
/// @note The bbox diagonal computation is not exact, but numerical error here has minimal impact on
/// the correctness of the algorithm. When two axis have very close bbox diagonals, the choice of
/// projection axis using either one should be fine.
int coplanar_projection_axis(const double t1[3][3], const double t2[3][3])
{
    double bbox_min[3] = {t1[0][0], t1[0][1], t1[0][2]};
    double bbox_max[3] = {t1[0][0], t1[0][1], t1[0][2]};
    for (int i = 0; i < 3; ++i) {
        for (int k = 0; k < 3; ++k) {
            bbox_min[k] = std::min(bbox_min[k], t1[i][k]);
            bbox_max[k] = std::max(bbox_max[k], t1[i][k]);
            bbox_min[k] = std::min(bbox_min[k], t2[i][k]);
            bbox_max[k] = std::max(bbox_max[k], t2[i][k]);
        }
    }
    double bbox_diag[3] = {
        bbox_max[0] - bbox_min[0],
        bbox_max[1] - bbox_min[1],
        bbox_max[2] - bbox_min[2]};
    int seg_axis = 0;
    if (bbox_diag[1] < bbox_diag[seg_axis]) seg_axis = 1;
    if (bbox_diag[2] < bbox_diag[seg_axis]) seg_axis = 2;
    return seg_axis;
}

///
/// Check if two 2D segments [a0,a1] and [b0,b1] intersect.
/// Orientation decisions use exact predicates; the collinear-overlap check uses
/// standard floating-point interval arithmetic.
///
/// @param include_boundary  If true, touching at endpoints/boundaries counts as intersection.
///                          If false, only proper interior crossings count.
///
bool segments_intersect_2d(
    const double a0[2],
    const double a1[2],
    const double b0[2],
    const double b1[2],
    const ExactPredicatesShewchuk& pred,
    bool include_boundary)
{
    // Check if segments intersect using orientation tests
    short o1 = pred.orient2D(a0, a1, b0);
    short o2 = pred.orient2D(a0, a1, b1);
    short o3 = pred.orient2D(b0, b1, a0);
    short o4 = pred.orient2D(b0, b1, a1);

    // Case 1: Proper crossing - segments cross in their interiors
    // Both endpoints of each segment are on opposite sides of the other segment
    if (o1 * o2 < 0 && o3 * o4 < 0) {
        return true;
    }

    // Case 2: Collinear segments - need to check if they actually overlap
    if (o1 == 0 && o2 == 0 && o3 == 0 && o4 == 0) {
        // All four orientations are zero - segments are collinear
        // Need to check if they overlap on the line

        // Pick the axis with the largest bounding-box extent over all 4 points
        double extent0 =
            std::max({a0[0], a1[0], b0[0], b1[0]}) - std::min({a0[0], a1[0], b0[0], b1[0]});
        double extent1 =
            std::max({a0[1], a1[1], b0[1], b1[1]}) - std::min({a0[1], a1[1], b0[1], b1[1]});
        int axis = (extent1 > extent0) ? 1 : 0;

        // Get intervals for both segments on the dominant axis
        double a_min = std::min(a0[axis], a1[axis]);
        double a_max = std::max(a0[axis], a1[axis]);
        double b_min = std::min(b0[axis], b1[axis]);
        double b_max = std::max(b0[axis], b1[axis]);

        // Check for overlap
        if (include_boundary) {
            // Touching at endpoints counts as intersection
            return !(a_max < b_min || b_max < a_min);
        } else {
            // Only interior overlap counts
            return std::max(a_min, b_min) < std::min(a_max, b_max);
        }
    }

    // Case 3: One or more points on the supporting line of the other segment
    // Need to check if the point actually lies on the segment (not just the line)

    // Helper: check if point p lies on segment [a, b] (when already known to be collinear)
    auto point_on_segment = [](const double p[2], const double a[2], const double b[2]) -> bool {
        // For collinear points, p is on segment [a,b] if it's between a and b
        // Use coordinate-wise check
        bool x_between = (p[0] >= std::min(a[0], b[0]) && p[0] <= std::max(a[0], b[0]));
        bool y_between = (p[1] >= std::min(a[1], b[1]) && p[1] <= std::max(a[1], b[1]));
        return x_between && y_between;
    };

    // Helper: check if point p is strictly inside segment [a, b] (not at endpoints)
    auto point_strictly_inside_segment =
        [](const double p[2], const double a[2], const double b[2]) -> bool {
        // Point must not equal either endpoint
        bool not_a = (p[0] != a[0] || p[1] != a[1]);
        bool not_b = (p[0] != b[0] || p[1] != b[1]);
        if (!not_a || !not_b) return false;

        // Point must be strictly between endpoints on both axes
        bool x_strictly_between = (p[0] > std::min(a[0], b[0]) && p[0] < std::max(a[0], b[0]));
        bool y_strictly_between = (p[1] > std::min(a[1], b[1]) && p[1] < std::max(a[1], b[1]));

        // Handle degenerate cases (segment is a point or parallel to axis)
        bool x_degenerate = (a[0] == b[0]);
        bool y_degenerate = (a[1] == b[1]);

        if (x_degenerate && y_degenerate) return false; // Segment is a point
        if (x_degenerate) return y_strictly_between && (p[0] == a[0]);
        if (y_degenerate) return x_strictly_between && (p[1] == a[1]);

        return x_strictly_between && y_strictly_between;
    };

    // Check which endpoints are on which segments
    bool b0_on_a = (o1 == 0) && point_on_segment(b0, a0, a1);
    bool b1_on_a = (o2 == 0) && point_on_segment(b1, a0, a1);
    bool a0_on_b = (o3 == 0) && point_on_segment(a0, b0, b1);
    bool a1_on_b = (o4 == 0) && point_on_segment(a1, b0, b1);

    // Check for nested segments
    // Segment B is nested in A if both endpoints of B are on segment A
    bool b_nested_in_a = b0_on_a && b1_on_a;
    // Segment A is nested in B if both endpoints of A are on segment B
    bool a_nested_in_b = a0_on_b && a1_on_b;

    if (b_nested_in_a || a_nested_in_b) {
        // One segment is completely nested in the other
        // This counts as intersection even in strict mode
        return true;
    }

    // Check for any actual contact
    bool has_contact = b0_on_a || b1_on_a || a0_on_b || a1_on_b;

    if (!has_contact) {
        // No actual contact - points are on supporting lines but outside segments
        return false;
    }

    // Case 4: There is actual contact (point on segment) but not nested
    if (!include_boundary) {
        // Strict mode: only count if one endpoint is strictly inside the other segment
        bool b0_strictly_inside_a = (o1 == 0) && point_strictly_inside_segment(b0, a0, a1);
        bool b1_strictly_inside_a = (o2 == 0) && point_strictly_inside_segment(b1, a0, a1);
        bool a0_strictly_inside_b = (o3 == 0) && point_strictly_inside_segment(a0, b0, b1);
        bool a1_strictly_inside_b = (o4 == 0) && point_strictly_inside_segment(a1, b0, b1);

        return b0_strictly_inside_a || b1_strictly_inside_a || a0_strictly_inside_b ||
               a1_strictly_inside_b;
    }

    // include_boundary = true: contact counts as intersection
    return true;
}

///
/// Check if a segment [a, b] intersects a triangle [t0, t1, t2] when all points are
/// known to be coplanar. Projects to 2D using the triangle's normal for the orientation tests.
///
bool segment_intersects_triangle_coplanar(
    const double* a,
    const double* b,
    const double* t0,
    const double* t1,
    const double* t2,
    const ExactPredicatesShewchuk& pred,
    bool include_boundary)
{
    // Pick projection axis from the bbox of all 5 input points (segment endpoints +
    // triangle vertices). Using the triangle's normal here would fail when the triangle is
    // degenerate (zero normal) or near-degenerate along an axis that actually separates the
    // segment and triangle in 3D — the smallest-extent axis would be discarded by the
    // projection, causing false positives.
    double t_seg[3][3] = {{a[0], a[1], a[2]}, {b[0], b[1], b[2]}, {b[0], b[1], b[2]}};
    double t_tri[3][3] = {{t0[0], t0[1], t0[2]}, {t1[0], t1[1], t1[2]}, {t2[0], t2[1], t2[2]}};
    int axis = coplanar_projection_axis(t_seg, t_tri);
    int i0 = (axis + 1) % 3;
    int i1 = (axis + 2) % 3;

    // When T is degenerate (collinear), the caller's coplanarity precondition (d_a==d_b==0
    // from orient3D) is vacuous: orient3D against a collinear triangle returns 0 for any
    // 4th point. Verify true 3D coplanarity explicitly; if (a,b) is skew to T's line, no
    // intersection. If coplanar, reduce to 2D segment-segment against each T edge — the
    // point_location/edge-crossing logic below would otherwise misreport a point on T's
    // supporting line (but outside T's span) as a boundary hit.
    if (is_degenerate_3d(t0, t1, t2, pred)) {
        if (pred.orient3D(a, b, t0, t1) != 0 || pred.orient3D(a, b, t1, t2) != 0 ||
            pred.orient3D(a, b, t0, t2) != 0) {
            return false;
        }
        double a2_d[2] = {a[i0], a[i1]};
        double b2_d[2] = {b[i0], b[i1]};
        double t_pts[3][2] = {{t0[i0], t0[i1]}, {t1[i0], t1[i1]}, {t2[i0], t2[i1]}};
        for (int i = 0; i < 3; ++i) {
            if (segments_intersect_2d(
                    a2_d,
                    b2_d,
                    t_pts[i],
                    t_pts[(i + 1) % 3],
                    pred,
                    include_boundary)) {
                return true;
            }
        }
        return false;
    }

    double a2[2] = {a[i0], a[i1]};
    double b2[2] = {b[i0], b[i1]};
    double t0_2[2] = {t0[i0], t0[i1]};
    double t1_2[2] = {t1[i0], t1[i1]};
    double t2_2[2] = {t2[i0], t2[i1]};

    // Classify a 2D point relative to the triangle.
    // Returns 1 = strictly interior, 0 = on boundary, -1 = outside.
    auto point_location = [&](double p[2]) -> int {
        short o1 = pred.orient2D(t0_2, t1_2, p);
        short o2 = pred.orient2D(t1_2, t2_2, p);
        short o3 = pred.orient2D(t2_2, t0_2, p);
        bool inside = (o1 >= 0 && o2 >= 0 && o3 >= 0) || (o1 <= 0 && o2 <= 0 && o3 <= 0);
        if (!inside) return -1;
        return (o1 != 0 && o2 != 0 && o3 != 0) ? 1 : 0;
    };

    // Check if either endpoint is inside (or on the boundary of) the triangle
    int a_loc = point_location(a2);
    int b_loc = point_location(b2);
    if (include_boundary) {
        if (a_loc >= 0 || b_loc >= 0) return true;
    } else {
        if (a_loc == 1 || b_loc == 1) return true;
    }

    // Check if the segment crosses any edge of the triangle.
    // In strict mode, skip collinear pairs so that a shared edge (or partial overlap
    // along a triangle edge) does not count as an intersection.
    double* tri_verts[3] = {t0_2, t1_2, t2_2};
    for (int i = 0; i < 3; ++i) {
        double* ea = tri_verts[i];
        double* eb = tri_verts[(i + 1) % 3];

        if (!include_boundary) {
            short co1 = pred.orient2D(a2, b2, ea);
            short co2 = pred.orient2D(a2, b2, eb);
            short co3 = pred.orient2D(ea, eb, a2);
            short co4 = pred.orient2D(ea, eb, b2);
            if (co1 == 0 && co2 == 0 && co3 == 0 && co4 == 0) continue;
        }

        if (segments_intersect_2d(a2, b2, ea, eb, pred, include_boundary)) {
            return true;
        }
    }

    return false;
}

///
/// Check if triangles intersect when coplanar using 2D projection.
///
bool coplanar_triangles_intersect(
    double t1[3][3],
    double t2[3][3],
    const ExactPredicatesShewchuk& pred,
    bool include_boundary)
{
    // When t1 is degenerate (collinear/coincident vertices, zero-area), handle it early.
    // A degenerate t1 has no interior, so strict mode never intersects.
    // In boundary mode, reduce to: segment-vs-triangle (if t2 is non-degenerate) or
    // segment-vs-segment (if both are degenerate). Both are done with the existing
    // segment_intersects_triangle_coplanar / segments_intersect_2d helpers.
    if (is_degenerate_3d(t1[0], t1[1], t1[2], pred)) {
        if (!include_boundary) return false;

        if (!is_degenerate_3d(t2[0], t2[1], t2[2], pred)) {
            // t2 is non-degenerate: check each edge of degenerate t1 against t2
            for (int i = 0; i < 3; ++i) {
                if (segment_intersects_triangle_coplanar(
                        t1[i],
                        t1[(i + 1) % 3],
                        t2[0],
                        t2[1],
                        t2[2],
                        pred,
                        include_boundary))
                    return true;
            }
            return false;
        }

        // Both degenerate: segment-segment check.
        int seg_axis = coplanar_projection_axis(t1, t2);
        int si0 = (seg_axis + 1) % 3, si1 = (seg_axis + 2) % 3;
        for (int i = 0; i < 3; ++i) {
            double a0[2] = {t1[i][si0], t1[i][si1]};
            double a1[2] = {t1[(i + 1) % 3][si0], t1[(i + 1) % 3][si1]};
            for (int j = 0; j < 3; ++j) {
                double b0[2] = {t2[j][si0], t2[j][si1]};
                double b1[2] = {t2[(j + 1) % 3][si0], t2[(j + 1) % 3][si1]};
                if (segments_intersect_2d(a0, a1, b0, b1, pred, include_boundary)) return true;
            }
        }
        return false;
    }

    int axis = coplanar_projection_axis(t1, t2);
    int i0 = (axis + 1) % 3;
    int i1 = (axis + 2) % 3;

    // Project triangles to 2D
    double t1_2d[3][2], t2_2d[3][2];
    for (int i = 0; i < 3; ++i) {
        t1_2d[i][0] = t1[i][i0];
        t1_2d[i][1] = t1[i][i1];
        t2_2d[i][0] = t2[i][i0];
        t2_2d[i][1] = t2[i][i1];
    }

    // Check if any vertex of t2 is inside t1
    for (int i = 0; i < 3; ++i) {
        short o1 = pred.orient2D(t1_2d[0], t1_2d[1], t2_2d[i]);
        short o2 = pred.orient2D(t1_2d[1], t1_2d[2], t2_2d[i]);
        short o3 = pred.orient2D(t1_2d[2], t1_2d[0], t2_2d[i]);

        if (include_boundary) {
            // Boundary mode: point inside or on boundary
            if ((o1 >= 0 && o2 >= 0 && o3 >= 0) || (o1 <= 0 && o2 <= 0 && o3 <= 0)) {
                return true;
            }
        } else {
            // Strict mode: point must be in interior (not on boundary)
            if ((o1 > 0 && o2 > 0 && o3 > 0) || (o1 < 0 && o2 < 0 && o3 < 0)) {
                return true;
            }
        }
    }

    // Check if any vertex of t1 is inside t2
    for (int i = 0; i < 3; ++i) {
        short o1 = pred.orient2D(t2_2d[0], t2_2d[1], t1_2d[i]);
        short o2 = pred.orient2D(t2_2d[1], t2_2d[2], t1_2d[i]);
        short o3 = pred.orient2D(t2_2d[2], t2_2d[0], t1_2d[i]);

        if (include_boundary) {
            // Boundary mode: point inside or on boundary
            if ((o1 >= 0 && o2 >= 0 && o3 >= 0) || (o1 <= 0 && o2 <= 0 && o3 <= 0)) {
                return true;
            }
        } else {
            // Strict mode: point must be in interior (not on boundary)
            if ((o1 > 0 && o2 > 0 && o3 > 0) || (o1 < 0 && o2 < 0 && o3 < 0)) {
                return true;
            }
        }
    }

    // Check if any edges intersect
    // Skip collinear edge pairs (e.g., when triangles share an edge)
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            // Get edge endpoints
            const double* e1_a = t1_2d[i];
            const double* e1_b = t1_2d[(i + 1) % 3];
            const double* e2_a = t2_2d[j];
            const double* e2_b = t2_2d[(j + 1) % 3];

            if (!include_boundary) {
                // Check if edges are collinear (all 4 cross-orientation tests return 0)
                short o1 = pred.orient2D(e1_a, e1_b, e2_a);
                short o2 = pred.orient2D(e1_a, e1_b, e2_b);
                short o3 = pred.orient2D(e2_a, e2_b, e1_a);
                short o4 = pred.orient2D(e2_a, e2_b, e1_b);

                bool edges_collinear = (o1 == 0 && o2 == 0 && o3 == 0 && o4 == 0);

                // Skip collinear edge pairs ONLY when include_boundary=false
                if (edges_collinear) {
                    continue;
                }
            }

            // Test edge pair
            if (segments_intersect_2d(e1_a, e1_b, e2_a, e2_b, pred, include_boundary)) {
                return true;
            }
        }
    }

    // Nesting / duplication check (strict mode only):
    // The vertex-in-triangle tests above only detect strict interior containment, and the
    // collinear-edge skip can prevent crossing detection for identical or nested triangles.
    // If a non-degenerate triangle's vertices are all non-outside the other triangle
    // (on boundary or inside), their interiors overlap — e.g. duplicate triangles or one
    // triangle fully enclosing the other with its vertices landing on the boundary.
    // The non-degeneracy guard prevents false positives when a degenerate (zero-area)
    // triangle's coincident vertices happen to fall inside the other triangle.
    if (!include_boundary) {
        auto is_nondegenerate = [&](const double tri[3][2]) {
            return !is_degenerate_2d(tri[0], tri[1], tri[2], pred);
        };
        auto all_inside_or_on = [&](const double verts[3][2], const double tri[3][2]) {
            for (int i = 0; i < 3; ++i) {
                short o1 = pred.orient2D(tri[0], tri[1], verts[i]);
                short o2 = pred.orient2D(tri[1], tri[2], verts[i]);
                short o3 = pred.orient2D(tri[2], tri[0], verts[i]);
                bool non_outside =
                    (o1 >= 0 && o2 >= 0 && o3 >= 0) || (o1 <= 0 && o2 <= 0 && o3 <= 0);
                if (!non_outside) return false;
            }
            return true;
        };
        if ((is_nondegenerate(t2_2d) && all_inside_or_on(t2_2d, t1_2d)) ||
            (is_nondegenerate(t1_2d) && all_inside_or_on(t1_2d, t2_2d)))
            return true;
    }

    return false;
}

} // anonymous namespace

bool triangle_triangle_intersection_3d(
    double t1_v0[3],
    double t1_v1[3],
    double t1_v2[3],
    double t2_v0[3],
    double t2_v1[3],
    double t2_v2[3],
    bool include_boundary)
{
    ExactPredicatesShewchuk pred;

    // Compute signed distances of triangle 2 vertices from the plane of triangle 1
    short d2_v0 = pred.orient3D(t1_v0, t1_v1, t1_v2, t2_v0);
    short d2_v1 = pred.orient3D(t1_v0, t1_v1, t1_v2, t2_v1);
    short d2_v2 = pred.orient3D(t1_v0, t1_v1, t1_v2, t2_v2);

    // Quick exit: t2 entirely on one side of t1's plane
    if (d2_v0 == d2_v1 && d2_v1 == d2_v2 && d2_v0 != 0) {
        return false;
    }

    // Compute signed distances of triangle 1 vertices from the plane of triangle 2
    short d1_v0 = pred.orient3D(t2_v0, t2_v1, t2_v2, t1_v0);
    short d1_v1 = pred.orient3D(t2_v0, t2_v1, t2_v2, t1_v1);
    short d1_v2 = pred.orient3D(t2_v0, t2_v1, t2_v2, t1_v2);

    // Quick exit: t1 entirely on one side of t2's plane
    if (d1_v0 == d1_v1 && d1_v1 == d1_v2 && d1_v0 != 0) {
        return false;
    }

    // Handle all-zero d2_v*: either truly coplanar or t1 is degenerate (collinear/coincident).
    // When t1 is degenerate, orient3D always returns 0 for any 4th point, so d2_v* == 0
    // even when t2 is not in t1's plane. Verify true coplanarity by requiring d1_v* also
    // all zero (non-degenerate t1 and t2 sharing a plane implies t1 in t2's plane too).
    if ((d2_v0 == 0 && d2_v1 == 0 && d2_v2 == 0) && (d1_v0 == 0 && d1_v1 == 0 && d1_v2 == 0)) {
        // Both d* sets are all-zero. This means either:
        //   (a) the triangles are truly coplanar, or
        //   (b) both are degenerate — orient3D returns 0 for any 4th point when the first
        //       three are collinear, so non-coplanar (skew) degenerate pairs reach here too.
        // Disambiguate: if both are degenerate, run explicit coplanarity checks using mixed
        // orient3D calls (one vertex pair from each triangle). Any non-zero result means the
        // segments are skew and share no point — return false for both modes.
        if (is_degenerate_3d(t1_v0, t1_v1, t1_v2, pred) &&
            is_degenerate_3d(t2_v0, t2_v1, t2_v2, pred)) {
            bool coplanar = pred.orient3D(t1_v0, t1_v1, t2_v0, t2_v1) == 0 &&
                            pred.orient3D(t1_v0, t1_v2, t2_v0, t2_v1) == 0 &&
                            pred.orient3D(t1_v0, t1_v1, t2_v0, t2_v2) == 0;
            if (!coplanar) return false;
        }

        // Truly coplanar: check 2D intersection
        double t1[3][3] = {
            {t1_v0[0], t1_v0[1], t1_v0[2]},
            {t1_v1[0], t1_v1[1], t1_v1[2]},
            {t1_v2[0], t1_v2[1], t1_v2[2]}};
        double t2[3][3] = {
            {t2_v0[0], t2_v0[1], t2_v0[2]},
            {t2_v1[0], t2_v1[1], t2_v1[2]},
            {t2_v2[0], t2_v2[1], t2_v2[2]}};
        return coplanar_triangles_intersect(t1, t2, pred, include_boundary);
    } else if (!include_boundary) {
        if (d2_v0 == 0 && d2_v1 == 0 && d2_v2 == 0) {
            // t1 is degenerate (collinear). It has no interior, so no strict intersection.
            return false;
        }
        if (d1_v0 == 0 && d1_v1 == 0 && d1_v2 == 0) {
            // t2 is degenerate (collinear). It has no interior, so no strict intersection.
            return false;
        }
        // When neither triangle is degenerate or boundary=ON:
        // fall through to edge-based checks to detect boundary contact.
    }

    // Algorithm: For each edge of T1, check if it intersects T2
    // And vice versa for T2's edges with T1

    // Helper function: test if edge (a, b) intersects triangle T using orient3D
    // Track boundary contact types across all edge tests to detect the case where
    // the intersection segment has both endpoints on triangle boundaries.
    // has_vertex_contact: some edge passed through a triangle vertex (zeros==2)
    // has_genuine_edge_contact: some straddling edge hit a triangle edge interior
    //   (zeros==1, d_a*d_b<0, o-check confirmed inside)
    bool has_vertex_contact = false;
    bool has_genuine_edge_contact = false;

    auto edge_intersects_triangle =
        [&pred, include_boundary, &has_vertex_contact, &has_genuine_edge_contact](
            const double* a,
            const double* b,
            const double* t0,
            const double* t1,
            const double* t2,
            short d_a,
            short d_b) -> bool {
        // If both endpoints are on the same side of T's plane, no intersection
        if (d_a * d_b > 0) return false;

        // If both endpoints are on the plane, test the segment against the triangle in 2D
        if (d_a == 0 && d_b == 0) {
            return segment_intersects_triangle_coplanar(a, b, t0, t1, t2, pred, include_boundary);
        }

        // If one or both endpoints are on the plane, or they straddle the plane:
        // Test which side of each triangle edge (in 3D) the segment (a,b) lies on.
        // Each orient3D orients the tetrahedron formed by (a,b) and one directed edge of T.
        // Counting zeros classifies the contact point:
        //   zeros == 0  => interior-to-interior crossing (all signs identical)
        //   zeros == 1  => landing on a triangle edge interior (boundary contact)
        //   zeros == 2  => landing on a triangle vertex (boundary contact)
        //   zeros == 3  => impossible: all-coplanar case handled above

        short o1 = pred.orient3D(a, b, t0, t1);
        short o2 = pred.orient3D(a, b, t1, t2);
        short o3 = pred.orient3D(a, b, t2, t0);

        // Count zeros
        int zeros = (o1 == 0) + (o2 == 0) + (o3 == 0);

        if (zeros == 3) {
            // Unreachable: the all-coplanar (d_a==0 && d_b==0) case is handled above.
            la_debug_assert(
                false,
                "Unexpected: all three edge tests are zero but segment not coplanar");
            return false;
        }

        if (zeros == 2) {
            // The segment passes through a vertex of T (boundary contact).
            // Two zeros means the segment is coplanar with two edges of T, forcing it
            // through their shared vertex.
            has_vertex_contact = true;
            return include_boundary;
        }

        if (zeros == 1) {
            // Confirm the contact is within the edge interior, not on its extension.
            bool on_edge;
            if (o1 == 0)
                on_edge = (o2 * o3 > 0);
            else if (o2 == 0)
                on_edge = (o1 * o3 > 0);
            else
                on_edge = (o1 * o2 > 0);

            if (include_boundary) return on_edge;

            // In strict mode: record genuine straddling hits on T's edge interior for the
            // shared-boundary-segment check at the end.
            if (on_edge && d_a * d_b < 0) has_genuine_edge_contact = true;
            return false;
        }

        // No zeros - all three must have same sign for interior intersection
        return (o1 > 0 && o2 > 0 && o3 > 0) || (o1 < 0 && o2 < 0 && o3 < 0);
    };

    // Check each edge of T1 against T2
    if (edge_intersects_triangle(t1_v0, t1_v1, t2_v0, t2_v1, t2_v2, d1_v0, d1_v1)) return true;
    if (edge_intersects_triangle(t1_v1, t1_v2, t2_v0, t2_v1, t2_v2, d1_v1, d1_v2)) return true;
    if (edge_intersects_triangle(t1_v2, t1_v0, t2_v0, t2_v1, t2_v2, d1_v2, d1_v0)) return true;

    // Check each edge of T2 against T1
    if (edge_intersects_triangle(t2_v0, t2_v1, t1_v0, t1_v1, t1_v2, d2_v0, d2_v1)) return true;
    if (edge_intersects_triangle(t2_v1, t2_v2, t1_v0, t1_v1, t1_v2, d2_v1, d2_v2)) return true;
    if (edge_intersects_triangle(t2_v2, t2_v0, t1_v0, t1_v1, t1_v2, d2_v2, d2_v0)) return true;

    // Special case: intersection segment has both endpoints on triangle boundaries.
    // If one endpoint is a shared vertex (vertex contact) and the other is an interior
    // point of an opposite edge (genuine edge contact), the segment is non-degenerate
    // and implies strict interior overlap.
    return has_vertex_contact && has_genuine_edge_contact;
}

} // namespace internal

template <typename Scalar>
bool triangle_triangle_intersection(
    span<const Scalar, 3> t1_v0,
    span<const Scalar, 3> t1_v1,
    span<const Scalar, 3> t1_v2,
    span<const Scalar, 3> t2_v0,
    span<const Scalar, 3> t2_v1,
    span<const Scalar, 3> t2_v2,
    IncludeBoundaryIntersection boundary)
{
    // Convert to double for exact predicates
    double t1_v0_d[3] = {
        static_cast<double>(t1_v0[0]),
        static_cast<double>(t1_v0[1]),
        static_cast<double>(t1_v0[2])};
    double t1_v1_d[3] = {
        static_cast<double>(t1_v1[0]),
        static_cast<double>(t1_v1[1]),
        static_cast<double>(t1_v1[2])};
    double t1_v2_d[3] = {
        static_cast<double>(t1_v2[0]),
        static_cast<double>(t1_v2[1]),
        static_cast<double>(t1_v2[2])};
    double t2_v0_d[3] = {
        static_cast<double>(t2_v0[0]),
        static_cast<double>(t2_v0[1]),
        static_cast<double>(t2_v0[2])};
    double t2_v1_d[3] = {
        static_cast<double>(t2_v1[0]),
        static_cast<double>(t2_v1[1]),
        static_cast<double>(t2_v1[2])};
    double t2_v2_d[3] = {
        static_cast<double>(t2_v2[0]),
        static_cast<double>(t2_v2[1]),
        static_cast<double>(t2_v2[2])};

    return internal::triangle_triangle_intersection_3d(
        t1_v0_d,
        t1_v1_d,
        t1_v2_d,
        t2_v0_d,
        t2_v1_d,
        t2_v2_d,
        boundary == IncludeBoundaryIntersection::Yes);
}

// Explicit template instantiations
template LA_CORE_API bool triangle_triangle_intersection<float>(
    span<const float, 3>,
    span<const float, 3>,
    span<const float, 3>,
    span<const float, 3>,
    span<const float, 3>,
    span<const float, 3>,
    IncludeBoundaryIntersection);

template LA_CORE_API bool triangle_triangle_intersection<double>(
    span<const double, 3>,
    span<const double, 3>,
    span<const double, 3>,
    span<const double, 3>,
    span<const double, 3>,
    span<const double, 3>,
    IncludeBoundaryIntersection);

} // namespace lagrange
