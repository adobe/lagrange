/*
 * Copyright 2026 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */

#include <lagrange/Attribute.h>
#include <lagrange/AttributeValueType.h>
#include <lagrange/SurfaceMesh.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/compute_area.h>
#include <lagrange/sampling/api.h>
#include <lagrange/sampling/sample_epsilon.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/invalid.h>
#include <lagrange/utils/safe_cast.h>
#include <lagrange/utils/tracy.h>
#include <lagrange/views.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <nanoflann.hpp>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <pcg_random.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <random>
#include <string>
#include <vector>

namespace lagrange::sampling {

namespace {

/// Point cloud adaptor over an externally-owned, growable, flat position buffer for nanoflann.
template <typename Scalar>
struct SampleCloud
{
    const std::vector<Scalar>* points = nullptr; // flat, num_points * dim
    int dim = 3;

    inline size_t kdtree_get_point_count() const
    {
        return dim > 0 ? points->size() / static_cast<size_t>(dim) : 0;
    }

    inline Scalar kdtree_get_pt(size_t idx, size_t d) const
    {
        return (*points)[idx * static_cast<size_t>(dim) + d];
    }

    template <typename BBOX>
    bool kdtree_get_bbox(BBOX&) const
    {
        return false;
    }
};

using KDTreeIndex = std::uint32_t;

template <typename Scalar>
using DynamicKDTree = nanoflann::KDTreeSingleIndexDynamicAdaptor<
    nanoflann::L2_Simple_Adaptor<Scalar, SampleCloud<Scalar>>,
    SampleCloud<Scalar>,
    -1 /* runtime dimensionality */,
    KDTreeIndex>;

/// Squared distance from `query` (length `dim`) to the nearest accepted sample. Returns a very
/// large value when there are no accepted samples yet.
template <typename Scalar>
Scalar nearest_sample_distance_squared(
    const DynamicKDTree<Scalar>& tree,
    size_t num_accepted,
    const Scalar* query)
{
    if (num_accepted == 0) return std::numeric_limits<Scalar>::max();
    size_t nn_index = 0;
    Scalar nn_dist_sqr = std::numeric_limits<Scalar>::max();
    nanoflann::KNNResultSet<Scalar, size_t> result_set(1);
    result_set.init(&nn_index, &nn_dist_sqr);
    tree.findNeighbors(result_set, query);
    return nn_dist_sqr;
}

} // namespace

template <typename Scalar, typename Index>
EpsilonSamplingResult<Scalar, Index> sample_epsilon(
    const SurfaceMesh<Scalar, Index>& mesh,
    std::string_view lfs_attribute,
    Scalar epsilon,
    const EpsilonSamplingOptions& options)
{
    LAGRANGE_ZONE_SCOPED;
    la_runtime_assert(mesh.is_triangle_mesh(), "Only triangle meshes are supported.");
    la_runtime_assert(epsilon > 0, "epsilon must be strictly positive.");
    la_runtime_assert(options.max_failed_attempts > 0, "max_failed_attempts must be positive.");

    // Validate and read the local feature size attribute.
    la_runtime_assert(
        mesh.has_attribute(lfs_attribute),
        "Local feature size attribute '" + std::string(lfs_attribute) + "' not found.");
    const auto lfs_attr_id = mesh.get_attribute_id(lfs_attribute);
    {
        const auto& attr = mesh.get_attribute_base(lfs_attr_id);
        la_runtime_assert(
            attr.get_element_type() == AttributeElement::Vertex,
            "Local feature size attribute must be a per-vertex attribute.");
        la_runtime_assert(
            attr.get_num_channels() == 1,
            "Local feature size attribute must be a single-channel scalar attribute.");
        la_runtime_assert(
            attr.get_value_type() == make_attribute_value_type<Scalar>(),
            "Local feature size attribute value type must match the mesh scalar type.");
    }
    auto lfs = attribute_vector_view<Scalar>(mesh, lfs_attr_id);
    la_runtime_assert(
        lfs.size() == 0 || lfs.minCoeff() > 0,
        "Local feature size values must be strictly positive.");

    const int dim = static_cast<int>(mesh.get_dimension());
    auto vertices = vertex_view(mesh);
    auto triangles = facet_view(mesh);

    // Compute facet areas and a cumulative-area table for area-weighted facet selection.
    auto mesh_copy = mesh;
    const auto area_attr_id = compute_facet_area(mesh_copy);
    auto areas = attribute_vector_view<Scalar>(mesh_copy, area_attr_id);
    const Index num_facets = safe_cast<Index>(mesh.get_num_facets());
    std::vector<Scalar> cumulative_areas(static_cast<size_t>(num_facets));
    std::partial_sum(areas.begin(), areas.end(), cumulative_areas.begin());
    const Scalar total_area = cumulative_areas.empty() ? Scalar(0) : cumulative_areas.back();
    la_runtime_assert(total_area > 0, "Mesh must have positive non-null area.");

    // Accepted samples and their spatial index; the cloud reads directly from `result.samples`.
    EpsilonSamplingResult<Scalar, Index> result;
    SampleCloud<Scalar> cloud;
    cloud.points = &result.samples;
    cloud.dim = dim;
    DynamicKDTree<Scalar> tree(dim, cloud, nanoflann::KDTreeSingleIndexAdaptorParams(10));

    size_t num_accepted = 0;

    auto accept_sample =
        [&](const std::array<Scalar, 3>& position, Index facet, const std::array<Scalar, 3>& bary) {
            const auto new_index = static_cast<KDTreeIndex>(num_accepted);
            for (int d = 0; d < dim; ++d) {
                result.samples.push_back(position[d]);
            }
            tree.addPoints(new_index, new_index);
            result.facet_indices.push_back(facet);
            if (options.compute_barycentric) {
                result.barycentric.push_back(bary[0]);
                result.barycentric.push_back(bary[1]);
                result.barycentric.push_back(bary[2]);
            }
            ++num_accepted;
        };

    pcg32 rng(options.seed);
    std::uniform_real_distribution<Scalar> dist_area(0, total_area);
    std::uniform_real_distribution<Scalar> dist_unit(0, 1);

    // Dart throwing: reject a dart if an accepted sample lies within epsilon * lfs of it. When
    // max_failed_attempts consecutive darts are all rejected, the sampling is (probabilistically)
    // maximal and every surface point is within epsilon * lfs of a sample.
    size_t consecutive_failures = 0;
    while (consecutive_failures < options.max_failed_attempts) {
        const Scalar t = dist_area(rng);
        const Index facet = std::min(
            safe_cast<Index>(std::distance(
                cumulative_areas.begin(),
                std::lower_bound(cumulative_areas.begin(), cumulative_areas.end(), t))),
            num_facets - 1);

        Scalar alpha = dist_unit(rng);
        Scalar beta = dist_unit(rng);
        if (alpha + beta > 1) {
            alpha = 1 - alpha;
            beta = 1 - beta;
        }
        const std::array<Scalar, 3> bary = {1 - alpha - beta, alpha, beta};

        const Index v0 = triangles(facet, 0);
        const Index v1 = triangles(facet, 1);
        const Index v2 = triangles(facet, 2);

        std::array<Scalar, 3> position = {0, 0, 0};
        for (int d = 0; d < dim; ++d) {
            position[d] =
                bary[0] * vertices(v0, d) + bary[1] * vertices(v1, d) + bary[2] * vertices(v2, d);
        }
        const Scalar lfs_p = bary[0] * lfs(v0) + bary[1] * lfs(v1) + bary[2] * lfs(v2);
        const Scalar radius = epsilon * lfs_p;

        const Scalar nn_dist_sqr =
            nearest_sample_distance_squared(tree, num_accepted, position.data());
        if (nn_dist_sqr >= radius * radius) {
            accept_sample(position, facet, bary);
            consecutive_failures = 0;
        } else {
            ++consecutive_failures;
        }
    }

    // Optionally close vertex-scale coverage gaps deterministically.
    if (options.ensure_vertex_coverage) {
        const Index num_vertices = safe_cast<Index>(mesh.get_num_vertices());
        std::vector<Index> vertex_facet(static_cast<size_t>(num_vertices), invalid<Index>());
        std::vector<uint8_t> vertex_corner(static_cast<size_t>(num_vertices), 0);
        for (Index f = 0; f < num_facets; ++f) {
            for (uint8_t c = 0; c < 3; ++c) {
                const Index v = triangles(f, c);
                if (vertex_facet[v] == invalid<Index>()) {
                    vertex_facet[v] = f;
                    vertex_corner[v] = c;
                }
            }
        }
        for (Index v = 0; v < num_vertices; ++v) {
            if (vertex_facet[v] == invalid<Index>()) continue;
            std::array<Scalar, 3> position = {0, 0, 0};
            for (int d = 0; d < dim; ++d) position[d] = vertices(v, d);
            const Scalar radius = epsilon * lfs(v);
            const Scalar nn_dist_sqr =
                nearest_sample_distance_squared(tree, num_accepted, position.data());
            if (nn_dist_sqr >= radius * radius) {
                std::array<Scalar, 3> bary = {0, 0, 0};
                bary[vertex_corner[v]] = 1;
                accept_sample(position, vertex_facet[v], bary);
            }
        }
    }

    return result;
}

#define LA_X_sample_epsilon(_, Scalar, Index)                                     \
    template LA_SAMPLING_API EpsilonSamplingResult<Scalar, Index> sample_epsilon( \
        const SurfaceMesh<Scalar, Index>&,                                        \
        std::string_view,                                                         \
        Scalar,                                                                   \
        const EpsilonSamplingOptions&);
LA_SURFACE_MESH_X(sample_epsilon, 0)

} // namespace lagrange::sampling
