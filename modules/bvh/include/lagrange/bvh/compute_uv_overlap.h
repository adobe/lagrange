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
#pragma once

#include <lagrange/AttributeFwd.h>
#include <lagrange/SurfaceMesh.h>
#include <lagrange/bvh/api.h>

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace lagrange::bvh {

/// @addtogroup module-bvh
/// @{

///
/// Algorithm used to find candidate bounding-box pairs in compute_uv_overlap.
///
enum class UVOverlapMethod {
    ///
    /// Zomorodian-Edelsbrunner sweep-and-prune.
    ///
    /// Sorts triangle bounding-box x-intervals and sweeps a 1-D active set to enumerate
    /// all overlapping pairs in O((n + k) log n) time, where k is the output size.
    ///
    SweepAndPrune,

    ///
    /// AABB tree per-triangle query.
    ///
    /// Builds an @ref AABB<Scalar,2> tree on all triangle bounding boxes, then queries
    /// each box against the tree in parallel.  Useful for benchmarking and cross-validation
    /// against the sweep-and-prune path.
    ///
    BVH,

    ///
    /// Zomorodian-Edelsbrunner HYBRID algorithm (recursive divide-and-conquer).
    ///
    /// Based on the HYBRID procedure from Figure 5 of "Fast Software for Box
    /// Intersections" (Zomorodian & Edelsbrunner, 2002), simplified for the 2-D
    /// complete case.  Recursively splits on the y-dimension median, handles spanning
    /// intervals at each node with a one-dimensional OneWayScan, and falls through to
    /// scanning for subsets below a cutoff size.  See the implementation file for a
    /// detailed description of the differences from the paper's algorithm.
    ///
    Hybrid,
};

///
/// Options for compute_uv_overlap.
///
struct UVOverlapOptions
{
    /// UV attribute name. Empty string = use the first compatible UV attribute found on the mesh.
    /// Vertex, indexed, and corner attributes are all supported.
    std::string uv_attribute_name;

    /// If true (default), compute the total overlap area via Sutherland-Hodgman clipping.
    /// Set to false when only overlap detection or coloring is needed (faster).
    bool compute_overlap_area = true;

    ///
    /// If true, compute a per-facet integer attribute that assigns each triangle a color
    /// (greedy graph coloring of the overlap graph). Color 0 = not overlapping with any
    /// other triangle; colors >= 1 are assigned so that no two triangles with the same
    /// color have a positive-area intersection. This allows splitting UV charts into
    /// non-overlapping layers before repacking.
    ///
    bool compute_overlap_coloring = false;

    /// Name of the per-facet output integer attribute written when compute_overlap_coloring
    /// is true.
    std::string overlap_coloring_attribute_name = "@uv_overlap_color";

    ///
    /// If true, populate UVOverlapResult::overlapping_pairs with the sorted list of
    /// (i, j) facet-index pairs that have a positive-area interior intersection.
    ///
    bool compute_overlapping_pairs = false;

    /// Candidate pair detection algorithm.
    UVOverlapMethod method = UVOverlapMethod::Hybrid;
};

///
/// Result of compute_uv_overlap.
///
/// @tparam Scalar  Mesh scalar type.
/// @tparam Index   Mesh index type.
///
template <typename Scalar, typename Index>
struct UVOverlapResult
{
    /// True when at least one pair of UV triangles has a positive-area interior intersection.
    /// This is the canonical overlap indicator and is valid regardless of compute_overlap_area.
    bool has_overlap = false;

    ///
    /// Sum of intersection areas over all overlapping triangle pairs.  std::nullopt when
    /// UVOverlapOptions::compute_overlap_area is false or when has_overlap is false.
    /// Check has_overlap (not this field) to test for overlap existence.
    ///
    std::optional<Scalar> overlap_area;

    ///
    /// Sorted list of (i, j) facet-index pairs (i < j) that have a positive-area interior
    /// intersection.  Empty when UVOverlapOptions::compute_overlapping_pairs is false or
    /// when there are no overlapping triangles.
    ///
    std::vector<std::pair<Index, Index>> overlapping_pairs;

    ///
    /// AttributeId of the per-facet integer coloring attribute written to the mesh.
    /// Equals invalid_attribute_id() when UVOverlapOptions::compute_overlap_coloring is
    /// false or when there are no overlapping triangles.
    ///
    AttributeId overlap_coloring_id = invalid_attribute_id();
};

///
/// Compute pairwise UV triangle overlap.
///
/// For every pair of UV-space triangles whose 2-D axis-aligned bounding boxes intersect
/// (detected via the Zomorodian-Edelsbrunner sweep-and-prune or a BVH), an exact
/// separating-axis test using orient2D predicates is applied to confirm a genuine
/// interior intersection before computing the intersection area using
/// Sutherland-Hodgman polygon clipping.
///
/// Triangles that share only a boundary edge or a single vertex are never counted as
/// overlapping — the exact orient2D predicate handles boundary contacts correctly.
///
/// @param[in,out] mesh     Input triangle mesh with a UV attribute.
/// @param[in]     options  Options controlling algorithm and output.
///
/// @return @ref UVOverlapResult containing the optional total overlap area and the
///         optional AttributeId of the per-facet coloring attribute.
///
/// @tparam Scalar  Mesh scalar type.
/// @tparam Index   Mesh index type.
///
template <typename Scalar, typename Index>
LA_BVH_API UVOverlapResult<Scalar, Index> compute_uv_overlap(
    SurfaceMesh<Scalar, Index>& mesh,
    const UVOverlapOptions& options = {});

/// @}

} // namespace lagrange::bvh
