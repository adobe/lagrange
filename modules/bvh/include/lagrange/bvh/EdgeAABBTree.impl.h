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

#include <lagrange/bvh/EdgeAABBTree.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/point_on_segment.h>
#include <lagrange/utils/point_segment_squared_distance.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <igl/barycenter.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <limits>
#include <numeric>

namespace lagrange {
namespace bvh {

////////////////////////////////////////////////////////////////////////////////

namespace internal {

template <typename Scalar>
Scalar sqr(Scalar x)
{
    return x * x;
}

///
/// Computes the squared distance between a point and a box.
///
/// @param[in]  p       The point.
/// @param[in]  B       The box.
///
/// @tparam     Scalar  Scalar type.
/// @tparam     Dim     Dimension of domain.
///
/// @return     The squared distance between @p p and @p B.
/// @pre        p is inside B.
///
template <typename Scalar, int Dim>
Scalar inner_point_box_squared_distance(
    const Eigen::Matrix<Scalar, Dim, 1>& p,
    const Eigen::AlignedBox<Scalar, Dim>& B)
{
    assert(B.contains(p));
    Scalar result = sqr(p[0] - B.min()[0]);
    result = std::min(result, sqr(p[0] - B.max()[0]));
    for (int c = 1; c < Dim; ++c) {
        result = std::min(result, sqr(p[c] - B.min()[c]));
        result = std::min(result, sqr(p[c] - B.max()[c]));
    }
    return result;
}

///
/// Computes the squared distance between a point and a box with negative sign if the point is
/// inside the box.
///
/// @param[in]  p       The point.
/// @param[in]  B       The box.
///
/// @tparam     Scalar  Scalar type.
///
/// @return     The signed squared distance between @p p and @p B.
///
template <typename Scalar, int Dim>
Scalar point_box_signed_squared_distance(
    const Eigen::Matrix<Scalar, Dim, 1>& p,
    const Eigen::AlignedBox<Scalar, Dim>& B)
{
    bool inside = true;
    Scalar result = 0;
    for (int c = 0; c < Dim; c++) {
        if (p[c] < B.min()[c]) {
            inside = false;
            result += sqr(p[c] - B.min()[c]);
        } else if (p[c] > B.max()[c]) {
            inside = false;
            result += sqr(p[c] - B.max()[c]);
        }
    }
    if (inside) {
        result = -inner_point_box_squared_distance(p, B);
    }
    return result;
}

///
/// Bounding box of an edge.
///
/// @param[in]  a     First endpoint.
/// @param[in]  b     Second endpoint.
///
/// @return     The bounding box.
///
template <typename Scalar, int Dim>
Eigen::AlignedBox<Scalar, Dim> bbox_edge(
    const Eigen::Matrix<Scalar, 1, Dim>& a,
    const Eigen::Matrix<Scalar, 1, Dim>& b)
{
    Eigen::AlignedBox<Scalar, Dim> bbox;
    bbox.extend(a.transpose());
    bbox.extend(b.transpose());
    return bbox;
}

} // namespace internal

////////////////////////////////////////////////////////////////////////////////

template <typename VertexArray, typename EdgeArray, int Dim>
EdgeAABBTree<VertexArray, EdgeArray, Dim>::EdgeAABBTree(const VertexArray& V, const EdgeArray& E)
{
    la_runtime_assert(Dim == V.cols(), "Dimension mismatch in EdgeAABBTree!");

    m_vertices = &V;
    m_edges = &E;

    // Compute bounding boxes for all edges
    std::vector<typename AABB<Scalar, Dim>::Box> aabb_boxes(E.rows());

    for (Index i = 0; i < static_cast<Index>(E.rows()); ++i) {
        RowVectorType a = V.row(E(i, 0));
        RowVectorType b = V.row(E(i, 1));
        auto& bbox = aabb_boxes[i];
        bbox.setEmpty();
        bbox.extend(a.transpose());
        bbox.extend(b.transpose());
    }

    // Build the AABB tree
    m_aabb.build(span<typename AABB<Scalar, Dim>::Box>(aabb_boxes.data(), aabb_boxes.size()));
}

// -----------------------------------------------------------------------------

template <typename VertexArray, typename EdgeArray, int Dim>
void EdgeAABBTree<VertexArray, EdgeArray, Dim>::get_element_closest_point(
    const RowVectorType& p,
    Index element_id,
    RowVectorType& closest_point,
    Scalar& closest_sq_dist) const
{
    RowVectorType v0 = m_vertices->row((*m_edges)(element_id, 0));
    RowVectorType v1 = m_vertices->row((*m_edges)(element_id, 1));
    Scalar l0, l1;
    closest_sq_dist = point_segment_squared_distance(p, v0, v1, closest_point, l0, l1);
    if (point_on_segment(p, v0, v1)) {
        closest_sq_dist = 0;
        closest_point = p;
    }
}

// -----------------------------------------------------------------------------

template <typename VertexArray, typename EdgeArray, int Dim>
void EdgeAABBTree<VertexArray, EdgeArray, Dim>::foreach_element_in_radius(
    const RowVectorType& p,
    Scalar sq_dist,
    function_ref<void(Scalar, Index, const RowVectorType&)> func) const
{
    m_aabb.foreach_element_within_radius(p, sq_dist, [&](Index edge_idx) {
        RowVectorType closest_point;
        Scalar closest_sq_dist;
        get_element_closest_point(p, edge_idx, closest_point, closest_sq_dist);
        if (closest_sq_dist <= sq_dist) {
            func(closest_sq_dist, edge_idx, closest_point);
        }
    });
}

// -----------------------------------------------------------------------------

template <typename VertexArray, typename EdgeArray, int Dim>
void EdgeAABBTree<VertexArray, EdgeArray, Dim>::foreach_element_containing(
    const RowVectorType& p,
    function_ref<void(Scalar, Index, const RowVectorType&)> func) const
{
    // Create a point query box
    typename AABB<Scalar, Dim>::Box query_box;
    typename AABB<Scalar, Dim>::Box::VectorType point_vec = p.transpose();
    query_box.min() = point_vec;
    query_box.max() = point_vec;

    // Check each intersecting edge for exact containment
    m_aabb.intersect(query_box, [&](Index aabb_idx) {
        Index edge_idx = static_cast<Index>(aabb_idx);
        RowVectorType p0 = m_vertices->row((*m_edges)(edge_idx, 0));
        RowVectorType p1 = m_vertices->row((*m_edges)(edge_idx, 1));
        if (point_on_segment(p, p0, p1)) {
            func(0, edge_idx, p);
        }
        return true;
    });
}

// -----------------------------------------------------------------------------

template <typename VertexArray, typename EdgeArray, int Dim>
void EdgeAABBTree<VertexArray, EdgeArray, Dim>::get_closest_point(
    const RowVectorType& query_pt,
    Index& element_id,
    RowVectorType& closest_point,
    Scalar& closest_sq_dist,
    function_ref<bool(Index)> filter_func) const
{
    la_runtime_assert(!empty());
    element_id = invalid<Index>();
    closest_sq_dist = std::numeric_limits<Scalar>::max();
    closest_point.setConstant(invalid<Scalar>());

    auto get_closest_point = [&](Index edge_id) {
        la_debug_assert(edge_id != invalid<Index>());
        if (!filter_func || filter_func(edge_id)) {
            get_element_closest_point(query_pt, edge_id, closest_point, closest_sq_dist);
            return closest_sq_dist;
        } else {
            return std::numeric_limits<Scalar>::max();
        }
    };
    element_id = m_aabb.get_closest_element(query_pt.transpose(), get_closest_point);
    get_closest_point(element_id);
}

} // namespace bvh
} // namespace lagrange
