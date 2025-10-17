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

#include <lagrange/bvh/AABB.h>
#include <lagrange/common.h>
#include <lagrange/utils/function_ref.h>

#include <Eigen/Core>
#include <Eigen/Geometry>

namespace lagrange {
namespace bvh {

/// @addtogroup module-bvh
/// @{

// AABB tree for an edge graph
template <typename VertexArray, typename EdgeArray, int Dim = 3>
struct EdgeAABBTree
{
    using Scalar = typename VertexArray::Scalar;
    using Index = typename EdgeArray::Scalar;
    using AlignedBoxType = typename AABB<Scalar, Dim>::Box;
    using RowVectorType = Eigen::Matrix<Scalar, 1, Dim>;

private:
    const VertexArray* m_vertices = nullptr;
    const EdgeArray* m_edges = nullptr;

    // Use AABB for spatial indexing
    AABB<Scalar, Dim> m_aabb;

public:
    /// closest_sq_dist x element_id x closest_point
    using ActionCallback = function_ref<void(Scalar, Index, const RowVectorType&)>;

    EdgeAABBTree() = default; // Default empty constructor

    ///
    /// Construct an AABB over the given edge graph.
    ///
    /// @param[in]  V     #V x Dim input vertex positions.
    /// @param[in]  E     #E x 2 input edge vertices.
    ///
    EdgeAABBTree(const VertexArray& V, const EdgeArray& E);

    ///
    /// Test whether the tree is empty
    ///
    /// @return     True iff empty, False otherwise.
    ///
    bool empty() const { return m_aabb.empty(); }

    ///
    /// Gets the closest point to a given element.
    ///
    /// @param[in]  p                Query point.
    /// @param[in]  element_id       Element id.
    /// @param[out] closest_point    Closest point on the element.
    /// @param[out] closest_sq_dist  Squared distance between closest point and query point.
    ///
    void get_element_closest_point(
        const RowVectorType& p,
        Index element_id,
        RowVectorType& closest_point,
        Scalar& closest_sq_dist) const;

    ///
    /// Iterate over edges within a prescribed distance from a query point.
    ///
    /// @param[in]  p        1 x Dim query point.
    /// @param[in]  sq_dist  Squared query radius.
    /// @param[in]  func     Function to apply to every edge within query distance.
    ///
    void foreach_element_in_radius(const RowVectorType& p, Scalar sq_dist, ActionCallback func)
        const;

    ///
    /// Iterate over edges that contain exactly a given query point. This function uses exact
    /// predicates to determine whether the query point belong to an edge segment. Note that this is
    /// slightly different from calling foreach_element_in_radius with a search radius of 0, since
    /// foreach_element_in_radius does not use exact predicates, it might return false positives
    /// (i.e. points which are at distance 0, but not exactly collinear).
    ///
    /// @param[in]  p     1 x Dim query point.
    /// @param[in]  func  Function to apply to every edge within query distance.
    ///
    void foreach_element_containing(const RowVectorType& p, ActionCallback func) const;

    ///
    /// Gets the closest point to an element of the tree. Whereas get_element_closest_point returns
    /// the closest point to *a* queried element, this function recursively traverses the tree nodes
    /// to find the element which is closest to the query point.
    ///
    /// @param[in]  p                Query point.
    /// @param[out] element_id       Closest element id.
    /// @param[out] closest_point    Closest point on the element.
    /// @param[out] closest_sq_dist  Squared distance between closest point and query point.
    /// @param[in]  filter_func      Optional function to filter out elements from the test. Only
    ///                              elements for which filter_func(element_id) == true will be
    ///                              considered for closest point.
    ///
    void get_closest_point(
        const RowVectorType& p,
        Index& element_id,
        RowVectorType& closest_point,
        Scalar& closest_sq_dist,
        function_ref<bool(Index)> filter_func = [](Index) { return true; }) const;
};

/// @

} // namespace bvh
} // namespace lagrange

// Templated definitions
#include <lagrange/bvh/EdgeAABBTree.impl.h>
