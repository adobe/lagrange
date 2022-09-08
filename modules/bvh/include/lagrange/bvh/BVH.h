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

#include <algorithm>
#include <exception>
#include <vector>

#include <lagrange/bvh/BVHType.h>
#include <lagrange/common.h>
#include <lagrange/utils/range.h>

namespace lagrange {
namespace bvh {

template <typename _VertexArray, typename _ElementArray>
class BVH
{
public:
    using VertexArray = _VertexArray;
    using ElementArray = _ElementArray;
    using Scalar = typename VertexArray::Scalar;
    using Index = typename ElementArray::Scalar;
    using PointType = Eigen::Matrix<Scalar, 1, VertexArray::ColsAtCompileTime>;

public:
    BVH() = default;
    virtual ~BVH() = default;

public:
    /**
     * Get the enum type
     */
    virtual BVHType get_bvh_type() const = 0;

    /**
     * Does it support supplying elements or just points?
     *
     * By using this function the user can prevent accidentally calling build on a type
     * that does not support elements, and face an exception.
     */
    virtual bool does_support_pointcloud() const = 0;
    virtual bool does_support_triangles() const = 0;
    virtual bool does_support_lines() const = 0;

    /**
     * Construct bvh based on vertices and elements.
     */
    virtual void build(const VertexArray& vertices, const ElementArray& elements) = 0;

    virtual void build(const VertexArray& vertices) = 0;

    struct ClosestPoint
    {
        Index embedding_element_idx = invalid<Index>();
        Index closest_vertex_idx = invalid<Index>();
        PointType closest_point;
        Scalar squared_distance = invalid<Scalar>();
    };

    /**
     * Query for the closest point.
     */
    virtual bool does_support_query_closest_point() const = 0;
    virtual ClosestPoint query_closest_point(const PointType& p) const = 0;

    /**
     * Query for the k nearest neighbours.
     */
    virtual bool does_support_query_k_nearest_neighbours() const = 0;
    virtual std::vector<ClosestPoint> query_k_nearest_neighbours(const PointType& p, int k)
        const = 0;

    /**
     * Query for the closest point with in radius.
     */
    virtual bool does_support_query_in_sphere_neighbours() const = 0;
    virtual std::vector<ClosestPoint> query_in_sphere_neighbours(
        const PointType& p,
        const Scalar radius) const = 0;

    /**
     * Batch query closest points.
     */
    virtual std::vector<ClosestPoint> batch_query_closest_point(
        const VertexArray& query_pts) const = 0;

protected:
    std::vector<ClosestPoint> default_batch_query_closest_point(const VertexArray& query_pts) const
    {
        const auto num_queries = query_pts.rows();
        std::vector<ClosestPoint> results(num_queries);

        // TODO: investigate use of tbb here if need be.
        std::transform(
            query_pts.rowwise().begin(),
            query_pts.rowwise().end(),
            results.begin(),
            [this](const PointType& p) { return this->query_closest_point(p); });

        return results;
    }
};

} // namespace bvh
} // namespace lagrange
