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
#include <lagrange/bvh/AABB.h>
#include <lagrange/bvh/api.h>
#include <lagrange/utils/SmallVector.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/invalid.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <tbb/parallel_sort.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <algorithm>
#include <numeric>

namespace lagrange::bvh {

template <typename Scalar, int Dim>
void AABB<Scalar, Dim>::build(span<Box> boxes)
{
    if (boxes.empty()) {
        m_nodes.clear();
        m_root = invalid<Index>();
        return;
    }

    // Create indices for the boxes
    std::vector<Index> box_indices(boxes.size());
    std::iota(box_indices.begin(), box_indices.end(), 0);

    // Compute centroids for sorting
    std::vector<typename Box::VectorType> centroids(boxes.size());
    for (size_t i = 0; i < boxes.size(); ++i) {
        centroids[i] = boxes[i].center();
    }

    // Clear existing nodes
    m_nodes.clear();
    m_nodes.reserve(boxes.size() * 2);

    // Top-down recursive tree construction
    auto build_aabb_tree = [&](Index start, Index end, auto&& build_recursive) -> Index {
        // Empty range
        if (end - start == 0) {
            return invalid<Index>();
        }

        // Single box - create leaf node
        if (end - start == 1) {
            Node node;
            Index box_idx = box_indices[start];
            node.bbox = boxes[box_idx];
            node.left = invalid<Index>();
            node.right = invalid<Index>();
            node.element_idx = box_idx;
            m_nodes.push_back(node);
            return static_cast<Index>(m_nodes.size() - 1);
        }

        // Multiple boxes - create internal node
        // Find the longest Dimension of the centroid bounding box
        Box centroid_box;
        for (Index i = start; i < end; ++i) {
            centroid_box.extend(centroids[box_indices[i]]);
        }

        auto extent = centroid_box.diagonal();
        int longest_dim = 0;
        for (int d = 1; d < Dim; ++d) {
            if (extent(d) > extent(longest_dim)) {
                longest_dim = d;
            }
        }

        // Sort boxes by centroid along the longest Dimension
        tbb::parallel_sort(
            box_indices.begin() + start,
            box_indices.begin() + end,
            [&](Index a, Index b) {
                return centroids[a](longest_dim) < centroids[b](longest_dim);
            });

        // Create internal node
        Index current_idx = static_cast<Index>(m_nodes.size());
        m_nodes.resize(current_idx + 1);

        // Use better splitting strategy for small ranges
        Index midpoint = (start + end) / 2;
        Index left_child = build_recursive(start, midpoint, build_recursive);
        Index right_child = build_recursive(midpoint, end, build_recursive);

        Node& node = m_nodes[current_idx];
        node.left = left_child;
        node.right = right_child;
        node.element_idx = invalid<Index>();

        // Compute bounding box as union of children
        if (left_child != invalid<Index>() && right_child != invalid<Index>()) {
            node.bbox = m_nodes[left_child].bbox;
            node.bbox.extend(m_nodes[right_child].bbox);
        } else if (left_child != invalid<Index>()) {
            node.bbox = m_nodes[left_child].bbox;
        } else if (right_child != invalid<Index>()) {
            node.bbox = m_nodes[right_child].bbox;
        }

        return current_idx;
    };

    m_root = build_aabb_tree(0, static_cast<Index>(boxes.size()), build_aabb_tree);
}

template <typename Scalar, int Dim>
void AABB<Scalar, Dim>::intersect(const Box& query_box, std::vector<Index>& results) const
{
    results.clear();
    intersect(query_box, [&](Index idx) {
        results.push_back(idx);
        return true;
    });
}

template <typename Scalar, int Dim>
void AABB<Scalar, Dim>::intersect(const Box& query_box, function_ref<bool(Index)> callback) const
{
    if (m_nodes.empty() || m_root == invalid<Index>()) {
        return;
    }

    // Iterative stack-based traversal for better performance
    SmallVector<Index, 64> stack;
    stack.push_back(m_root);

    while (!stack.empty()) {
        Index node_idx = stack.back();
        stack.pop_back();

        if (node_idx == invalid<Index>()) {
            continue;
        }

        const Node& node = m_nodes[node_idx];

        // Check if query box intersects with this node's bounding box
        if (!query_box.intersects(node.bbox)) {
            continue;
        }

        if (node.is_leaf()) {
            // Leaf node - call callback with the element index
            bool keep_going = callback(node.element_idx);
            if (!keep_going) {
                break;
            }
        } else {
            // Internal node - add children to stack
            stack.push_back(node.right);
            stack.push_back(node.left);
        }
    }
}

template <typename Scalar, int Dim>
typename AABB<Scalar, Dim>::Index AABB<Scalar, Dim>::intersect_first(const Box& query_box) const
{
    Index result = invalid<Index>();
    intersect(query_box, [&](Index idx) {
        result = idx;
        return false;
    });
    return result;
}

template <typename Scalar, int Dim>
typename AABB<Scalar, Dim>::Index AABB<Scalar, Dim>::get_closest_element(
    const Point& q,
    function_ref<Scalar(Index)> sq_dist_fn) const
{
    if (m_nodes.empty() || m_root == invalid<Index>()) {
        return invalid<Index>();
    }
    constexpr Index invalid_index = invalid<Index>();

    Index closest_elem = invalid_index;
    Scalar closest_sq_dist = std::numeric_limits<Scalar>::max();

    SmallVector<Index, 64> stack;
    stack.push_back(m_root);

    while (!stack.empty()) {
        Index node_idx = stack.back();
        stack.pop_back();

        la_debug_assert(node_idx != invalid_index);
        const Node& node = m_nodes[node_idx];

        if (node.is_leaf()) {
            // Leaf node - compute distance using provided function
            la_debug_assert(node.element_idx != invalid_index);
            Scalar sq_dist = sq_dist_fn(node.element_idx);

            if (sq_dist < closest_sq_dist) {
                closest_sq_dist = sq_dist;
                closest_elem = node.element_idx;
            }
        } else {
            // Internal node - add children to stack
            la_debug_assert(node.element_idx == invalid_index);
            la_debug_assert(node.left != invalid_index);
            la_debug_assert(node.right != invalid_index);
            la_debug_assert(node.bbox.contains(m_nodes[node.left].bbox));
            la_debug_assert(node.bbox.contains(m_nodes[node.right].bbox));

            Scalar left_dist = m_nodes[node.left].bbox.squaredExteriorDistance(q);
            Scalar right_dist = m_nodes[node.right].bbox.squaredExteriorDistance(q);

            if (left_dist >= right_dist) {
                if (left_dist < closest_sq_dist) {
                    stack.push_back(node.left);
                }
                if (right_dist < closest_sq_dist) {
                    stack.push_back(node.right);
                }
            } else {
                if (right_dist < closest_sq_dist) {
                    stack.push_back(node.right);
                }
                if (left_dist < closest_sq_dist) {
                    stack.push_back(node.left);
                }
            }
        }
    }

    la_debug_assert(closest_elem != invalid_index);
    return closest_elem;
}

template <typename Scalar, int Dim>
void AABB<Scalar, Dim>::foreach_element_within_radius(
    const Point& q,
    Scalar sq_radius,
    function_ref<void(Index)> func) const
{
    constexpr Index invalid_index = invalid<Index>();

    if (m_nodes.empty() || m_root == invalid_index ||
        m_nodes[m_root].bbox.squaredExteriorDistance(q) > sq_radius) {
        return;
    }

    SmallVector<Index, 64> stack;
    stack.push_back(m_root);

    while (!stack.empty()) {
        Index node_idx = stack.back();
        stack.pop_back();

        la_debug_assert(node_idx != invalid_index);
        const Node& node = m_nodes[node_idx];

        if (node.is_leaf()) {
            // Leaf node - compute distance using provided function
            la_debug_assert(node.element_idx != invalid_index);
            func(node.element_idx);
        } else {
            // Internal node - add children to stack
            la_debug_assert(node.element_idx == invalid_index);
            la_debug_assert(node.left != invalid_index);
            la_debug_assert(node.right != invalid_index);
            la_debug_assert(node.bbox.contains(m_nodes[node.left].bbox));
            la_debug_assert(node.bbox.contains(m_nodes[node.right].bbox));

            Scalar left_dist = m_nodes[node.left].bbox.squaredExteriorDistance(q);
            Scalar right_dist = m_nodes[node.right].bbox.squaredExteriorDistance(q);

            if (left_dist >= right_dist) {
                if (left_dist < sq_radius) {
                    stack.push_back(node.left);
                }
                if (right_dist < sq_radius) {
                    stack.push_back(node.right);
                }
            } else {
                if (right_dist < sq_radius) {
                    stack.push_back(node.right);
                }
                if (left_dist < sq_radius) {
                    stack.push_back(node.left);
                }
            }
        }
    }
}

#define LA_X_AABB(_, Scalar)                   \
    template class LA_BVH_API AABB<Scalar, 2>; \
    template class LA_BVH_API AABB<Scalar, 3>;
LA_SURFACE_MESH_SCALAR_X(AABB, 0)

} // namespace lagrange::bvh
