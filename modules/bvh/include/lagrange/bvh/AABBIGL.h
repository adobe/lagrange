/*
 * Copyright 2019 Adobe. All rights reserved.
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

#include <exception>
#include <limits>
#include <memory>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <igl/AABB.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <lagrange/bvh/BVH.h>
#include <lagrange/Logger.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/safe_cast.h>
#include <lagrange/utils/range.h>

namespace lagrange {
namespace bvh {

template <typename _VertexArray, typename _ElementArray = lagrange::Triangles>
class AABBIGL : public BVH<_VertexArray, _ElementArray>
{
public:
    using Parent = BVH<_VertexArray, _ElementArray>;
    using VertexArray = typename Parent::VertexArray;
    using ElementArray = typename Parent::ElementArray;
    using Scalar = typename Parent::Scalar;
    using Index = typename Parent::Index;
    using PointType = typename Parent::PointType;
    using ClosestPoint = typename Parent::ClosestPoint;
    using AABB = igl::AABB<VertexArray, 3>;

public:
    BVHType get_bvh_type() const override
    { //
        return BVHType::IGL;
    }

    bool does_support_pointcloud() const override
    { //
        // This is actually not true for at least the current version of
        // libigl. https://github.com/libigl/libigl/issues/1040
        return false;
    }
    bool does_support_triangles() const override
    { //
        return true;
    }
    bool does_support_lines() const override
    { //
        return false;
    }

    void build(const VertexArray& vertices, const ElementArray& elements) override
    {
        la_runtime_assert(elements.cols() == 3, "LibIGL AABB only supports triangles mesh");
        m_vertices = vertices;
        m_elements = elements;
        m_aabb.init(m_vertices, m_elements);
    }

    void build(const VertexArray&) override
    {
        la_runtime_assert(0, "LibIGL AABB does not support a pointcloud");
    }

    bool does_support_query_closest_point() const override
    { //
        return true;
    }
    ClosestPoint query_closest_point(const PointType& p) const override
    {
        ClosestPoint r;
        int idx = -1;
        r.squared_distance =
            m_aabb.squared_distance(m_vertices, m_elements, p, idx, r.closest_point);
        r.embedding_element_idx = safe_cast<Index>(idx);

        compute_closest_vertex_within_element(r, p);
        return r;
    }

    bool does_support_query_k_nearest_neighbours() const override
    {
        return false;
    }
    std::vector<ClosestPoint> query_k_nearest_neighbours(const PointType& p, int k) const override
    {
        (void)p; (void)k;
        throw std::runtime_error("LibIGL AABB does not support KNN queries");
    }

    bool does_support_query_in_sphere_neighbours() const override
    { //
        return false;
    }
    std::vector<ClosestPoint> query_in_sphere_neighbours(const PointType&, const Scalar)
        const override
    {
        throw std::runtime_error("LibIGL AABB does not support radius queries");
    }

    std::vector<ClosestPoint> batch_query_closest_point(const VertexArray& query_pts) const override
    {
        const auto num_queries = query_pts.rows();
        Eigen::Matrix<Scalar, Eigen::Dynamic, 1> sq_dists(num_queries);
        Eigen::Matrix<Index, Eigen::Dynamic, 1> embedding_elements(num_queries);
        VertexArray closest_points(num_queries, m_vertices.cols());

        m_aabb.squared_distance(
            m_vertices,
            m_elements,
            query_pts,
            sq_dists,
            embedding_elements,
            closest_points);

        std::vector<ClosestPoint> r(num_queries);
        for (auto i : range(num_queries)) {
            r[i].embedding_element_idx = embedding_elements[i];
            r[i].squared_distance = sq_dists[i];
            r[i].closest_point = closest_points.row(i);
            compute_closest_vertex_within_element(r[i], query_pts.row(i));
        }
        return r;
    }

private:
    /**
     * Compute the closest vertex within the closest element.
     * Note this is **different** from the global closest vertex!
     */
    void compute_closest_vertex_within_element(ClosestPoint& entry, const PointType& p) const
    {
        assert(entry.embedding_element_idx >= 0);
        assert(entry.embedding_element_idx < safe_cast<Index>(m_elements.rows()));
        const auto vertex_per_element = m_elements.cols();
        Scalar min_v_dist = std::numeric_limits<Scalar>::max();
        for (auto i : range(vertex_per_element)) {
            const auto vid = m_elements(entry.embedding_element_idx, i);
            const auto sq_dist = (m_vertices.row(vid) - p).squaredNorm();
            if (sq_dist < min_v_dist) {
                min_v_dist = sq_dist;
                entry.closest_vertex_idx = vid;
            }
        }
    }


private:
    VertexArray m_vertices;
    ElementArray m_elements;
    AABB m_aabb;
};

} // namespace bvh
} // namespace lagrange
