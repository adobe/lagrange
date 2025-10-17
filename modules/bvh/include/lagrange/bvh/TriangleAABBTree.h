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

#include <lagrange/SurfaceMesh.h>
#include <lagrange/bvh/AABB.h>
#include <lagrange/utils/function_ref.h>

#include <Eigen/Core>
#include <Eigen/Geometry>

namespace lagrange::bvh {

/// @addtogroup module-bvh
/// @{

///
/// AABB tree for a triangle mesh.
///
/// This data structure organizes triangles in a spatial hierarchy to enable
/// efficient geometric queries such as closest point, ray intersection, and
/// spatial containment tests.
///
/// @tparam Scalar  Scalar type for coordinates.
/// @tparam Index   Index type for triangle indices.
/// @tparam Dim     Spatial Dimension (typically 3).
///
template <typename Scalar, typename Index, int Dim = 3>
class TriangleAABBTree
{
public:
    using AlignedBoxType = typename AABB<Scalar, Dim>::Box;
    using RowVectorType = Eigen::Matrix<Scalar, 1, Dim>;

public:
    /// Callback signature: (squared_distance, triangle_id, closest_point)
    using ActionCallback = function_ref<void(Scalar, Index, const RowVectorType&)>;

    ///
    /// Construct an AABB tree over the given triangle mesh.
    ///
    /// @param[in]  mesh  Input surface mesh.
    ///
    TriangleAABBTree(const SurfaceMesh<Scalar, Index>& mesh);

    ///
    /// Test whether the tree is empty.
    ///
    /// @return     True iff empty, False otherwise.
    ///
    bool empty() const { return m_aabb.empty(); }

    ///
    /// Iterate over triangles within a prescribed distance from a query point.
    ///
    /// @param[in]  p        1 x Dim query point.
    /// @param[in]  sq_dist  Squared query radius.
    /// @param[in]  func     Function to apply to every triangle within query distance.
    ///
    void foreach_triangle_in_radius(const RowVectorType& p, Scalar sq_dist, ActionCallback func)
        const;

    ///
    /// Gets the nearest triangle to a query point using automatic hint optimization.
    /// This is an optimized version that automatically finds a good starting triangle
    /// based on bounding box centers to potentially reduce search time.
    ///
    /// @param[in]  p                Query point.
    /// @param[out] triangle_id      Nearest triangle id.
    /// @param[out] closest_point    Closest point on the triangle.
    /// @param[out] closest_sq_dist  Squared distance between closest point and query point.
    ///
    /// @return     True if a triangle was found, false otherwise.
    ///
    bool get_closest_point(
        const RowVectorType& p,
        Index& triangle_id,
        RowVectorType& closest_point,
        Scalar& closest_sq_dist) const;

private:
    SurfaceMesh<Scalar, Index> m_mesh;

    // Use AABB for spatial indexing
    AABB<Scalar, Dim> m_aabb;
};

/// @}

} // namespace lagrange::bvh
