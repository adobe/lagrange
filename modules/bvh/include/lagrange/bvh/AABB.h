/*
 * Copyright 2025 Adobe. All rights reserved.
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

#include <lagrange/utils/function_ref.h>
#include <lagrange/utils/invalid.h>
#include <lagrange/utils/span.h>

#include <Eigen/Geometry>
#include <functional>
#include <vector>

namespace lagrange::bvh {

/// @addtogroup module-bvh
/// @{

///
/// Axis-Aligned Bounding Box (AABB) tree for efficient spatial queries.
///
/// This data structure organizes a collection of bounding boxes in a binary tree
/// to enable fast intersection queries. The tree is built using a top-down approach
/// by recursively splitting boxes along the longest Dimension of their centroids.
///
/// @tparam Scalar Numeric type for coordinates (e.g., float or double).
/// @tparam Dim  Spatial Dimension (2 or 3).
///
template <typename Scalar, int Dim>
class AABB
{
public:
    using Index = uint32_t;
    using Box = Eigen::AlignedBox<Scalar, Dim>;
    using Point = typename Box::VectorType;

public:
    ///
    /// Build the AABB tree from a collection of bounding boxes.
    ///
    /// @param[in] boxes  Input bounding boxes to organize in the tree.
    ///
    void build(span<Box> boxes);

    ///
    /// Find all boxes that intersect with a query box.
    ///
    /// @param[in]  query_box  The query bounding box.
    /// @param[out] results    Indices of boxes that intersect with the query box.
    ///
    void intersect(const Box& query_box, std::vector<Index>& results) const;

    ///
    /// Find all boxes that intersect with a query box and call a function for each.
    ///
    /// @param[in] query_box  The query bounding box.
    /// @param[in] callback   Function to call for each intersecting box index.
    ///                       The callback function takes an element  ID as input and returns a bool
    ///                       indicating whether to continue the search (true) or terminate early
    ///                       (false).
    ///
    void intersect(const Box& query_box, function_ref<bool(Index)> callback) const;

    ///
    /// Find the first box that intersects with a query box.
    ///
    /// @param[in] query_box  The query bounding box.
    ///
    /// @return Element ID of the first intersecting box, or invalid<Index>() if none found.
    ///
    Index intersect_first(const Box& query_box) const;

    ///
    /// Find the index of the closest element to a query point.
    ///
    /// @param[in]  q           The query point.
    /// @param[in]  sq_dist_fn  Squared distance function for point-element distance.
    ///
    /// @return     Index of the closest element, or invalid<Index>() if tree is empty.
    ///
    Index get_closest_element(const Point& q, function_ref<Scalar(Index)> sq_dist_fn) const;

    ///
    /// Call a function for each element within a given radius from a query point.
    ///
    /// @param[in] q          The query point.
    /// @param[in] sq_radius  The search radius squared.
    /// @param[in] func       Function to call for each element whose bounding box is within the radius.
    ///
    /// @remark This method checks bounding boxes, not exact geometry.
    ///
    void foreach_element_within_radius(
        const Point& q,
        Scalar sq_radius,
        function_ref<void(Index)> func) const;

    ///
    /// Check if the tree is empty.
    ///
    /// @return True if the tree is empty, false otherwise.
    ///
    bool empty() const { return m_root == invalid<Index>(); }

private:
    struct Node
    {
        Box bbox;
        Index left = invalid<Index>();
        Index right = invalid<Index>();
        Index element_idx = invalid<Index>();

        bool is_leaf() const { return left == invalid<Index>() && right == invalid<Index>(); }
    };

private:
    std::vector<Node> m_nodes;
    Index m_root = invalid<Index>();
};

/// @}

} // namespace lagrange::bvh
