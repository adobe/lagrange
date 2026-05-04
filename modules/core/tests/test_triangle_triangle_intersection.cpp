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
#include <lagrange/testing/common.h>

#include <lagrange/utils/triangle_triangle_intersection.h>

TEST_CASE("triangle_triangle_intersection", "[utils][triangle_intersection]")
{
    using namespace lagrange;

    SECTION("Non-intersecting triangles - separated")
    {
        // Triangle 1 in XY plane at z=0
        double t1_v0[3] = {0.0, 0.0, 0.0};
        double t1_v1[3] = {1.0, 0.0, 0.0};
        double t1_v2[3] = {0.0, 1.0, 0.0};

        // Triangle 2 in XY plane at z=1 (parallel, separated)
        double t2_v0[3] = {0.0, 0.0, 1.0};
        double t2_v1[3] = {1.0, 0.0, 1.0};
        double t2_v2[3] = {0.0, 1.0, 1.0};

        span<const double, 3> s1_v0(t1_v0, 3);
        span<const double, 3> s1_v1(t1_v1, 3);
        span<const double, 3> s1_v2(t1_v2, 3);
        span<const double, 3> s2_v0(t2_v0, 3);
        span<const double, 3> s2_v1(t2_v1, 3);
        span<const double, 3> s2_v2(t2_v2, 3);

        REQUIRE_FALSE(triangle_triangle_intersection(s1_v0, s1_v1, s1_v2, s2_v0, s2_v1, s2_v2));
    }

    SECTION("Non-intersecting triangles - same plane, no overlap")
    {
        // Triangle 1
        double t1_v0[3] = {0.0, 0.0, 0.0};
        double t1_v1[3] = {1.0, 0.0, 0.0};
        double t1_v2[3] = {0.0, 1.0, 0.0};

        // Triangle 2 in same plane but far away
        double t2_v0[3] = {5.0, 5.0, 0.0};
        double t2_v1[3] = {6.0, 5.0, 0.0};
        double t2_v2[3] = {5.0, 6.0, 0.0};

        span<const double, 3> s1_v0(t1_v0, 3);
        span<const double, 3> s1_v1(t1_v1, 3);
        span<const double, 3> s1_v2(t1_v2, 3);
        span<const double, 3> s2_v0(t2_v0, 3);
        span<const double, 3> s2_v1(t2_v1, 3);
        span<const double, 3> s2_v2(t2_v2, 3);

        REQUIRE_FALSE(triangle_triangle_intersection(s1_v0, s1_v1, s1_v2, s2_v0, s2_v1, s2_v2));
    }

    SECTION("Intersecting triangles - crossing")
    {
        // Triangle 1 in XY plane
        double t1_v0[3] = {-1.0, 0.0, 0.0};
        double t1_v1[3] = {1.0, 0.0, 0.0};
        double t1_v2[3] = {0.0, 1.0, 0.0};

        // Triangle 2 crossing through triangle 1
        double t2_v0[3] = {0.0, 0.5, -1.0};
        double t2_v1[3] = {0.0, 0.5, 1.0};
        double t2_v2[3] = {0.0, -0.5, 0.0};

        span<const double, 3> s1_v0(t1_v0, 3);
        span<const double, 3> s1_v1(t1_v1, 3);
        span<const double, 3> s1_v2(t1_v2, 3);
        span<const double, 3> s2_v0(t2_v0, 3);
        span<const double, 3> s2_v1(t2_v1, 3);
        span<const double, 3> s2_v2(t2_v2, 3);

        REQUIRE(triangle_triangle_intersection(s1_v0, s1_v1, s1_v2, s2_v0, s2_v1, s2_v2));
    }

    SECTION("Intersecting triangles - coplanar overlap")
    {
        // Triangle 1
        double t1_v0[3] = {0.0, 0.0, 0.0};
        double t1_v1[3] = {2.0, 0.0, 0.0};
        double t1_v2[3] = {0.0, 2.0, 0.0};

        // Triangle 2 overlapping in same plane
        double t2_v0[3] = {0.5, 0.5, 0.0};
        double t2_v1[3] = {1.5, 0.5, 0.0};
        double t2_v2[3] = {0.5, 1.5, 0.0};

        span<const double, 3> s1_v0(t1_v0, 3);
        span<const double, 3> s1_v1(t1_v1, 3);
        span<const double, 3> s1_v2(t1_v2, 3);
        span<const double, 3> s2_v0(t2_v0, 3);
        span<const double, 3> s2_v1(t2_v1, 3);
        span<const double, 3> s2_v2(t2_v2, 3);

        REQUIRE(triangle_triangle_intersection(s1_v0, s1_v1, s1_v2, s2_v0, s2_v1, s2_v2));
    }

    SECTION("Touching triangles - shared vertex")
    {
        // Triangle 1
        double t1_v0[3] = {0.0, 0.0, 0.0};
        double t1_v1[3] = {1.0, 0.0, 0.0};
        double t1_v2[3] = {0.0, 1.0, 0.0};

        // Triangle 2 sharing a vertex but on different planes
        double t2_v0[3] = {0.0, 0.0, 0.0}; // Shared vertex
        double t2_v1[3] = {1.0, 0.0, 1.0};
        double t2_v2[3] = {0.0, 1.0, 1.0};

        span<const double, 3> s1_v0(t1_v0, 3);
        span<const double, 3> s1_v1(t1_v1, 3);
        span<const double, 3> s1_v2(t1_v2, 3);
        span<const double, 3> s2_v0(t2_v0, 3);
        span<const double, 3> s2_v1(t2_v1, 3);
        span<const double, 3> s2_v2(t2_v2, 3);

        // Touching at a single shared vertex is a boundary-only contact.
        REQUIRE_FALSE(triangle_triangle_intersection(s1_v0, s1_v1, s1_v2, s2_v0, s2_v1, s2_v2));
        REQUIRE(triangle_triangle_intersection(
            s1_v0,
            s1_v1,
            s1_v2,
            s2_v0,
            s2_v1,
            s2_v2,
            IncludeBoundaryIntersection::Yes));
    }

    SECTION("Span interface - float")
    {
        using Scalar = float;

        Scalar t1_v0_data[3] = {0.0f, 0.0f, 0.0f};
        Scalar t1_v1_data[3] = {1.0f, 0.0f, 0.0f};
        Scalar t1_v2_data[3] = {0.0f, 1.0f, 0.0f};

        Scalar t2_v0_data[3] = {0.0f, 0.5f, -1.0f};
        Scalar t2_v1_data[3] = {0.0f, 0.5f, 1.0f};
        Scalar t2_v2_data[3] = {0.0f, -0.5f, 0.0f};

        span<const Scalar, 3> t1_v0(t1_v0_data, 3);
        span<const Scalar, 3> t1_v1(t1_v1_data, 3);
        span<const Scalar, 3> t1_v2(t1_v2_data, 3);
        span<const Scalar, 3> t2_v0(t2_v0_data, 3);
        span<const Scalar, 3> t2_v1(t2_v1_data, 3);
        span<const Scalar, 3> t2_v2(t2_v2_data, 3);

        REQUIRE(triangle_triangle_intersection(t1_v0, t1_v1, t1_v2, t2_v0, t2_v1, t2_v2));
    }

    SECTION("Edge case - degenerate triangle")
    {
        // Triangle 1 - normal triangle
        double t1_v0[3] = {0.0, 0.0, 0.0};
        double t1_v1[3] = {1.0, 0.0, 0.0};
        double t1_v2[3] = {0.0, 1.0, 0.0};

        // Triangle 2 - degenerate (all vertices collinear)
        double t2_v0[3] = {0.5, 0.5, 0.0};
        double t2_v1[3] = {0.6, 0.6, 0.0};
        double t2_v2[3] = {0.7, 0.7, 0.0};

        span<const double, 3> s1_v0(t1_v0, 3);
        span<const double, 3> s1_v1(t1_v1, 3);
        span<const double, 3> s1_v2(t1_v2, 3);
        span<const double, 3> s2_v0(t2_v0, 3);
        span<const double, 3> s2_v1(t2_v1, 3);
        span<const double, 3> s2_v2(t2_v2, 3);

        // Degenerate triangles should be handled gracefully
        auto result = triangle_triangle_intersection(s1_v0, s1_v1, s1_v2, s2_v0, s2_v1, s2_v2);
        // Accept any result for degenerate case
        (void)result;
    }

    SECTION("Coplanar - bowtie configuration (sharing single vertex)")
    {
        // ._____.
        //  \   /
        //   \ /
        //    *
        //   / \.
        //  /   \.
        // /_____\.

        // Triangle 1
        double t1_v0[3] = {0.0, 0.0, 0.0}; // Shared vertex
        double t1_v1[3] = {1.0, 1.0, 0.0};
        double t1_v2[3] = {-1.0, 1.0, 0.0};

        // Triangle 2
        double t2_v0[3] = {0.0, 0.0, 0.0}; // Shared vertex
        double t2_v1[3] = {-1.0, -1.0, 0.0};
        double t2_v2[3] = {1.0, -1.0, 0.0};

        span<const double, 3> s1_v0(t1_v0, 3);
        span<const double, 3> s1_v1(t1_v1, 3);
        span<const double, 3> s1_v2(t1_v2, 3);
        span<const double, 3> s2_v0(t2_v0, 3);
        span<const double, 3> s2_v1(t2_v1, 3);
        span<const double, 3> s2_v2(t2_v2, 3);

        // Boundary OFF: No intersection (only touching at shared vertex)
        REQUIRE_FALSE(triangle_triangle_intersection(
            s1_v0,
            s1_v1,
            s1_v2,
            s2_v0,
            s2_v1,
            s2_v2,
            IncludeBoundaryIntersection::No));

        // Boundary ON: Intersection (touching at vertex counts)
        REQUIRE(triangle_triangle_intersection(
            s1_v0,
            s1_v1,
            s1_v2,
            s2_v0,
            s2_v1,
            s2_v2,
            IncludeBoundaryIntersection::Yes));
    }

    SECTION("Coplanar - nested triangles (sharing single vertex)")
    {
        // Large triangle
        double t1_v0[3] = {0.0, 0.0, 0.0}; // Shared vertex
        double t1_v1[3] = {3.0, 0.0, 0.0};
        double t1_v2[3] = {0.0, 3.0, 0.0};

        // Small triangle (nested inside, sharing one vertex)
        // Positioned so its interior overlaps with large triangle's interior
        double t2_v0[3] = {0.0, 0.0, 0.0}; // Shared vertex
        double t2_v1[3] = {1.5, 0.5, 0.0}; // Inside large triangle
        double t2_v2[3] = {0.5, 1.5, 0.0}; // Inside large triangle

        span<const double, 3> s1_v0(t1_v0, 3);
        span<const double, 3> s1_v1(t1_v1, 3);
        span<const double, 3> s1_v2(t1_v2, 3);
        span<const double, 3> s2_v0(t2_v0, 3);
        span<const double, 3> s2_v1(t2_v1, 3);
        span<const double, 3> s2_v2(t2_v2, 3);

        // Boundary OFF: Intersection (small triangle's interior overlaps large triangle's interior)
        REQUIRE(triangle_triangle_intersection(
            s1_v0,
            s1_v1,
            s1_v2,
            s2_v0,
            s2_v1,
            s2_v2,
            IncludeBoundaryIntersection::No));

        // Boundary ON: Intersection (same, plus the shared vertex counts)
        REQUIRE(triangle_triangle_intersection(
            s1_v0,
            s1_v1,
            s1_v2,
            s2_v0,
            s2_v1,
            s2_v2,
            IncludeBoundaryIntersection::Yes));
    }

    SECTION("Coplanar - nested triangles (sharing single sub-edge)")
    {
        // Large triangle
        double t1_v0[3] = {0.0, 0.0, 0.0};
        double t1_v1[3] = {3.0, 0.0, 0.0};
        double t1_v2[3] = {0.0, 3.0, 0.0};

        // Small triangle (nested inside, sharing a sub-edge)
        double t2_v0[3] = {1.0, 0.0, 0.0};
        double t2_v1[3] = {2.0, 0.0, 0.0};
        double t2_v2[3] = {1.5, 1.5, 0.0};

        span<const double, 3> s1_v0(t1_v0, 3);
        span<const double, 3> s1_v1(t1_v1, 3);
        span<const double, 3> s1_v2(t1_v2, 3);
        span<const double, 3> s2_v0(t2_v0, 3);
        span<const double, 3> s2_v1(t2_v1, 3);
        span<const double, 3> s2_v2(t2_v2, 3);

        // Boundary OFF: Intersection (small triangle's interior overlaps large triangle's interior)
        REQUIRE(triangle_triangle_intersection(
            s1_v0,
            s1_v1,
            s1_v2,
            s2_v0,
            s2_v1,
            s2_v2,
            IncludeBoundaryIntersection::No));

        // Boundary ON: Intersection (same, plus the shared sub-edge)
        REQUIRE(triangle_triangle_intersection(
            s1_v0,
            s1_v1,
            s1_v2,
            s2_v0,
            s2_v1,
            s2_v2,
            IncludeBoundaryIntersection::Yes));
    }

    SECTION("Coplanar - sharing edge, no interior intersection (valid fold)")
    {
        // Two coplanar triangles sharing an edge, forming a valid "fold" or "butterfly"
        // Their interiors do NOT overlap
        //
        //      (0.5,1,0)
        //         /\.
        //        /  \.
        //       /    \.
        //  (0,0,0)---(1,0,0)  <- shared edge
        //       \    /
        //        \  /
        //         \/
        //      (0.5,-1,0)

        // Triangle 1 (upper)
        double t1_v0[3] = {0.0, 0.0, 0.0}; // Shared edge vertex 1
        double t1_v1[3] = {1.0, 0.0, 0.0}; // Shared edge vertex 2
        double t1_v2[3] = {0.5, 1.0, 0.0}; // Unique vertex

        // Triangle 2 (lower)
        double t2_v0[3] = {0.0, 0.0, 0.0}; // Shared edge vertex 1
        double t2_v1[3] = {1.0, 0.0, 0.0}; // Shared edge vertex 2
        double t2_v2[3] = {0.5, -1.0, 0.0}; // Unique vertex

        span<const double, 3> s1_v0(t1_v0, 3);
        span<const double, 3> s1_v1(t1_v1, 3);
        span<const double, 3> s1_v2(t1_v2, 3);
        span<const double, 3> s2_v0(t2_v0, 3);
        span<const double, 3> s2_v1(t2_v1, 3);
        span<const double, 3> s2_v2(t2_v2, 3);

        // Boundary OFF: No intersection (only share edge, interiors don't overlap)
        REQUIRE_FALSE(triangle_triangle_intersection(
            s1_v0,
            s1_v1,
            s1_v2,
            s2_v0,
            s2_v1,
            s2_v2,
            IncludeBoundaryIntersection::No));

        // Boundary ON: Intersection (shared edge counts as contact)
        REQUIRE(triangle_triangle_intersection(
            s1_v0,
            s1_v1,
            s1_v2,
            s2_v0,
            s2_v1,
            s2_v2,
            IncludeBoundaryIntersection::Yes));
    }

    SECTION("Coplanar - sharing edge, WITH interior intersection (invalid geometry)")
    {
        // Two coplanar triangles sharing an edge, with interiors overlapping
        // This represents INVALID geometry where adjacent faces improperly overlap
        //
        //  Triangle 1: (0,0,0)-(1,0,0)-(0.5,1,0)
        //  Triangle 2: (0,0,0)-(1,0,0)-(0.5,0.5,0)
        //
        // Triangle 2's apex (0.5,0.5,0) is inside Triangle 1

        // Triangle 1: standard triangle
        double t1_v0[3] = {0.0, 0.0, 0.0}; // Shared edge vertex 1
        double t1_v1[3] = {1.0, 0.0, 0.0}; // Shared edge vertex 2
        double t1_v2[3] = {0.5, 1.0, 0.0}; // Apex

        // Triangle 2: shares edge (0,0,0)-(1,0,0), apex inside Triangle 1
        double t2_v0[3] = {0.0, 0.0, 0.0}; // Shared edge vertex 1
        double t2_v1[3] = {1.0, 0.0, 0.0}; // Shared edge vertex 2
        double t2_v2[3] = {0.5, 0.5, 0.0}; // Apex inside Triangle 1's interior

        span<const double, 3> s1_v0(t1_v0, 3);
        span<const double, 3> s1_v1(t1_v1, 3);
        span<const double, 3> s1_v2(t1_v2, 3);
        span<const double, 3> s2_v0(t2_v0, 3);
        span<const double, 3> s2_v1(t2_v1, 3);
        span<const double, 3> s2_v2(t2_v2, 3);

        // Boundary OFF: Intersection (Triangle 2's apex is in Triangle 1's interior)
        REQUIRE(triangle_triangle_intersection(
            s1_v0,
            s1_v1,
            s1_v2,
            s2_v0,
            s2_v1,
            s2_v2,
            IncludeBoundaryIntersection::No));

        // Boundary ON: Intersection (both interior overlap and shared edge)
        REQUIRE(triangle_triangle_intersection(
            s1_v0,
            s1_v1,
            s1_v2,
            s2_v0,
            s2_v1,
            s2_v2,
            IncludeBoundaryIntersection::Yes));
    }

    SECTION("Touching triangles - edge tip meets edge interior (non-coplanar)")
    {
        // T1's edge (1,1,-1)→(1,-1,1) crosses T2's plane (z=0) at (1,0,0),
        // which is the midpoint of T2's edge (0,0,0)→(2,0,0).
        // This is a boundary-only contact (edge piercing a triangle edge interior,
        // not the triangle interior), so it should NOT count as an intersection
        // in strict mode.
        //
        //   T1_v0 (1, 1,-1)
        //          |
        // (1,0,0) [*]  <- hits interior of T2 edge (0,0,0)-(2,0,0)
        //          |
        //   T1_v1 (1,-1, 1)

        double t1_v0[3] = {1.0, 1.0, -1.0};
        double t1_v1[3] = {1.0, -1.0, 1.0};
        double t1_v2[3] = {0.0, 1.0, -2.0};

        double t2_v0[3] = {0.0, 0.0, 0.0};
        double t2_v1[3] = {2.0, 0.0, 0.0};
        double t2_v2[3] = {0.0, 2.0, 0.0};

        span<const double, 3> s1_v0(t1_v0, 3);
        span<const double, 3> s1_v1(t1_v1, 3);
        span<const double, 3> s1_v2(t1_v2, 3);
        span<const double, 3> s2_v0(t2_v0, 3);
        span<const double, 3> s2_v1(t2_v1, 3);
        span<const double, 3> s2_v2(t2_v2, 3);

        // Boundary OFF: No intersection (contact is only on T2's edge, not its interior)
        REQUIRE_FALSE(triangle_triangle_intersection(
            s1_v0,
            s1_v1,
            s1_v2,
            s2_v0,
            s2_v1,
            s2_v2,
            IncludeBoundaryIntersection::No));

        // Boundary ON: Intersection (edge-touching-edge counts as contact)
        REQUIRE(triangle_triangle_intersection(
            s1_v0,
            s1_v1,
            s1_v2,
            s2_v0,
            s2_v1,
            s2_v2,
            IncludeBoundaryIntersection::Yes));
    }

    SECTION("Degenerate triangle - collinear vertices")
    {
        // A degenerate triangle where all three vertices are collinear (zero area)
        // Such triangles should not have interior intersection with any valid triangle

        // Degenerate triangle: all vertices on a line segment
        double degenerate_v0[3] = {0.0, 0.0, 0.0};
        double degenerate_v1[3] = {1.0, 0.0, 0.0};
        double degenerate_v2[3] = {0.5, 0.0, 0.0}; // On the line segment [v0, v1]

        // Valid triangle that intersects the line segment
        double valid_v0[3] = {0.5, -1.0, 0.0};
        double valid_v1[3] = {0.5, 1.0, 0.0};
        double valid_v2[3] = {1.5, 0.0, 0.0};

        span<const double, 3> s_degen_v0(degenerate_v0, 3);
        span<const double, 3> s_degen_v1(degenerate_v1, 3);
        span<const double, 3> s_degen_v2(degenerate_v2, 3);
        span<const double, 3> s_valid_v0(valid_v0, 3);
        span<const double, 3> s_valid_v1(valid_v1, 3);
        span<const double, 3> s_valid_v2(valid_v2, 3);

        // Boundary OFF: No interior intersection (degenerate has zero area, no interior)
        REQUIRE_FALSE(triangle_triangle_intersection(
            s_degen_v0,
            s_degen_v1,
            s_degen_v2,
            s_valid_v0,
            s_valid_v1,
            s_valid_v2,
            IncludeBoundaryIntersection::No));

        // Boundary ON: Intersection at segment (0.5, 0) to (1, 0)
        REQUIRE(triangle_triangle_intersection(
            s_degen_v0,
            s_degen_v1,
            s_degen_v2,
            s_valid_v0,
            s_valid_v1,
            s_valid_v2,
            IncludeBoundaryIntersection::Yes));
    }

    SECTION("Degenerate triangle - edge intersection")
    {
        // Degenerate triangle: all vertices on a line segment
        double degenerate_v0[3] = {0.0, 0.0, 0.0};
        double degenerate_v1[3] = {1.0, 0.0, 0.0};
        double degenerate_v2[3] = {0.9, 0.0, 0.0}; // On the line segment [v0, v1]

        // Valid triangle that intersects the line segment
        double valid_v0[3] = {0.5, -1.0, 0.0};
        double valid_v1[3] = {0.5, 1.0, 0.0};
        double valid_v2[3] = {0.0, 0.0, 1.0};

        span<const double, 3> s_degen_v0(degenerate_v0, 3);
        span<const double, 3> s_degen_v1(degenerate_v1, 3);
        span<const double, 3> s_degen_v2(degenerate_v2, 3);
        span<const double, 3> s_valid_v0(valid_v0, 3);
        span<const double, 3> s_valid_v1(valid_v1, 3);
        span<const double, 3> s_valid_v2(valid_v2, 3);

        // Boundary OFF: No interior intersection (degenerate has zero area, no interior)
        REQUIRE_FALSE(triangle_triangle_intersection(
            s_degen_v0,
            s_degen_v1,
            s_degen_v2,
            s_valid_v0,
            s_valid_v1,
            s_valid_v2,
            IncludeBoundaryIntersection::No));

        // Boundary ON: Intersection at point (0.5, 0, 0)
        REQUIRE(triangle_triangle_intersection(
            s_degen_v0,
            s_degen_v1,
            s_degen_v2,
            s_valid_v0,
            s_valid_v1,
            s_valid_v2,
            IncludeBoundaryIntersection::Yes));
    }

    SECTION("Degenerate triangle - all vertices at same point")
    {
        // A completely degenerate triangle: all vertices at the same point

        // Degenerate triangle: single point
        double degenerate_v0[3] = {0.5, 0.5, 0.0};
        double degenerate_v1[3] = {0.5, 0.5, 0.0};
        double degenerate_v2[3] = {0.5, 0.5, 0.0};

        // Valid triangle containing the degenerate point
        double valid_v0[3] = {0.0, 0.0, 0.0};
        double valid_v1[3] = {1.0, 0.0, 0.0};
        double valid_v2[3] = {0.5, 1.0, 0.0};

        span<const double, 3> s_degen_v0(degenerate_v0, 3);
        span<const double, 3> s_degen_v1(degenerate_v1, 3);
        span<const double, 3> s_degen_v2(degenerate_v2, 3);
        span<const double, 3> s_valid_v0(valid_v0, 3);
        span<const double, 3> s_valid_v1(valid_v1, 3);
        span<const double, 3> s_valid_v2(valid_v2, 3);

        // Boundary OFF: No interior intersection (degenerate has zero area)
        REQUIRE_FALSE(triangle_triangle_intersection(
            s_degen_v0,
            s_degen_v1,
            s_degen_v2,
            s_valid_v0,
            s_valid_v1,
            s_valid_v2,
            IncludeBoundaryIntersection::No));

        // Boundary ON: Intersection at the single point (0.5, 0.5, 0)
        REQUIRE(triangle_triangle_intersection(
            s_degen_v0,
            s_degen_v1,
            s_degen_v2,
            s_valid_v0,
            s_valid_v1,
            s_valid_v2,
            IncludeBoundaryIntersection::Yes));
    }

    SECTION("Degenerate triangle - all vertices at edge midpoint")
    {
        // A completely degenerate triangle: all vertices at the same point

        // Degenerate triangle: single point
        double degenerate_v0[3] = {0.5, 0.0, 0.0};
        double degenerate_v1[3] = {0.5, 0.0, 0.0};
        double degenerate_v2[3] = {0.5, 0.0, 0.0};

        // Valid triangle containing the degenerate point
        double valid_v0[3] = {0.0, 0.0, 0.0};
        double valid_v1[3] = {1.0, 0.0, 0.0};
        double valid_v2[3] = {0.5, 1.0, 0.0};

        span<const double, 3> s_degen_v0(degenerate_v0, 3);
        span<const double, 3> s_degen_v1(degenerate_v1, 3);
        span<const double, 3> s_degen_v2(degenerate_v2, 3);
        span<const double, 3> s_valid_v0(valid_v0, 3);
        span<const double, 3> s_valid_v1(valid_v1, 3);
        span<const double, 3> s_valid_v2(valid_v2, 3);

        // Boundary OFF: No interior intersection (degenerate has zero area)
        REQUIRE_FALSE(triangle_triangle_intersection(
            s_degen_v0,
            s_degen_v1,
            s_degen_v2,
            s_valid_v0,
            s_valid_v1,
            s_valid_v2,
            IncludeBoundaryIntersection::No));

        // Boundary ON: Intersection at the single point (0.5, 0.0, 0)
        REQUIRE(triangle_triangle_intersection(
            s_degen_v0,
            s_degen_v1,
            s_degen_v2,
            s_valid_v0,
            s_valid_v1,
            s_valid_v2,
            IncludeBoundaryIntersection::Yes));
    }

    SECTION("Two degenerate triangle - both segments")
    {
        // Degenerate triangle: horizontal line segment
        double degenerate_v0[3] = {-1.0, 0.1, 0.0};
        double degenerate_v1[3] = {0.0, 0.1, 0.0};
        double degenerate_v2[3] = {1.0, 0.1, 0.0};

        // Degenerate triangle: vertical line segment
        double valid_v0[3] = {0.1, -1.0, 0.0};
        double valid_v1[3] = {0.1, 0.0, 0.0};
        double valid_v2[3] = {0.1, 1.0, 0.0};

        span<const double, 3> s_degen_v0(degenerate_v0, 3);
        span<const double, 3> s_degen_v1(degenerate_v1, 3);
        span<const double, 3> s_degen_v2(degenerate_v2, 3);
        span<const double, 3> s_valid_v0(valid_v0, 3);
        span<const double, 3> s_valid_v1(valid_v1, 3);
        span<const double, 3> s_valid_v2(valid_v2, 3);

        // Boundary OFF: No interior intersection (both triangle have zero area)
        REQUIRE_FALSE(triangle_triangle_intersection(
            s_degen_v0,
            s_degen_v1,
            s_degen_v2,
            s_valid_v0,
            s_valid_v1,
            s_valid_v2,
            IncludeBoundaryIntersection::No));

        // Boundary ON: Intersection at the single point (0.1, 0.1, 0)
        REQUIRE(triangle_triangle_intersection(
            s_degen_v0,
            s_degen_v1,
            s_degen_v2,
            s_valid_v0,
            s_valid_v1,
            s_valid_v2,
            IncludeBoundaryIntersection::Yes));
    }

    SECTION("Two degenerate triangle - both segments but not touching")
    {
        // Degenerate triangle: horizontal line segment
        double degenerate_v0[3] = {-1.0, 1.1, 0.0};
        double degenerate_v1[3] = {0.0, 1.1, 0.0};
        double degenerate_v2[3] = {1.0, 1.1, 0.0};

        // Degenerate triangle: vertical line segment
        double valid_v0[3] = {0.1, -1.0, 0.0};
        double valid_v1[3] = {0.1, 0.0, 0.0};
        double valid_v2[3] = {0.1, 1.0, 0.0};

        span<const double, 3> s_degen_v0(degenerate_v0, 3);
        span<const double, 3> s_degen_v1(degenerate_v1, 3);
        span<const double, 3> s_degen_v2(degenerate_v2, 3);
        span<const double, 3> s_valid_v0(valid_v0, 3);
        span<const double, 3> s_valid_v1(valid_v1, 3);
        span<const double, 3> s_valid_v2(valid_v2, 3);

        // Boundary OFF: No interior intersection (both triangle have zero area)
        REQUIRE_FALSE(triangle_triangle_intersection(
            s_degen_v0,
            s_degen_v1,
            s_degen_v2,
            s_valid_v0,
            s_valid_v1,
            s_valid_v2,
            IncludeBoundaryIntersection::No));

        // Boundary ON: No intersection (degenerate segments do not touch)
        REQUIRE_FALSE(triangle_triangle_intersection(
            s_degen_v0,
            s_degen_v1,
            s_degen_v2,
            s_valid_v0,
            s_valid_v1,
            s_valid_v2,
            IncludeBoundaryIntersection::Yes));
    }

    SECTION("Two degenerate triangle - non coplanar")
    {
        // Degenerate triangle: horizontal line segment in plane z=0
        double degenerate_v0[3] = {-1.0, 0.0, 0.0};
        double degenerate_v1[3] = {0.0, 0.0, 0.0};
        double degenerate_v2[3] = {1.0, 0.0, 0.0};

        // Degenerate triangle: vertical line segment in plane z=1
        double valid_v0[3] = {0.0, -1.0, 1.0};
        double valid_v1[3] = {0.0, 0.0, 1.0};
        double valid_v2[3] = {0.0, 1.0, 1.0};

        span<const double, 3> s_degen_v0(degenerate_v0, 3);
        span<const double, 3> s_degen_v1(degenerate_v1, 3);
        span<const double, 3> s_degen_v2(degenerate_v2, 3);
        span<const double, 3> s_valid_v0(valid_v0, 3);
        span<const double, 3> s_valid_v1(valid_v1, 3);
        span<const double, 3> s_valid_v2(valid_v2, 3);

        // Boundary OFF: No interior intersection (both triangle have zero area)
        REQUIRE_FALSE(triangle_triangle_intersection(
            s_degen_v0,
            s_degen_v1,
            s_degen_v2,
            s_valid_v0,
            s_valid_v1,
            s_valid_v2,
            IncludeBoundaryIntersection::No));

        // Boundary ON: No intersection (degenerate segments do not touch)
        REQUIRE_FALSE(triangle_triangle_intersection(
            s_degen_v0,
            s_degen_v1,
            s_degen_v2,
            s_valid_v0,
            s_valid_v1,
            s_valid_v2,
            IncludeBoundaryIntersection::Yes));
    }

    SECTION("Two degenerate triangle - different points")
    {
        // Degenerate triangle: (0, 0, 0)
        double degenerate_v0[3] = {0.0, 0.0, 0.0};
        double degenerate_v1[3] = {0.0, 0.0, 0.0};
        double degenerate_v2[3] = {0.0, 0.0, 0.0};

        // Degenerate triangle: (0, 0, 1)
        double valid_v0[3] = {0.0, 0.0, 1.0};
        double valid_v1[3] = {0.0, 0.0, 1.0};
        double valid_v2[3] = {0.0, 0.0, 1.0};

        span<const double, 3> s_degen_v0(degenerate_v0, 3);
        span<const double, 3> s_degen_v1(degenerate_v1, 3);
        span<const double, 3> s_degen_v2(degenerate_v2, 3);
        span<const double, 3> s_valid_v0(valid_v0, 3);
        span<const double, 3> s_valid_v1(valid_v1, 3);
        span<const double, 3> s_valid_v2(valid_v2, 3);

        // Boundary OFF: No interior intersection (both triangle have zero area)
        REQUIRE_FALSE(triangle_triangle_intersection(
            s_degen_v0,
            s_degen_v1,
            s_degen_v2,
            s_valid_v0,
            s_valid_v1,
            s_valid_v2,
            IncludeBoundaryIntersection::No));

        // Boundary ON: No intersection (degenerate segments do not touch)
        REQUIRE_FALSE(triangle_triangle_intersection(
            s_degen_v0,
            s_degen_v1,
            s_degen_v2,
            s_valid_v0,
            s_valid_v1,
            s_valid_v2,
            IncludeBoundaryIntersection::Yes));
    }

    SECTION("Two degenerate triangle - same points")
    {
        // Degenerate triangle: (0, 0, 0)
        double degenerate_v0[3] = {0.0, 0.0, 0.0};
        double degenerate_v1[3] = {0.0, 0.0, 0.0};
        double degenerate_v2[3] = {0.0, 0.0, 0.0};

        // Degenerate triangle: (0, 0, 1)
        double valid_v0[3] = {0.0, 0.0, 0.0};
        double valid_v1[3] = {0.0, 0.0, 0.0};
        double valid_v2[3] = {0.0, 0.0, 0.0};

        span<const double, 3> s_degen_v0(degenerate_v0, 3);
        span<const double, 3> s_degen_v1(degenerate_v1, 3);
        span<const double, 3> s_degen_v2(degenerate_v2, 3);
        span<const double, 3> s_valid_v0(valid_v0, 3);
        span<const double, 3> s_valid_v1(valid_v1, 3);
        span<const double, 3> s_valid_v2(valid_v2, 3);

        // Boundary OFF: No interior intersection (both triangle have zero area)
        REQUIRE_FALSE(triangle_triangle_intersection(
            s_degen_v0,
            s_degen_v1,
            s_degen_v2,
            s_valid_v0,
            s_valid_v1,
            s_valid_v2,
            IncludeBoundaryIntersection::No));

        // Boundary ON: intersection at the single point (0, 0, 0)
        REQUIRE(triangle_triangle_intersection(
            s_degen_v0,
            s_degen_v1,
            s_degen_v2,
            s_valid_v0,
            s_valid_v1,
            s_valid_v2,
            IncludeBoundaryIntersection::Yes));
    }

    SECTION("Degenerate triangle along z, non-degenerate triangle separated in x")
    {
        // T2 is a degenerate triangle collinear along the z-axis at x=y=0.
        // T1 is a non-degenerate triangle at z=0.5 but x in [1,2], so the only axis
        // separating them in 3D is x. A normal-based projection picks the x-axis as the
        // discarded one (T2's normal is zero so axis defaults to 0), causing a false
        // positive. The fix uses a bbox-based axis pick over all 5 relevant points.
        double t1_v0[3] = {1.0, 0.0, 0.5};
        double t1_v1[3] = {2.0, 0.0, 0.5};
        double t1_v2[3] = {1.0, 1.0, 0.5};
        double t2_v0[3] = {0.0, 0.0, 0.0};
        double t2_v1[3] = {0.0, 0.0, 1.0};
        double t2_v2[3] = {0.0, 0.0, 2.0};

        span<const double, 3> s1_v0(t1_v0, 3);
        span<const double, 3> s1_v1(t1_v1, 3);
        span<const double, 3> s1_v2(t1_v2, 3);
        span<const double, 3> s2_v0(t2_v0, 3);
        span<const double, 3> s2_v1(t2_v1, 3);
        span<const double, 3> s2_v2(t2_v2, 3);

        REQUIRE_FALSE(triangle_triangle_intersection(
            s1_v0,
            s1_v1,
            s1_v2,
            s2_v0,
            s2_v1,
            s2_v2,
            IncludeBoundaryIntersection::No));

        REQUIRE_FALSE(triangle_triangle_intersection(
            s1_v0,
            s1_v1,
            s1_v2,
            s2_v0,
            s2_v1,
            s2_v2,
            IncludeBoundaryIntersection::Yes));
    }
}
