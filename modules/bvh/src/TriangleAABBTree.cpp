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
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/bvh/TriangleAABBTree.h>
#include <lagrange/bvh/api.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/invalid.h>
#include <lagrange/utils/point_triangle_squared_distance.h>
#include <lagrange/views.h>

#include <algorithm>
#include <limits>

namespace lagrange::bvh {


template <typename Scalar, typename Index, int Dim>
TriangleAABBTree<Scalar, Index, Dim>::TriangleAABBTree(const SurfaceMesh<Scalar, Index>& mesh)
    : m_mesh(mesh)
{
    la_runtime_assert(Dim == mesh.get_dimension(), "Dimension mismatch in TriangleAABBTree!");
    la_runtime_assert(mesh.is_triangle_mesh(), "Mesh must be triangular!");

    auto vertices = vertex_view(mesh);

    // Compute bounding boxes for all triangles
    const auto num_facets = mesh.get_num_facets();
    std::vector<typename AABB<Scalar, Dim>::Box> triangle_boxes;
    triangle_boxes.resize(num_facets);

    for (Index i = 0; i < num_facets; ++i) {
        auto facet = mesh.get_facet_vertices(i);
        auto& bbox = triangle_boxes[i];
        bbox.setEmpty();
        bbox.extend(vertices.row(facet[0]).transpose());
        bbox.extend(vertices.row(facet[1]).transpose());
        bbox.extend(vertices.row(facet[2]).transpose());
    }

    // Build the AABB tree
    m_aabb.build(
        span<typename AABB<Scalar, Dim>::Box>(triangle_boxes.data(), triangle_boxes.size()));
}


template <typename Scalar, typename Index, int Dim>
void TriangleAABBTree<Scalar, Index, Dim>::foreach_triangle_in_radius(
    const RowVectorType& p,
    Scalar sq_dist,
    function_ref<void(Scalar, Index, const RowVectorType&)> func) const
{
    auto vertices = vertex_view(m_mesh);
    auto facets = facet_view(m_mesh);

    m_aabb.foreach_element_within_radius(p, sq_dist, [&](Index triangle_idx) {
        Scalar bc[3];
        RowVectorType closest_point;

        // Use lagrange utility for point-triangle distance
        Scalar closest_sq_dist = point_triangle_squared_distance(
            p,
            vertices.row(facets(triangle_idx, 0)),
            vertices.row(facets(triangle_idx, 1)),
            vertices.row(facets(triangle_idx, 2)),
            closest_point,
            bc[0],
            bc[1],
            bc[2]);

        if (closest_sq_dist <= sq_dist) {
            func(closest_sq_dist, triangle_idx, closest_point);
        }
    });
}


template <typename Scalar, typename Index, int Dim>
bool TriangleAABBTree<Scalar, Index, Dim>::get_closest_point(
    const RowVectorType& p,
    Index& triangle_id,
    RowVectorType& closest_point,
    Scalar& closest_sq_dist) const
{
    typename AABB<Scalar, Dim>::Point query_point = p.transpose();
    if (empty()) {
        triangle_id = invalid<Index>();
        closest_sq_dist = std::numeric_limits<Scalar>::max();
        closest_point.setConstant(invalid<Scalar>());
        return false;
    }

    triangle_id = invalid<Index>();
    closest_sq_dist = std::numeric_limits<Scalar>::max();
    closest_point.setConstant(invalid<Scalar>());

    auto vertices = vertex_view(m_mesh);
    auto facets = facet_view(m_mesh);

    Scalar bc[3];
    auto get_closest_point = [&](Index tri_idx) {
        // Use lagrange utility for point-triangle distance
        return point_triangle_squared_distance(
            p,
            vertices.row(facets(tri_idx, 0)),
            vertices.row(facets(tri_idx, 1)),
            vertices.row(facets(tri_idx, 2)),
            closest_point,
            bc[0],
            bc[1],
            bc[2]);
    };

    triangle_id = m_aabb.get_closest_element(query_point, get_closest_point);
    closest_sq_dist = get_closest_point(triangle_id);

    return true;
}


#define LA_X_TriangleAABBTree(_, Scalar, Index)                   \
    template class LA_BVH_API TriangleAABBTree<Scalar, Index, 2>; \
    template class LA_BVH_API TriangleAABBTree<Scalar, Index, 3>;
LA_SURFACE_MESH_X(TriangleAABBTree, 0)

} // namespace lagrange::bvh
