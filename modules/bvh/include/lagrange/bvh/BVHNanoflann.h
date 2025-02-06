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

#include <memory>

#include <nanoflann.hpp>

#include <lagrange/Logger.h>
#include <lagrange/bvh/BVH.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/safe_cast.h>

namespace lagrange {
namespace bvh {

template <typename _VertexArray, typename _ElementArray = lagrange::Triangles>
class BVHNanoflann : public BVH<_VertexArray, _ElementArray>
{
public:
    using Parent = BVH<_VertexArray, _ElementArray>;
    using VertexArray = typename Parent::VertexArray;
    using ElementArray = typename Parent::ElementArray;
    using Scalar = typename Parent::Scalar;
    using Index = typename Parent::Index;
    using PointType = typename Parent::PointType;
    using ClosestPoint = typename Parent::ClosestPoint;
    using KDTree = nanoflann::KDTreeEigenMatrixAdaptor<VertexArray>;

public:
    BVHType get_bvh_type() const override
    {
        return BVHType::NANOFLANN;
    }

    bool does_support_pointcloud() const override
    {
        return true;
    }
    bool does_support_triangles() const override
    {
        return false;
    }
    bool does_support_lines() const override
    {
        return false;
    }

    void build(const VertexArray&, const ElementArray&) override
    {
        la_runtime_assert(0, "BVHNannoflann does not support elements.");
    }

    void build(const VertexArray& vertices) override
    {
        // Nanoflann stores a const ref to vertices, we need to make sure
        // vertices outlives the BVH.  Storing a local copy for now.
        m_vertices = vertices;

        constexpr int max_leaf = 10; // TODO: Experiment with different values.
        m_tree = std::make_unique<KDTree>(m_vertices.cols(), m_vertices, max_leaf);
        m_tree->index_->buildIndex();
    }

    bool does_support_query_closest_point() const override
    {
        return true;
    }
    ClosestPoint query_closest_point(const PointType& p) const override
    {
        la_runtime_assert(m_tree);
        ClosestPoint r;

        typename KDTree::IndexType out_idx[1];
        Scalar out_sq_dist[1];
        auto num_valid_pts = m_tree->index_->knnSearch(p.data(), 1, out_idx, out_sq_dist);

        if (num_valid_pts == 0) {
            throw std::runtime_error("Nanoflann did not find any valid closest points.");
        }

        r.closest_vertex_idx = lagrange::safe_cast<Index>(out_idx[0]);
        r.closest_point = m_vertices.row(out_idx[0]);
        r.squared_distance = out_sq_dist[0];

        return r;
    }

    bool does_support_query_k_nearest_neighbours() const override
    {
        return true;
    }
    std::vector<ClosestPoint> query_k_nearest_neighbours(const PointType& p, int k) const override
    {
        la_runtime_assert(m_tree);
        std::vector<ClosestPoint> rs;

        std::vector<typename KDTree::IndexType> out_idxs(k);
        std::vector<Scalar> out_sq_dists(k);
        auto num_valid_pts =
            m_tree->index_->knnSearch(p.data(), k, out_idxs.data(), out_sq_dists.data());

        rs.resize(num_valid_pts);
        for (size_t i = 0; i < num_valid_pts; i++) {
            rs[i].closest_vertex_idx = lagrange::safe_cast<Index>(out_idxs[i]);
            rs[i].closest_point = m_vertices.row(out_idxs[i]);
            rs[i].squared_distance = out_sq_dists[i];
        }

        return rs;
    }

    bool does_support_query_in_sphere_neighbours() const override
    {
        return true;
    }
    std::vector<ClosestPoint> query_in_sphere_neighbours(const PointType& p, const Scalar radius)
        const override
    {
        la_runtime_assert(m_tree);
        std::vector<ClosestPoint> r;

        std::vector<nanoflann::ResultItem<typename KDTree::IndexType, Scalar>> output;
        nanoflann::SearchParameters params(
            0, // Epsilon, should be 0.
            true); // Sort result by distance.
        m_tree->index_->radiusSearch(p.data(), radius * radius, output, params);

        r.reserve(output.size());
        for (const auto& entry : output) {
            ClosestPoint cp;
            cp.closest_vertex_idx = (Index)entry.first;
            cp.closest_point = m_vertices.row(cp.closest_vertex_idx);
            cp.squared_distance = entry.second;
            r.push_back(std::move(cp));
        }
        return r;
    }

    std::vector<ClosestPoint> batch_query_closest_point(const VertexArray& query_pts) const override
    {
        return Parent::default_batch_query_closest_point(query_pts);
    }


private:
    VertexArray m_vertices;
    std::unique_ptr<KDTree> m_tree;
};

} // namespace bvh
} // namespace lagrange
