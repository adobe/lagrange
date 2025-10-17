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

namespace lagrange::bvh {

///
/// Thin wrapper to expose EdgeAABBTree to Python.
///
template <typename Scalar, typename Index, int Dim>
class PyEdgeAABBTree
{
public:
    using VertexArray = Eigen::Matrix<Scalar, Eigen::Dynamic, Dim, Eigen::RowMajor>;
    using EdgeArray = Eigen::Matrix<Index, Eigen::Dynamic, 2, Eigen::RowMajor>;
    using Tree = EdgeAABBTree<VertexArray, EdgeArray, Dim>;

public:
    PyEdgeAABBTree(const VertexArray& V, const EdgeArray& E)
        : m_vertices(V)
        , m_edges(E)
        , m_tree(m_vertices, m_edges) {};

    bool empty() const { return m_tree.empty(); }

    void get_element_closest_point(
        const typename Tree::RowVectorType& p,
        typename Tree::Index element_id,
        typename Tree::RowVectorType& closest_point,
        typename Tree::Scalar& closest_sq_dist) const
    {
        m_tree.get_element_closest_point(p, element_id, closest_point, closest_sq_dist);
    }

    void foreach_element_in_radius(
        const typename Tree::RowVectorType& p,
        typename Tree::Scalar sq_dist,
        typename Tree::ActionCallback func) const
    {
        m_tree.foreach_element_in_radius(p, sq_dist, func);
    }

    void foreach_element_containing(
        const typename Tree::RowVectorType& p,
        typename Tree::ActionCallback func) const
    {
        m_tree.foreach_element_containing(p, func);
    }

    void get_closest_point(
        const typename Tree::RowVectorType& p,
        typename Tree::Index& element_id,
        typename Tree::RowVectorType& closest_point,
        typename Tree::Scalar& closest_sq_dist) const
    {
        m_tree.get_closest_point(p, element_id, closest_point, closest_sq_dist);
    }

private:
    VertexArray m_vertices;
    EdgeArray m_edges;
    Tree m_tree;
};

} // namespace lagrange::bvh
