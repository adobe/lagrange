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
#include <lagrange/point_segment_squared_distance.h>
#include <lagrange/utils/assert.h>
#include <lagrange/point_on_segment.h>

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
/// @tparam     DIM     Dimension of domain.
///
/// @return     The squared distance between @p p and @p B.
/// @pre        p is inside B.
///
template <typename Scalar, int DIM>
Scalar inner_point_box_squared_distance(
    const Eigen::Matrix<Scalar, DIM, 1> &p,
    const Eigen::AlignedBox<Scalar, DIM> &B)
{
    assert(B.contains(p));
    Scalar result = sqr(p[0] - B.min()[0]);
    result = std::min(result, sqr(p[0] - B.max()[0]));
    for (int c = 1; c < DIM; ++c) {
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
template <typename Scalar, int DIM>
Scalar point_box_signed_squared_distance(
    const Eigen::Matrix<Scalar, DIM, 1> &p,
    const Eigen::AlignedBox<Scalar, DIM> &B)
{
    bool inside = true;
    Scalar result = 0;
    for (int c = 0; c < DIM; c++) {
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
template <typename Scalar, int DIM>
Eigen::AlignedBox<Scalar, DIM> bbox_edge(
    const Eigen::Matrix<Scalar, 1, DIM> &a,
    const Eigen::Matrix<Scalar, 1, DIM> &b)
{
    Eigen::AlignedBox<Scalar, DIM> bbox;
    bbox.extend(a.transpose());
    bbox.extend(b.transpose());
    return bbox;
}

} // namespace internal

////////////////////////////////////////////////////////////////////////////////

template <typename VertexArray, typename EdgeArray, int DIM>
EdgeAABBTree<VertexArray, EdgeArray, DIM>::EdgeAABBTree(const VertexArray &V, const EdgeArray &E)
{
    la_runtime_assert(DIM == V.cols(), "Dimension mismatch in EdgeAABBTree!");
    // Compute the centroids of all the edges in the input mesh
    Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> centroids;
    igl::barycenter(V, E, centroids);

    // Top-down approach: split each set of primitives into 2 sets of roughly equal size,
    // based on sorting the centroids along one direction or another.
    std::vector<Index> edges(E.rows());
    std::iota(edges.begin(), edges.end(), 0);

    std::function<Index(Index, Index, Index)> top_down = [&](Index i, Index j, Index parent) {
        // Scene is empty, so is the aabb tree
        if (j - i == 0) {
            return invalid<Index>();
        }

        // If there is only 1 edges left, then we are at a leaf
        if (j - i == 1) {
            Node node;
            Index e = edges[i];
            RowVectorType a = V.row(E(e, 0));
            RowVectorType b = V.row(E(e, 1));
            node.bbox = internal::bbox_edge<Scalar, DIM>(a, b);
            node.parent = parent;
            node.left = node.right = invalid<Index>();
            node.index = e;
            m_nodes.push_back(node);
            return (Index)(m_nodes.size() - 1);
        }

        // Otherwise, we need to sort centroids along the longest dimension, and split recursively
        Eigen::AlignedBox<Scalar, DIM> centroid_box;
        for (Index k = i; k < j; ++k) {
            Eigen::Matrix<Scalar, DIM, 1> c = centroids.row(edges[k]).transpose();
            centroid_box.extend(c);
        }
        auto extent = centroid_box.diagonal();
        int longest_dim = 0;
        for (int dim = 1; dim < DIM; ++dim) {
            if (extent(dim) > extent(longest_dim)) {
                longest_dim = dim;
            }
        }
        std::sort(edges.begin() + i, edges.begin() + j, [&](Index f1, Index f2) {
            return centroids(f1, longest_dim) < centroids(f2, longest_dim);
        });

        // Then we can create a new internal node
        Index current = (Index)m_nodes.size();
        m_nodes.resize(current + 1);
        Index midpoint = (i + j) / 2;
        Index left = top_down(i, midpoint, current);
        Index right = top_down(midpoint, j, current);
        Node &node = m_nodes[current];
        node.left = left;
        node.right = right;
        node.parent = parent;
        node.index = invalid<Index>();
        node.bbox = m_nodes[node.left].bbox.extend(m_nodes[node.right].bbox);

        return current;
    };

    m_root = top_down(0, (Index)edges.size(), invalid<Index>());
    m_vertices = &V;
    m_edges = &E;
}

// -----------------------------------------------------------------------------

template <typename VertexArray, typename EdgeArray, int DIM>
void EdgeAABBTree<VertexArray, EdgeArray, DIM>::get_element_closest_point(
    const RowVectorType &p,
    Index element_id,
    RowVectorType &closest_point,
    Scalar &closest_sq_dist) const
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

template <typename VertexArray, typename EdgeArray, int DIM>
void EdgeAABBTree<VertexArray, EdgeArray, DIM>::foreach_element_in_radius(
    const RowVectorType &p,
    Scalar sq_dist,
    std::function<void(Scalar, Index, const RowVectorType &)> func) const
{
    foreach_element_in_radius_recursive(p, sq_dist, (Index)m_root, func);
}

template <typename VertexArray, typename EdgeArray, int DIM>
void EdgeAABBTree<VertexArray, EdgeArray, DIM>::foreach_element_in_radius_recursive(
    const RowVectorType &p,
    Scalar sq_dist,
    Index node_id,
    ActionCallback func) const
{
    const auto &node = m_nodes[node_id];
    if (node.is_leaf()) {
        RowVectorType closest_point;
        Scalar closest_sq_dist;
        get_element_closest_point(p, node.index, closest_point, closest_sq_dist);
        if (closest_sq_dist <= sq_dist) {
            func(closest_sq_dist, node.index, closest_point);
        }
        return;
    }

    const Scalar dl =
        internal::point_box_signed_squared_distance<Scalar, DIM>(p, m_nodes[node.left].bbox);
    const Scalar dr =
        internal::point_box_signed_squared_distance<Scalar, DIM>(p, m_nodes[node.right].bbox);

    if (dl <= sq_dist) {
        foreach_element_in_radius_recursive(p, sq_dist, node.left, func);
    }
    if (dr <= sq_dist) {
        foreach_element_in_radius_recursive(p, sq_dist, node.right, func);
    }
}

// -----------------------------------------------------------------------------

template <typename VertexArray, typename EdgeArray, int DIM>
void EdgeAABBTree<VertexArray, EdgeArray, DIM>::foreach_element_containing(
    const RowVectorType &p,
    std::function<void(Scalar, Index, const RowVectorType &)> func) const
{
    foreach_element_containing_recursive(p, (Index)m_root, func);
}

template <typename VertexArray, typename EdgeArray, int DIM>
void EdgeAABBTree<VertexArray, EdgeArray, DIM>::foreach_element_containing_recursive(
    const RowVectorType &p,
    Index node_id,
    ActionCallback func) const
{
    const auto &node = m_nodes[node_id];
    if (node.is_leaf()) {
        RowVectorType p0 = m_vertices->row((*m_edges)(node.index, 0));
        RowVectorType p1 = m_vertices->row((*m_edges)(node.index, 1));
        if (point_on_segment(p, p0, p1)) {
            func(0, node.index, p);
        }
        return;
    }

    const Scalar dl =
        internal::point_box_signed_squared_distance<Scalar, DIM>(p, m_nodes[node.left].bbox);
    const Scalar dr =
        internal::point_box_signed_squared_distance<Scalar, DIM>(p, m_nodes[node.right].bbox);

    if (dl <= 0) {
        foreach_element_containing_recursive(p, node.left, func);
    }
    if (dr <= 0) {
        foreach_element_containing_recursive(p, node.right, func);
    }
}

// -----------------------------------------------------------------------------

template <typename VertexArray, typename EdgeArray, int DIM>
void EdgeAABBTree<VertexArray, EdgeArray, DIM>::get_closest_point(
    const RowVectorType &query_pt,
    Index &element_id,
    RowVectorType &closest_point,
    Scalar &closest_sq_dist,
    std::function<bool(Index)> filter_func) const
{
    la_runtime_assert(!empty());
    element_id = invalid<Index>();
    closest_sq_dist = std::numeric_limits<Scalar>::max();
    closest_point.setConstant(invalid<Scalar>());

    std::function<void(Index)> traverse_aabb_tree;
    traverse_aabb_tree = [&](Index node_id) {
        const auto &node = m_nodes[node_id];
        if (node.is_leaf()) {
            if (!filter_func || filter_func(node.index)) {
                RowVectorType _closest_point;
                Scalar _closest_sq_dist;
                get_element_closest_point(query_pt, node.index, _closest_point, _closest_sq_dist);
                if (_closest_sq_dist <= closest_sq_dist) {
                    closest_sq_dist = _closest_sq_dist;
                    element_id = node.index;
                    closest_point = _closest_point;
                }
            }
        } else {
            using namespace internal;
            const Scalar dl =
                point_box_signed_squared_distance<Scalar, DIM>(query_pt, m_nodes[node.left].bbox);
            const Scalar dr =
                point_box_signed_squared_distance<Scalar, DIM>(query_pt, m_nodes[node.right].bbox);

            // Explore the nearest subtree first.
            if (dl < dr) {
                if (dl <= closest_sq_dist) {
                    traverse_aabb_tree(node.left);
                }
                if (dr <= closest_sq_dist) {
                    traverse_aabb_tree(node.right);
                }
            } else {
                if (dr <= closest_sq_dist) {
                    traverse_aabb_tree(node.right);
                }
                if (dl <= closest_sq_dist) {
                    traverse_aabb_tree(node.left);
                }
            }
        }
    };

    traverse_aabb_tree((Index)m_root);
}

} // namespace bvh
} // namespace lagrange
