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
#include <lagrange/bvh/compute_uv_overlap.h>

#include <lagrange/Attribute.h>
#include <lagrange/ExactPredicatesShewchuk.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/bvh/AABB.h>
#include <lagrange/compute_facet_facet_adjacency.h>
#include <lagrange/internal/find_attribute_utils.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/triangle_area.h>
#include <lagrange/uv_mesh.h>
#include <lagrange/views.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <tbb/enumerable_thread_specific.h>
#include <tbb/parallel_for.h>
#include <tbb/parallel_invoke.h>
#include <tbb/parallel_sort.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <algorithm>
#include <array>
#include <deque>
#include <limits>
#include <numeric>
#include <optional>
#include <utility>
#include <vector>

namespace lagrange::bvh {

namespace {

// ---------------------------------------------------------------------------
// TBB helper
// ---------------------------------------------------------------------------

/// Flatten a tbb::enumerable_thread_specific<std::vector<T>> into a single vector.
template <typename T>
std::vector<T> flatten_thread_local(tbb::enumerable_thread_specific<std::vector<T>>& tls)
{
    size_t total = 0;
    for (auto& v : tls) total += v.size();
    std::vector<T> result;
    result.reserve(total);
    for (auto& v : tls) {
        result.insert(result.end(), v.begin(), v.end());
    }
    return result;
}

// ---------------------------------------------------------------------------
// 2-D geometry types
// ---------------------------------------------------------------------------

template <typename Scalar>
using Vec2 = Eigen::Matrix<Scalar, 2, 1>;

/// A small convex polygon with compile-time max vertex count (no heap allocation).
template <typename Scalar, int MaxVerts>
struct SmallPoly2D
{
    std::array<Vec2<Scalar>, MaxVerts> verts;
    int n = 0;
};

// Clipping a triangle (3 verts) against 3 half-planes adds at most 1 vertex per step:
//   3 → 4 → 5 → 6.  Use 8 for safety.
template <typename Scalar>
using ClipPoly = SmallPoly2D<Scalar, 8>;

/// Convert a Vec2<Scalar> to a double[2] array for orient2D predicates.
template <typename Scalar>
std::array<double, 2> to_double2(const Vec2<Scalar>& v)
{
    return {static_cast<double>(v.x()), static_cast<double>(v.y())};
}

// ---------------------------------------------------------------------------
// Sutherland-Hodgman clipping helpers
// ---------------------------------------------------------------------------

///
/// Clip @p poly_in against the CCW half-plane defined by edge (edge_a → edge_b).
/// "Inside" = orient2D(edge_a, edge_b, p) >= 0 (left side or on the line).
/// Intersection points are computed with floating-point arithmetic.
/// Results are placed in @p poly_out (which is overwritten).
///
template <typename Scalar>
void clip_by_halfplane(
    const Vec2<Scalar>& edge_a,
    const Vec2<Scalar>& edge_b,
    const ClipPoly<Scalar>& poly_in,
    ClipPoly<Scalar>& poly_out,
    const ExactPredicates& pred)
{
    poly_out.n = 0;

    auto da = to_double2(edge_a);
    auto db = to_double2(edge_b);

    for (int k = 0; k < poly_in.n; ++k) {
        const Vec2<Scalar>& curr = poly_in.verts[k];
        const Vec2<Scalar>& next = poly_in.verts[(k + 1) % poly_in.n];

        auto dc = to_double2(curr);
        auto dn = to_double2(next);
        const short s_curr = pred.orient2D(da.data(), db.data(), dc.data());
        const short s_next = pred.orient2D(da.data(), db.data(), dn.data());

        if (s_curr >= 0) {
            // curr is inside or on the clipping line → include it
            poly_out.verts[poly_out.n++] = curr;
        }

        if ((s_curr > 0 && s_next < 0) || (s_curr < 0 && s_next > 0)) {
            // The segment (curr → next) straddles the clipping line.
            // Compute floating-point intersection via cross-product parameterisation.
            const Scalar ex = edge_b.x() - edge_a.x();
            const Scalar ey = edge_b.y() - edge_a.y();
            const Scalar d_curr = ex * (curr.y() - edge_a.y()) - ey * (curr.x() - edge_a.x());
            const Scalar d_next = ex * (next.y() - edge_a.y()) - ey * (next.x() - edge_a.x());
            const Scalar t = d_curr / (d_curr - d_next);
            poly_out.verts[poly_out.n++] = curr + t * (next - curr);
        }
    }
}

// ---------------------------------------------------------------------------
// Exact separating-axis test
// ---------------------------------------------------------------------------

///
/// Return true iff triangles @p a and @p b have a positive-area interior intersection.
///
/// Uses exact orient2D predicates for a separating-axis test (SAT).  Two triangles are
/// rejected (no interior overlap) if there exists an edge of either triangle such that all
/// three vertices of the other triangle lie on the exterior-or-boundary side.  This
/// correctly handles adjacent triangles that share only an edge or a vertex.
///
template <typename Scalar>
bool triangles_have_interior_overlap(
    const Vec2<Scalar> a[3],
    const Vec2<Scalar> b[3],
    const ExactPredicates& pred)
{
    // Determine orientations to normalize to CCW.
    auto da0 = to_double2(a[0]);
    auto da1 = to_double2(a[1]);
    auto da2 = to_double2(a[2]);
    const short orient_a = pred.orient2D(da0.data(), da1.data(), da2.data());
    if (orient_a == 0) return false; // degenerate A — no interior

    auto db0 = to_double2(b[0]);
    auto db1 = to_double2(b[1]);
    auto db2 = to_double2(b[2]);
    const short orient_b = pred.orient2D(db0.data(), db1.data(), db2.data());
    if (orient_b == 0) return false; // degenerate B — no interior

    // Normalize to CCW: if CW, swap vertices 1 and 2 to reverse orientation.
    const Vec2<Scalar>* pa[3] = {&a[0], &a[1], &a[2]};
    const Vec2<Scalar>* pb[3] = {&b[0], &b[1], &b[2]};
    if (orient_a < 0) std::swap(pa[1], pa[2]);
    if (orient_b < 0) std::swap(pb[1], pb[2]);

    // For a CCW triangle, the interior of edge (p0 → p1) is the LEFT half-plane,
    // i.e. orient2D(p0, p1, q) > 0.
    //
    // A separating axis exists if ALL three vertices of the opposite triangle lie on the
    // exterior-or-boundary side: orient2D(p0, p1, q) <= 0 for all three.
    //
    // We test edges of A against B and edges of B against A.

    auto has_separating_edge =
        [&pred](const Vec2<Scalar>* tri[3], const Vec2<Scalar>* opp[3]) -> bool {
        for (int k = 0; k < 3; ++k) {
            auto dp0 = to_double2(*tri[k]);
            auto dp1 = to_double2(*tri[(k + 1) % 3]);
            bool all_outside = true;
            for (int j = 0; j < 3; ++j) {
                auto dq = to_double2(*opp[j]);
                if (pred.orient2D(dp0.data(), dp1.data(), dq.data()) > 0) {
                    all_outside = false;
                    break;
                }
            }
            if (all_outside) return true;
        }
        return false;
    };

    if (has_separating_edge(pa, pb)) return false;
    if (has_separating_edge(pb, pa)) return false;
    return true;
}

// ---------------------------------------------------------------------------
// Triangle-triangle intersection area (Sutherland-Hodgman)
// ---------------------------------------------------------------------------

///
/// Compute the area of the intersection of two UV-space triangles.
///
/// Returns 0 if the triangles do not overlap or are degenerate.
/// Caller should first run triangles_have_interior_overlap() to skip most zero-area cases.
///
template <typename Scalar>
Scalar triangle_intersection_area_2d(
    const Vec2<Scalar> a[3],
    const Vec2<Scalar> b[3],
    const ExactPredicates& pred)
{
    // Normalize a to CCW so that the S-H half-planes are oriented correctly.
    // Use exact orientation predicate for robustness with near-degenerate or
    // large-coordinate triangles.
    Vec2<Scalar> a_ccw[3] = {a[0], a[1], a[2]};
    {
        auto pa = to_double2(a_ccw[0]);
        auto pb = to_double2(a_ccw[1]);
        auto pc = to_double2(a_ccw[2]);
        const short o = pred.orient2D(pa.data(), pb.data(), pc.data());
        if (o < 0) {
            std::swap(a_ccw[1], a_ccw[2]);
        } else if (o == 0) {
            return Scalar(0); // degenerate triangle
        }
    }

    // Initialize polygon with the vertices of b.
    ClipPoly<Scalar> poly, tmp;
    poly.verts[0] = b[0];
    poly.verts[1] = b[1];
    poly.verts[2] = b[2];
    poly.n = 3;

    // Clip b against each CCW edge of a.
    for (int k = 0; k < 3 && poly.n > 0; ++k) {
        clip_by_halfplane(a_ccw[k], a_ccw[(k + 1) % 3], poly, tmp, pred);
        std::swap(poly, tmp);
    }

    if (poly.n < 3) return Scalar(0);

    // Shoelace formula for the area of the clipped polygon.
    Scalar area2 = Scalar(0);
    for (int i = 0; i < poly.n; ++i) {
        const int j = (i + 1) % poly.n;
        area2 += poly.verts[i].x() * poly.verts[j].y() - poly.verts[j].x() * poly.verts[i].y();
    }
    return std::abs(area2) * Scalar(0.5);
}

// ---------------------------------------------------------------------------
// Candidate pair processor (SAT + area with fast adjacent-triangle rejection)
// ---------------------------------------------------------------------------

///
/// Process a single candidate pair of triangles: check for overlap and compute intersection area.
///
/// For adjacent triangles sharing exactly 2 UV vertex indices (a shared edge), a fast orient2D
/// check determines whether the opposite vertices lie on the same side of the shared edge.
/// The common case (opposite sides = properly unfolded) is rejected with a single orient2D call
/// per vertex, avoiding the full 6-edge SAT.  Only when both opposite vertices are on the same
/// side (a UV fold/overlap) do we proceed to area computation.
///
/// @param area_out  If non-null, the intersection area is computed via Sutherland-Hodgman
///                  clipping and written to *area_out.  If null, only overlap detection is
///                  performed (faster).
/// @return true if the pair has an interior overlap.
///
template <typename Scalar, typename Index, typename UVVerts, typename UVFaces>
bool process_candidate_pair(
    Index i,
    Index j,
    const UVVerts& uv_verts,
    const UVFaces& uv_facets,
    const ExactPredicates& pred,
    Scalar* area_out)
{
    if (area_out) *area_out = Scalar(0);

    // Count shared UV vertex indices (cheap index comparisons only).
    int shared = 0;
    int sa[2] = {}, sb[2] = {};
    for (int ka = 0; ka < 3; ++ka) {
        for (int kb = 0; kb < 3; ++kb) {
            if (uv_facets(i, ka) == uv_facets(j, kb)) {
                if (shared < 2) {
                    sa[shared] = ka;
                    sb[shared] = kb;
                }
                ++shared;
                break; // each vertex of i matches at most one vertex of j
            }
        }
    }

    auto load_vert = [&](Index f, int k) -> Vec2<Scalar> {
        const Index v = uv_facets(f, k);
        return Vec2<Scalar>(uv_verts(v, 0), uv_verts(v, 1));
    };

    if (shared == 2) {
        // Adjacent triangles sharing an edge.  Check whether the opposite vertices lie on the
        // same side of the shared edge line.  If they are on opposite sides (the common case
        // for properly unfolded UV maps), there is no interior overlap.
        const int opp_a = 3 - sa[0] - sa[1];
        const int opp_b = 3 - sb[0] - sb[1];
        auto ds0 = to_double2(load_vert(i, sa[0]));
        auto ds1 = to_double2(load_vert(i, sa[1]));
        auto da = to_double2(load_vert(i, opp_a));
        auto db = to_double2(load_vert(j, opp_b));
        const short oa = pred.orient2D(ds0.data(), ds1.data(), da.data());
        const short ob = pred.orient2D(ds0.data(), ds1.data(), db.data());
        // Different sides or collinear → no interior overlap.
        // Same side (oa * ob > 0) → UV fold, overlap confirmed, skip SAT.
        // oa or ob == 0 means a degenerate triangle (opposite vertex on the shared edge),
        // which has no interior and cannot produce positive-area overlap.
        if (oa * ob <= 0) return false;

        // Overlap confirmed (shared==2 fast path).  Compute area if requested.
        if (area_out) {
            Vec2<Scalar> tri_a[3], tri_b[3];
            for (int k = 0; k < 3; ++k) {
                tri_a[k] = load_vert(i, k);
                tri_b[k] = load_vert(j, k);
            }
            *area_out = triangle_intersection_area_2d(tri_a, tri_b, pred);
        }
        return true;
    } else {
        Vec2<Scalar> tri_a[3], tri_b[3];
        for (int k = 0; k < 3; ++k) {
            tri_a[k] = load_vert(i, k);
            tri_b[k] = load_vert(j, k);
        }
        if (!triangles_have_interior_overlap(tri_a, tri_b, pred)) return false;

        // Overlap confirmed.  Compute area if requested (reuse already-loaded vertices).
        if (area_out) {
            *area_out = triangle_intersection_area_2d(tri_a, tri_b, pred);
        }
        return true;
    }
}

// ---------------------------------------------------------------------------
// Candidate pair finders
// ---------------------------------------------------------------------------

///
/// Zomorodian-Edelsbrunner OneWayScan for 2-D axis-aligned boxes (complete case).
///
/// Sort boxes by x low endpoint.  For each box i, scan forward through boxes j
/// (in sorted order, so x.lo[j] >= x.lo[i]) while x.lo[j] <= x.hi[i].  By
/// Property 2 of the ZE paper, two intervals intersect iff one contains the low
/// endpoint of the other; since j comes after i in sorted order, the only possible
/// containment is x.lo[j] ∈ [x.lo[i], x.hi[i]], so this one-directional scan
/// reports every intersecting x-interval pair exactly once.  Then verify y-interval
/// overlap before emitting the candidate pair.
///
/// Total work is O(n log n + k) where k = number of candidate pairs.
/// The outer loop is run in parallel via TBB.
///
/// Reference: "Fast Software for Box Intersections", A. Zomorodian & H. Edelsbrunner,
/// Int. J. Computational Geometry & Applications 12(1-2), 2002.
///
template <typename Scalar, typename Index>
std::vector<std::pair<Index, Index>> sweep_and_prune_candidates(
    const std::vector<Eigen::AlignedBox<Scalar, 2>>& boxes)
{
    const Index n = static_cast<Index>(boxes.size());
    if (n < 2) return {};

    // Sort box indices by x low endpoint.
    std::vector<Index> order(n);
    std::iota(order.begin(), order.end(), Index(0));
    tbb::parallel_sort(order.begin(), order.end(), [&](Index a, Index b) {
        return boxes[a].min().x() < boxes[b].min().x();
    });

    // Parallel outer loop: each ii is independent (read-only access to order/boxes).
    using PairVec = std::vector<std::pair<Index, Index>>;
    tbb::enumerable_thread_specific<PairVec> local_pairs;

    tbb::parallel_for(Index(0), n, [&](Index ii) {
        const Index i = order[ii];
        const auto& bi = boxes[i];
        auto& lp = local_pairs.local();

        // Scan forward: j comes after i in x.lo order.
        // Stop as soon as x.lo[j] >= x.hi[i] (no further j can strictly overlap i in x).
        for (Index jj = ii + 1; jj < n; ++jj) {
            const Index j = order[jj];
            const auto& bj = boxes[j];
            if (bj.min().x() >= bi.max().x()) break;
            // Strict overlap in both axes.
            if (bj.max().x() > bi.min().x() && bj.max().y() > bi.min().y() &&
                bj.min().y() < bi.max().y()) {
                lp.push_back(std::minmax(i, j));
            }
        }
    });

    return flatten_thread_local(local_pairs);
}

///
/// Zomorodian-Edelsbrunner HYBRID algorithm for 2-D axis-aligned boxes (complete case).
///
/// Reference paper: "Fast Software for Box Intersections",
///   A. Zomorodian & H. Edelsbrunner, Int. J. Comp. Geom. & Appl. 12(1-2), 2002.
///   https://pub.ista.ac.at/~edels/Papers/2002-01-FastBoxIntersection.pdf
///
/// The paper's Figure 5 defines HYBRID(I, P, lo, hi, d) as a streamed segment tree that
/// maintains two separate sets — I (intervals) and P (points) — which can diverge through
/// recursion even for the complete case (I = P = S initially).  At each node it:
///   (4) extracts I_m = intervals spanning the segment [lo, hi), processes I_m × P
///       and P × I_m at d−1;
///   (5-7) splits P by a median mi, partitions I − I_m by intersection with [lo, mi)
///       and [mi, hi), and recurses on both children at d.  Note that I_l and I_r are
///       NOT disjoint: a box straddling mi (but not spanning [lo, hi)) appears in both.
///
/// This implementation is a simplification for the 2-D complete case (d ∈ {0, 1}) that
/// differs from the paper in several ways:
///
///   1. Single-set recursion.  Instead of tracking I and P separately, we maintain one
///      set S and always perform self-intersection.  This avoids the bookkeeping of two
///      diverging pointer arrays at the cost of generality (bipartite / 3-D would need
///      the paper's two-set structure).
///
///   2. Eager spanning extraction.  Where the paper gradually identifies spanning
///      intervals as [lo, hi) narrows (I_m is often ∅ at the root), and splits the
///      remaining intervals into overlapping I_l / I_r subsets, we eagerly partition
///      S into three disjoint subsets at every level:
///        S_m (spanning): y_lo ≤ mi  ∧  y_hi > mi   (all pairwise overlap in y)
///        S_l (below):    y_lo < mi  ∧  y_hi ≤ mi
///        S_r (above):    y_lo ≥ mi  ∧  ¬spanning
///      No explicit [lo, hi) segment bounds are tracked.  S_l and S_r are
///      y-separated (no pair can overlap in y), so S_l × S_r is skipped entirely.
///
///   3. Decomposition of S_m pairs.  The paper handles I_m × P recursively at d−1
///      (covering both self-pairs and cross-pairs in one call).  We split this into:
///        • S_m × S_m         self-pairs  → hybrid(s_m, d−1)
///        • S_m × (S_l ∪ S_r)            → flat bipartite OneWayScan
///      For 2-D these are equivalent because d−1 = 0 immediately reaches the scan base
///      case; a 3-D extension would need the paper's recursive approach.
///
///   4. Imbalanced partition fallback.  When max(|S_l|, |S_r|) > ¾ |S_l ∪ S_r|, the
///      recursion is replaced by a single flat OneWayScan on S_l ∪ S_r.  The paper
///      always recurses on both children.
///
///   d=0 (x dimension): base case — OneWayScan on x.
///   d=1 (y dimension): partition by median on y, recurse on left/right/spanning,
///                      bipartite scan for spanning × non-spanning pairs.
///
template <typename Scalar, typename Index>
class HybridSolver
{
    using Box2 = Eigen::AlignedBox<Scalar, 2>;
    using PairVec = std::vector<std::pair<Index, Index>>;

    // Scan cutoff: switch to flat OneWayScan when a subset has ≤ this many boxes.
    static constexpr Index k_cutoff = 512;

    const std::vector<Box2>& m_boxes;
    tbb::enumerable_thread_specific<PairVec>& m_tls_results;

public:
    HybridSolver(
        const std::vector<Box2>& boxes,
        tbb::enumerable_thread_specific<PairVec>& tls_results)
        : m_boxes(boxes)
        , m_tls_results(tls_results)
    {}

    // OneWayScan in x (complete case: S sorted by x.lo, emit each pair once).
    void one_way_scan(span<const Index> S)
    {
        const Index n = static_cast<Index>(S.size());
        tbb::parallel_for(Index(0), n, [&](Index ii) {
            auto& lp = m_tls_results.local();
            const Index i = S[ii];
            const auto& bi = m_boxes[i];

            for (Index jj = ii + 1; jj < n; ++jj) {
                const Index j = S[jj];
                const auto& bj = m_boxes[j];
                if (bj.min().x() >= bi.max().x()) break;
                if (bj.max().x() > bi.min().x() && bj.max().y() > bi.min().y() &&
                    bj.min().y() < bi.max().y()) {
                    lp.push_back(std::minmax(i, j));
                }
            }
        });
    }

    // OneWayScan in x (bipartite case: I and P are disjoint sets, both sorted by x.lo).
    //
    // The caller invokes this twice with swapped arguments to cover both
    // containment directions.  To avoid duplicate reports when x.lo[i] == x.lo[j],
    // the dual call should use strict=true.
    void one_way_scan_bipartite(span<const Index> I, span<const Index> P, bool strict = false)
    {
        const Index nI = static_cast<Index>(I.size());
        const Index nP = static_cast<Index>(P.size());
        if (nI == 0 || nP == 0) return;

        auto scan_one = [&](Index ii, PairVec& lp) {
            const Index i = I[ii];
            const auto& bi = m_boxes[i];
            const Scalar xlo_i = bi.min().x();

            Index start;
            if (strict) {
                start = static_cast<Index>(
                    std::upper_bound(
                        P.begin(),
                        P.end(),
                        xlo_i,
                        [&](Scalar v, Index jj) { return v < m_boxes[jj].min().x(); }) -
                    P.begin());
            } else {
                start = static_cast<Index>(
                    std::lower_bound(
                        P.begin(),
                        P.end(),
                        xlo_i,
                        [&](Index jj, Scalar v) { return m_boxes[jj].min().x() < v; }) -
                    P.begin());
            }

            for (Index jj = start; jj < nP; ++jj) {
                const Index j = P[jj];
                const auto& bj = m_boxes[j];
                if (bj.min().x() >= bi.max().x()) break;
                if (bj.max().x() > bi.min().x() && bj.max().y() > bi.min().y() &&
                    bj.min().y() < bi.max().y()) {
                    lp.push_back(std::minmax(i, j));
                }
            }
        };

        tbb::parallel_for(Index(0), nI, [&](Index ii) { scan_one(ii, m_tls_results.local()); });
    }

    // HYBRID recursive procedure (complete case only: self-intersection of a single set).
    //
    // Corresponds to HYBRID(I, P, lo, hi, d) in Figure 5 of the ZE paper, specialized
    // for the complete case (I = P = S) in 2-D.  See the class-level comment above for
    // a detailed description of the differences from the paper.
    //
    // Operates in-place on the mutable span S.  The input buffer is partitioned
    // in-place at each level into [S_l | S_r | S_m] so that S_l ∪ S_r is contiguous
    // and S_m is at the end.
    void hybrid(span<Index> S, int d)
    {
        const Index n = static_cast<Index>(S.size());
        if (n < 2) return;

        // Paper steps 2-3: base cases (d = 0 → scan, small set → scan).
        if (d == 0 || n <= k_cutoff) {
            sort_by_xlo(S);
            one_way_scan(S);
            return;
        }

        // Paper step 5: compute median of y_lo values.
        const Scalar mi = median_ylo(S);

        // Paper steps 4, 6-7 combined: three-way in-place partition [S_l | S_r | S_m].
        // Unlike the paper (which uses non-disjoint I_l / I_r and a separate I_m that
        // spans [lo, hi)), we eagerly extract S_m as boxes spanning the median point:
        //   S_m (spanning): ylo <= mi && yhi > mi  (all pairwise overlap in y)
        //   S_l (below):    ylo <= mi && yhi <= mi
        //   S_r (above):    ylo > mi  && yhi > mi  (S_l × S_r never overlap in y)
        //
        // First pass: separate non-spanning from spanning → [non-spanning | S_m]
        Index* mid_span = std::partition(S.data(), S.data() + S.size(), [&](Index i) {
            return !(m_boxes[i].min().y() <= mi && m_boxes[i].max().y() > mi);
        });
        // Second pass within non-spanning: separate S_l from S_r → [S_l | S_r | S_m]
        Index* mid_lr =
            std::partition(S.data(), mid_span, [&](Index i) { return m_boxes[i].min().y() <= mi; });

        const Index n_l = static_cast<Index>(mid_lr - S.data());
        const Index n_r = static_cast<Index>(mid_span - mid_lr);
        const Index n_m = static_cast<Index>(S.data() + S.size() - mid_span);

        span<Index> s_l(S.data(), n_l);
        span<Index> s_r(mid_lr, n_r);
        span<Index> s_m(mid_span, n_m);
        span<Index> s_rest(S.data(), n_l + n_r); // S_l ∪ S_r is contiguous

        // Paper step 4 (partial): S_m self-pairs at d-1.
        // Paper steps 6-7: recurse on left and right halves (y-separated, so S_l × S_r
        // pairs never overlap in y and can be skipped).  Must happen BEFORE the bipartite
        // scan because sorting s_rest by x.lo would mix S_l and S_r elements, destroying
        // the partition boundary.
        //
        // When the partition is very lopsided (one branch > 75%), fall through to a flat
        // scan on all of S_rest instead of recursing on nearly-empty + nearly-full halves.
        const Index max_branch = std::max(n_l, n_r);
        if (max_branch > static_cast<Index>(s_rest.size()) * 3 / 4) {
            tbb::parallel_invoke(
                [&] {
                    if (n_m >= 2) hybrid(s_m, d - 1);
                },
                [&] {
                    sort_by_xlo(s_rest);
                    one_way_scan(s_rest);
                });
        } else {
            tbb::parallel_invoke(
                [&] {
                    if (n_m >= 2) hybrid(s_m, d - 1);
                },
                [&] { hybrid(s_l, d); },
                [&] { hybrid(s_r, d); });
        }

        // Paper step 4 (remaining): S_m × (S_l ∪ S_r) cross-pairs via bipartite scan.
        // Both scan directions are needed to find all x-overlapping pairs.
        if (n_m > 0 && !s_rest.empty()) {
            sort_by_xlo(s_m);
            sort_by_xlo(s_rest);
            tbb::parallel_invoke(
                [&] { one_way_scan_bipartite(s_m, s_rest); },
                [&] { one_way_scan_bipartite(s_rest, s_m, true); });
        }
    }

private:
    void sort_by_xlo(span<Index> v)
    {
        tbb::parallel_sort(v.begin(), v.end(), [&](Index a, Index b) {
            return m_boxes[a].min().x() < m_boxes[b].min().x();
        });
    }

    Scalar median_ylo(span<Index> S)
    {
        auto* mid = S.data() + S.size() / 2;
        std::nth_element(S.data(), mid, S.data() + S.size(), [&](Index a, Index b) {
            return m_boxes[a].min().y() < m_boxes[b].min().y();
        });
        return m_boxes[*mid].min().y();
    }
};

template <typename Scalar, typename Index>
std::vector<std::pair<Index, Index>> hybrid_candidates(
    const std::vector<Eigen::AlignedBox<Scalar, 2>>& boxes)
{
    using PairVec = std::vector<std::pair<Index, Index>>;
    const Index n = static_cast<Index>(boxes.size());
    if (n < 2) return {};

    // Build initial index array sorted by y.lo (required by HYBRID entry point at d=1).
    std::vector<Index> order(n);
    std::iota(order.begin(), order.end(), Index(0));
    tbb::parallel_sort(order.begin(), order.end(), [&](Index a, Index b) {
        return boxes[a].min().y() < boxes[b].min().y();
    });

    tbb::enumerable_thread_specific<PairVec> tls_results;
    HybridSolver<Scalar, Index> solver(boxes, tls_results);
    solver.hybrid(span<Index>(order), 1);

    return flatten_thread_local(tls_results);
}

///
/// BVH-based candidate pair detection for benchmarking.
///
/// Builds an AABB<Scalar,2> tree on all triangle bounding boxes, then performs a
/// per-triangle box query in parallel.  Emits each pair (i,j) with i < j exactly once.
///
template <typename Scalar, typename Index>
std::vector<std::pair<Index, Index>> bvh_candidates(std::vector<Eigen::AlignedBox<Scalar, 2>> boxes)
{
    const Index n = static_cast<Index>(boxes.size());
    if (n < 2) return {};

    la_runtime_assert(
        n <= static_cast<Index>(std::numeric_limits<uint32_t>::max()),
        "BVH method: number of facets exceeds uint32_t range");

    // Build AABB tree (its Index type is always uint32_t).
    AABB<Scalar, 2> tree;
    tree.build(boxes);

    // Per-thread pair accumulator.
    using PairVec = std::vector<std::pair<Index, Index>>;
    tbb::enumerable_thread_specific<PairVec> local_pairs;

    tbb::parallel_for(Index(0), n, [&](Index i) {
        auto& lp = local_pairs.local();
        const auto& bi = boxes[i];
        tree.intersect(bi, [&](uint32_t j_u32) -> bool {
            const Index j = static_cast<Index>(j_u32);
            if (j <= i) return true; // emit each (i,j) with i < j only once; skip self
            // Strict interior overlap: the AABB tree uses non-strict intersection
            // for internal node traversal, so we filter at the leaf level.
            const auto& bj = boxes[j];
            if (bj.max().x() <= bi.min().x() || bj.min().x() >= bi.max().x()) return true;
            if (bj.max().y() <= bi.min().y() || bj.min().y() >= bi.max().y()) return true;
            lp.push_back({i, j});
            return true;
        });
    });

    return flatten_thread_local(local_pairs);
}

// ---------------------------------------------------------------------------
// Chart-aware graph coloring
// ---------------------------------------------------------------------------

///
/// Chart-aware graph coloring of the overlap graph.
///
/// Color 0 is reserved for non-overlapping triangles.  Overlapping triangles
/// receive colors 1, 2, ... such that no two overlapping triangles share the
/// same color.
///
/// Unlike a simple greedy coloring that processes facets in index order, this
/// performs a BFS traversal through each UV chart (via @p uv_adj).  When
/// visiting a facet, it tries to reuse the BFS parent's color (if not
/// forbidden by an overlap conflict).  This produces spatially coherent color
/// regions, minimizing the number of connected components per (chart, color)
/// pair when the coloring is later used to split charts for repacking.
///
template <typename Index>
std::vector<Index> chart_aware_graph_color(
    Index num_facets,
    const std::vector<std::pair<Index, Index>>& overlap_edges,
    const AdjacencyList<Index>& uv_adj)
{
    // Build overlap adjacency list.
    std::vector<std::vector<Index>> overlap_adj(num_facets);
    for (auto& [i, j] : overlap_edges) {
        overlap_adj[i].push_back(j);
        overlap_adj[j].push_back(i);
    }

    std::vector<Index> color(num_facets, Index(0)); // 0 = not involved in any overlap
    std::vector<bool> visited(num_facets, false);
    std::vector<bool> forbidden;
    std::deque<std::pair<Index, Index>> queue; // (facet, parent_facet)

    for (Index start = 0; start < num_facets; ++start) {
        if (visited[start]) continue;

        // BFS through one UV chart.
        queue.clear();
        queue.push_back({start, start});
        visited[start] = true;

        while (!queue.empty()) {
            auto [v, parent] = queue.front();
            queue.pop_front();

            if (!overlap_adj[v].empty()) {
                // Collect forbidden colors from overlap neighbors.
                forbidden.clear();
                for (Index nb : overlap_adj[v]) {
                    if (color[nb] > 0) {
                        const size_t c = static_cast<size_t>(color[nb]) - 1;
                        if (c >= forbidden.size()) forbidden.resize(c + 1, false);
                        forbidden[c] = true;
                    }
                }

                // Try to reuse the BFS parent's color first.
                Index best = 0;
                if (color[parent] > 0) {
                    const size_t pc = static_cast<size_t>(color[parent]) - 1;
                    if (pc >= forbidden.size() || !forbidden[pc]) {
                        best = color[parent];
                    }
                }

                // If parent's color is unavailable, try other UV neighbors' colors.
                if (best == 0) {
                    for (Index nb : uv_adj.get_neighbors(v)) {
                        if (color[nb] > 0) {
                            const size_t nc = static_cast<size_t>(color[nb]) - 1;
                            if (nc >= forbidden.size() || !forbidden[nc]) {
                                best = color[nb];
                                break;
                            }
                        }
                    }
                }

                // Fallback: smallest available color >= 1.
                if (best == 0) {
                    size_t c = 0;
                    while (c < forbidden.size() && forbidden[c]) ++c;
                    best = static_cast<Index>(c + 1);
                }

                color[v] = best;
            }

            // Enqueue UV-adjacent unvisited facets.
            for (Index nb : uv_adj.get_neighbors(v)) {
                if (!visited[nb]) {
                    visited[nb] = true;
                    queue.push_back({nb, v});
                }
            }
        }
    }

    return color;
}

} // namespace

// ---------------------------------------------------------------------------
// compute_uv_overlap — main entry point
// ---------------------------------------------------------------------------

template <typename Scalar, typename Index>
UVOverlapResult<Scalar, Index> compute_uv_overlap(
    SurfaceMesh<Scalar, Index>& mesh,
    const UVOverlapOptions& options)
{
    la_runtime_assert(mesh.is_triangle_mesh(), "compute_uv_overlap: mesh must be triangulated.");

    // ----- Phase 1: extract UV mesh -----
    UVMeshOptions uv_opts;
    uv_opts.uv_attribute_name = options.uv_attribute_name;
    uv_opts.element_types = UVMeshOptions::ElementTypes::All;
    // uv_mesh_view creates a 2-D mesh whose vertex positions are UV coordinates.
    auto uv_mesh = uv_mesh_view(mesh, uv_opts);

    const Index num_facets = uv_mesh.get_num_facets();
    if (num_facets == 0) return {};

    const auto uv_verts = vertex_view(uv_mesh); // (num_uv_verts × 2)
    const auto uv_facets = facet_view(uv_mesh); // (num_facets × 3)

    // ----- Phase 2: compute per-triangle 2-D bounding boxes -----
    using Box2 = Eigen::AlignedBox<Scalar, 2>;
    std::vector<Box2> boxes(num_facets);
    for (Index f = 0; f < num_facets; ++f) {
        boxes[f].setEmpty();
        for (int k = 0; k < 3; ++k) {
            const Index vid = uv_facets(f, k);
            boxes[f].extend(Vec2<Scalar>(uv_verts(vid, 0), uv_verts(vid, 1)));
        }
    }

    // ----- Phase 3: candidate pair detection -----
    std::vector<std::pair<Index, Index>> candidates;
    switch (options.method) {
    case UVOverlapMethod::SweepAndPrune:
        candidates = sweep_and_prune_candidates<Scalar, Index>(boxes);
        break;
    case UVOverlapMethod::BVH: candidates = bvh_candidates<Scalar, Index>(std::move(boxes)); break;
    case UVOverlapMethod::Hybrid: candidates = hybrid_candidates<Scalar, Index>(boxes); break;
    }

    if (candidates.empty()) return {};

    // ----- Phase 4: exact intersection test + optional area (parallel) -----
    ExactPredicatesShewchuk pred;
    const bool compute_area = options.compute_overlap_area;
    const bool collect_pairs =
        options.compute_overlapping_pairs || options.compute_overlap_coloring;

    struct ThreadLocal
    {
        std::vector<std::pair<Index, Index>> pairs; // only used when collect_pairs
        size_t count = 0;
        Scalar area = 0.f;
    };
    tbb::enumerable_thread_specific<ThreadLocal> tls;

    tbb::parallel_for(
        tbb::blocked_range<size_t>(0, candidates.size()),
        [&](const tbb::blocked_range<size_t>& r) {
            auto& loc = tls.local();
            for (size_t p = r.begin(); p != r.end(); ++p) {
                const Index i = candidates[p].first;
                const Index j = candidates[p].second;
                Scalar area = 0;
                Scalar* area_ptr = compute_area ? &area : nullptr;
                if (process_candidate_pair(i, j, uv_verts, uv_facets, pred, area_ptr)) {
                    loc.count++;
                    loc.area += area;
                    if (collect_pairs) {
                        loc.pairs.push_back({i, j});
                    }
                }
            }
        });

    // ----- Phase 5: accumulate results -----
    size_t total_results = 0;
    Scalar total_area = Scalar(0);
    std::vector<std::pair<Index, Index>> overlap_edges;

    for (auto& loc : tls) {
        total_results += loc.count;
        if (compute_area) total_area += loc.area;
    }
    if (collect_pairs && total_results > 0) {
        overlap_edges.reserve(total_results);
        for (auto& loc : tls) {
            overlap_edges.insert(overlap_edges.end(), loc.pairs.begin(), loc.pairs.end());
        }
    }

    if (total_results == 0) return {};

    UVOverlapResult<Scalar, Index> result;
    result.has_overlap = true;
    if (compute_area) result.overlap_area = total_area;

    // ----- Phase 6: optional per-facet overlap coloring -----
    if (options.compute_overlap_coloring && !overlap_edges.empty()) {
        const auto uv_adj = compute_facet_facet_adjacency(uv_mesh);
        std::vector<Index> colors =
            chart_aware_graph_color<Index>(num_facets, overlap_edges, uv_adj);

        const AttributeId attr_id = internal::find_or_create_attribute<Index>(
            mesh,
            options.overlap_coloring_attribute_name,
            AttributeElement::Facet,
            AttributeUsage::Scalar,
            1,
            internal::ResetToDefault::No);

        auto& attr = mesh.template ref_attribute<Index>(attr_id);
        auto attr_data = attr.ref_all();
        la_debug_assert(static_cast<Index>(attr_data.size()) == num_facets);
        for (Index f = 0; f < num_facets; ++f) {
            attr_data[f] = colors[f];
        }

        result.overlap_coloring_id = attr_id;
    }

    if (options.compute_overlapping_pairs) {
        tbb::parallel_sort(overlap_edges.begin(), overlap_edges.end());
        result.overlapping_pairs = std::move(overlap_edges);
    }

    return result;
}

// Explicit template instantiations.
#define LA_X_compute_uv_overlap(_, Scalar, Index)                                         \
    template LA_BVH_API UVOverlapResult<Scalar, Index> compute_uv_overlap<Scalar, Index>( \
        SurfaceMesh<Scalar, Index>&,                                                      \
        const UVOverlapOptions&);
LA_SURFACE_MESH_X(compute_uv_overlap, 0)

} // namespace lagrange::bvh
