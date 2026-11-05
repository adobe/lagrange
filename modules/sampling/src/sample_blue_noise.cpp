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

#include "internal/uniform_sample.h"

#include <lagrange/Attribute.h>
#include <lagrange/AttributeValueType.h>
#include <lagrange/SurfaceMesh.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/compute_area.h>
#include <lagrange/compute_barycentric_coordinates.h>
#include <lagrange/sampling/DensityError.h>
#include <lagrange/sampling/DensityStatus.h>
#include <lagrange/sampling/api.h>
#include <lagrange/sampling/internal/HeapWithVector.h>
#include <lagrange/sampling/internal/get_density_status.h>
#include <lagrange/sampling/sample_blue_noise.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/safe_cast.h>
#include <lagrange/utils/span.h>
#include <lagrange/utils/tracy.h>
#include <lagrange/views.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <nanoflann.hpp>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <pcg_random.hpp>

#include <tbb/blocked_range.h>
#include <tbb/enumerable_thread_specific.h>
#include <tbb/parallel_for.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace lagrange::sampling {

namespace {

template <typename Scalar>
using Cloud = Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;

template <typename Scalar>
using KDTree =
    nanoflann::KDTreeEigenMatrixAdaptor<Cloud<Scalar>, Eigen::Dynamic, nanoflann::metric_L2>;

template <typename Scalar>
using NNPair = typename nanoflann::ResultItem<Eigen::Index, Scalar>;

template <typename Scalar>
using WeightEntry = std::pair<Eigen::Index, NNPair<Scalar>>;

/// CSR (Compressed Sparse Row) layout for neighbor weight data.
/// Stores all entries in a single flat array with per-target offsets.
template <typename Scalar>
struct WeightCSR
{
    std::vector<NNPair<Scalar>> data;
    std::vector<size_t> offsets; // offsets[i] = start, offsets[i+1] = end for target i

    span<const NNPair<Scalar>> neighbors(size_t target) const
    {
        const size_t begin = offsets[target];
        const size_t count = offsets[target + 1] - begin;
        if (count == 0) return {};
        return {data.data() + begin, count};
    }
};


/// Build a density callback from a per-facet attribute.
template <typename Scalar, typename Index>
auto make_facet_density_callback(const SurfaceMesh<Scalar, Index>& mesh, AttributeId attr_id)
{
    auto densities_view = attribute_vector_view<Scalar>(mesh, attr_id);
    return [densities_view](
               span<const Scalar> /*positions*/,
               span<const Index> facet_indices,
               span<Scalar> output_densities) {
        for (size_t i = 0; i < facet_indices.size(); ++i) {
            output_densities[i] = densities_view(facet_indices[i]);
        }
    };
}

/// Build a density callback from a per-vertex attribute, using barycentric interpolation.
template <typename Scalar, typename Index>
auto make_vertex_density_callback(const SurfaceMesh<Scalar, Index>& mesh, AttributeId attr_id)
{
    auto densities_view = attribute_vector_view<Scalar>(mesh, attr_id);
    auto vertices = vertex_view(mesh);
    auto triangles = facet_view(mesh);
    const auto dim = mesh.get_dimension();
    la_runtime_assert(dim == 2 || dim == 3, "Only 2D and 3D meshes are supported.");

    return [densities_view, vertices, triangles, dim](
               span<const Scalar> positions,
               span<const Index> facet_indices,
               span<Scalar> output_densities) {
        // Bounded dynamic vector: runtime size 2 or 3, max allocation known at compile time.
        using ColVector = Eigen::Matrix<Scalar, Eigen::Dynamic, 1, 0, 3, 1>;
        const size_t num_points = facet_indices.size();
        for (size_t i = 0; i < num_points; ++i) {
            const auto fi = facet_indices[i];
            const auto vi0 = triangles(fi, 0);
            const auto vi1 = triangles(fi, 1);
            const auto vi2 = triangles(fi, 2);

            const ColVector p0 = vertices.row(vi0).transpose();
            const ColVector p1 = vertices.row(vi1).transpose();
            const ColVector p2 = vertices.row(vi2).transpose();
            const Eigen::Map<const ColVector> p(positions.data() + i * dim, dim);
            const auto bary = compute_barycentric_coordinates(p0, p1, p2, ColVector(p));

            output_densities[i] = bary(0) * densities_view(vi0) + bary(1) * densities_view(vi1) +
                                  bary(2) * densities_view(vi2);
        }
    };
}

/// Resolve the density variant into a concrete callback function.
template <typename Scalar, typename Index>
auto resolve_density_callback(
    const SurfaceMesh<Scalar, Index>& mesh,
    const std::variant<
        std::monostate,
        std::string_view,
        typename BlueNoiseSamplingOptions<Scalar, Index>::DensityCallback>& density)
{
    using DensityCallback = typename BlueNoiseSamplingOptions<Scalar, Index>::DensityCallback;

    // Validate the density attribute up front, before entering the generic lambda.
    if (const auto* attr_name = std::get_if<std::string_view>(&density)) {
        la_runtime_assert(
            mesh.has_attribute(*attr_name),
            "Density attribute '" + std::string(*attr_name) + "' not found.");
        const auto attr_id = mesh.get_attribute_id(*attr_name);
        const auto& attr = mesh.get_attribute_base(attr_id);
        const auto element = attr.get_element_type();
        la_runtime_assert(
            element == AttributeElement::Facet || element == AttributeElement::Vertex,
            "Density attribute must be per-facet or per-vertex.");
        la_runtime_assert(
            attr.get_num_channels() == 1,
            "Density attribute must be a single-channel scalar attribute.");
        constexpr auto expected_type = lagrange::make_attribute_value_type<Scalar>();
        la_runtime_assert(
            attr.get_value_type() == expected_type,
            "Density attribute value type must match the mesh scalar type.");
    }

    return std::visit(
        [&](auto&& arg) -> DensityCallback {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::monostate>) {
                lagrange::logger().debug("No density specified, using uniform density.");
                return [](span<const Scalar> /*positions*/,
                          span<const Index> facet_indices,
                          span<Scalar> output_densities) {
                    for (size_t i = 0; i < facet_indices.size(); ++i) {
                        output_densities[i] = Scalar(1);
                    }
                };
            } else if constexpr (std::is_same_v<T, std::string_view>) {
                lagrange::logger().debug("Using attribute '{}' for density evaluation.", arg);
                const auto attr_id = mesh.get_attribute_id(arg);
                const auto& attr = mesh.get_attribute_base(attr_id);
                const auto element = attr.get_element_type();
                if (element == AttributeElement::Facet) {
                    return make_facet_density_callback(mesh, attr_id);
                } else {
                    return make_vertex_density_callback(mesh, attr_id);
                }
            } else {
                lagrange::logger().debug("Using user-provided density callback.");
                return arg;
            }
        },
        density);
}

/// Compute pairwise weights in parallel and build the elimination heap.
template <typename Scalar, typename Index>
void compute_weights(
    const Cloud<Scalar>& cloud,
    const Eigen::Matrix<Scalar, Eigen::Dynamic, 1>& inv_sqrt_targets,
    Scalar rmax,
    Index cloud_size,
    WeightCSR<Scalar>& weights,
    internal::HeapWithVector<Eigen::Index, Scalar>& heap)
{
    LAGRANGE_ZONE_SCOPED;
    using Neighbors = std::vector<NNPair<Scalar>>;
    using Range = tbb::blocked_range<Index>;
    using ThreadLocalEntries = tbb::enumerable_thread_specific<std::vector<WeightEntry<Scalar>>>;

    const auto dim = cloud.cols();
    // When density varies, the pairwise interaction radius d_max_ij = rmax * (ist_i + ist_j) / 2
    // can exceed rmax. Scale the search radius by the maximum inv_sqrt_target to capture all
    // neighbors that could contribute non-zero weight.
    const Scalar max_ist = inv_sqrt_targets.maxCoeff();
    const Scalar search_radius = rmax * max_ist;
    const auto search_radius_squared = search_radius * search_radius;
    const nanoflann::SearchParameters search_params{0, false};

    auto tree = std::make_unique<KDTree<Scalar>>(dim, cloud, 10);
    tree->index_->buildIndex();

    std::vector<Scalar> total_weights(cloud_size);
    heap.reserve(cloud_size);

    ThreadLocalEntries thread_local_entries;
    tbb::enumerable_thread_specific<Neighbors> thread_local_neighbors;

    tbb::parallel_for(Range(0, cloud_size, 256), [&](const Range& range) {
        auto& neighbors = thread_local_neighbors.local();
        if (neighbors.capacity() < 256) neighbors.reserve(256);
        auto& local_entries = thread_local_entries.local();

        for (Index kk = range.begin(); kk < range.end(); kk++) {
            const auto current = cloud.row(kk);
            const auto ist_kk = inv_sqrt_targets(kk);
            Scalar weight_accum = 0;

            tree->index_
                ->radiusSearch(current.data(), search_radius_squared, neighbors, search_params);

            for (const auto& neighbor : neighbors) {
                if (static_cast<Index>(neighbor.first) == kk) continue;

                // Symmetric weight: D_ij = rmax * (1/sqrt(t_i) + 1/sqrt(t_j)) / 2
                // w_ij = max(0, 1 - dist / D_ij)^8
                const auto dist = std::sqrt(neighbor.second);
                const auto d_max_ij =
                    rmax * (ist_kk + inv_sqrt_targets(neighbor.first)) * Scalar(0.5);
                const auto t = std::max(Scalar(0), Scalar(1) - dist / d_max_ij);
                if (t == Scalar(0)) continue;
                const auto t2 = t * t;
                const auto t4 = t2 * t2;
                const auto weight = t4 * t4; // t^8

                weight_accum += weight;
                local_entries.emplace_back(
                    neighbor.first,
                    NNPair<Scalar>{static_cast<Eigen::Index>(kk), weight});
            }

            total_weights[kk] = weight_accum;
        }
    });

    // Merge thread-local entries into a flat CSR array using counting sort.
    // Pass 1: Count entries per target.
    weights.offsets.assign(static_cast<size_t>(cloud_size) + 1, 0);
    for (auto& local : thread_local_entries) {
        for (const auto& [target, pair] : local) {
            weights.offsets[target + 1]++;
        }
    }

    // Compute prefix sums.
    for (Index kk = 0; kk < cloud_size; kk++) {
        weights.offsets[kk + 1] += weights.offsets[kk];
    }

    // Pass 2: Scatter entries into the flat array.
    const auto total_entries = weights.offsets[cloud_size];
    weights.data.resize(total_entries);
    std::vector<size_t> write_pos(weights.offsets.begin(), weights.offsets.end() - 1);
    for (auto& local : thread_local_entries) {
        for (auto& [target, pair] : local) {
            weights.data[write_pos[target]++] = pair;
        }
    }

    for (Index kk = 0; kk < cloud_size; kk++) {
        heap.insert_or_update(kk, total_weights[kk]);
    }
}

/// Greedily eliminate samples with highest weight until the desired count is reached.
template <typename Scalar>
void eliminate_samples(
    internal::HeapWithVector<Eigen::Index, Scalar>& heap,
    const WeightCSR<Scalar>& weights,
    std::vector<bool>& is_eliminated,
    size_t target_count)
{
    LAGRANGE_ZONE_SCOPED;
    const auto& min_values = heap.min_values();
    while (heap.size() > target_count) {
        const auto& current = heap.pop();

        for (const auto& neighbor : weights.neighbors(current.first)) {
            if (is_eliminated[neighbor.first]) continue;

            la_debug_assert(neighbor.first >= 0);
            la_debug_assert(static_cast<size_t>(neighbor.first) < min_values.size());
            const auto min_val = min_values[neighbor.first];
            la_debug_assert(min_val >= 0);

            heap.insert_or_update(neighbor.first, min_val - neighbor.second);
        }

        la_debug_assert(!is_eliminated[current.first]);
        is_eliminated[current.first] = true;

        heap.cleanup(4);
    }

    la_debug_assert(heap.size() == target_count);
}

/// Collect surviving samples into the result struct.
template <typename Scalar, typename Index>
BlueNoiseSamplingResult<Scalar, Index> collect_results(
    const Cloud<Scalar>& cloud,
    const Eigen::Matrix<Scalar, Eigen::Dynamic, 1>& densities,
    const Eigen::Matrix<Index, Eigen::Dynamic, 1>& facet_indices,
    const std::vector<bool>& is_eliminated,
    Index num_samples)
{
    LAGRANGE_ZONE_SCOPED;
    const auto dim = cloud.cols();

    BlueNoiseSamplingResult<Scalar, Index> result;
    result.samples.resize(num_samples * dim);
    result.densities.resize(num_samples);
    result.facet_indices.resize(num_samples);

    Index out = 0;
    for (Index i = 0; i < static_cast<Index>(is_eliminated.size()); ++i) {
        if (is_eliminated[i]) continue;
        la_debug_assert(out < num_samples);
        for (Eigen::Index d = 0; d < dim; ++d) {
            result.samples[out * dim + d] = cloud(i, d);
        }
        result.densities[out] = densities(i);
        result.facet_indices[out] = facet_indices(i);
        out++;
    }

    return result;
}

} // namespace

template <typename Scalar, typename Index>
BlueNoiseSamplingResult<Scalar, Index> sample_blue_noise(
    const SurfaceMesh<Scalar, Index>& mesh,
    const BlueNoiseSamplingOptions<Scalar, Index>& options)
{
    LAGRANGE_ZONE_SCOPED;

    la_runtime_assert(
        options.num_samples > 0,
        "num_samples must be set in BlueNoiseSamplingOptions.");
    la_runtime_assert(mesh.is_triangle_mesh(), "Only triangle meshes are supported.");

    const Index num_samples = safe_cast<Index>(options.num_samples);
    const auto dim = mesh.get_dimension();

    // Resolve density variant into a concrete callback.
    auto density_callback = resolve_density_callback(mesh, options.density);

    // Compute facet areas.
    auto mesh_copy = mesh;
    auto area_attr_id = compute_facet_area(mesh_copy);
    auto areas = attribute_vector_view<Scalar>(mesh_copy, area_attr_id);
    const Scalar total_area = areas.sum();
    la_runtime_assert(total_area > 0, "Mesh must have positive non-null area.");

    // Validate sampling parameters.
    la_runtime_assert(
        std::isfinite(options.oversample_ratio) && options.oversample_ratio >= 1.0,
        "oversample_ratio must be finite and >= 1.");
    la_runtime_assert(
        std::isfinite(options.rmax_factor) && options.rmax_factor > 0.0,
        "rmax_factor must be finite and positive.");
    la_runtime_assert(
        std::isfinite(options.bias) && options.bias >= 0.0 && options.bias <= 1.0,
        "bias must be finite and in the range [0, 1].");

    // Generate uniform oversampled point cloud.
    const Index cloud_size = safe_cast<Index>(std::ceil(options.oversample_ratio * num_samples));
    la_runtime_assert(cloud_size >= num_samples);

    pcg32 rng(options.seed);
    auto vertices = vertex_view(mesh);
    auto triangles = facet_view(mesh);

    Cloud<Scalar> cloud(cloud_size, dim);
    Eigen::Matrix<Index, Eigen::Dynamic, 1> cloud_facets(cloud_size);
    {
        Eigen::Map<Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>>
            sample_buffer(cloud.data(), cloud_size, dim);
        Eigen::Map<Eigen::Matrix<Index, Eigen::Dynamic, 1>> index_buffer(
            cloud_facets.data(),
            cloud_size);
        uniform_sample(
            vertices,
            triangles,
            areas,
            cloud_size,
            rng,
            sample_buffer,
            index_buffer,
            true);
    }

    // Evaluate and validate density.
    Eigen::Matrix<Scalar, Eigen::Dynamic, 1> cloud_densities(cloud_size);
    density_callback(
        {cloud.data(), static_cast<size_t>(cloud_size * dim)},
        {cloud_facets.data(), static_cast<size_t>(cloud_size)},
        {cloud_densities.data(), static_cast<size_t>(cloud_size)});

    const auto density_status = internal::get_density_status(cloud_densities);
    if (density_status != DensityStatus::Valid) {
        throw DensityError(density_status);
    }

    // Compute normalized target densities and per-sample inverse-sqrt for weight function.
    const auto targets =
        options.bias + (1 - options.bias) * cloud_densities.array() / cloud_densities.maxCoeff();

    Eigen::Matrix<Scalar, Eigen::Dynamic, 1> inv_sqrt_targets(cloud_size);
    for (Index i = 0; i < cloud_size; ++i) {
        inv_sqrt_targets(i) = Scalar(1) / std::sqrt(std::max(targets(i), Scalar(1e-8)));
    }

    const Scalar rmax =
        options.rmax_factor * std::sqrt(total_area / 2 / std::sqrt(3.0) / num_samples);

    // Compute pairwise weights and build elimination heap.
    WeightCSR<Scalar> weights;
    internal::HeapWithVector<Eigen::Index, Scalar> heap;
    compute_weights(cloud, inv_sqrt_targets, rmax, cloud_size, weights, heap);

    // Eliminate samples until the desired count is reached.
    std::vector<bool> is_eliminated(cloud_size, false);
    eliminate_samples(heap, weights, is_eliminated, num_samples);

    return collect_results<Scalar, Index>(
        cloud,
        cloud_densities,
        cloud_facets,
        is_eliminated,
        num_samples);
}

#define LA_X_sample_blue_noise(_, Scalar, Index)                                       \
    template LA_SAMPLING_API BlueNoiseSamplingResult<Scalar, Index> sample_blue_noise( \
        const SurfaceMesh<Scalar, Index>&,                                             \
        const BlueNoiseSamplingOptions<Scalar, Index>&);
LA_SURFACE_MESH_X(sample_blue_noise, 0)

} // namespace lagrange::sampling
