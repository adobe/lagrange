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

#include <lagrange/legacy/inline.h>
#include <lagrange/sampling/internal/HeapWithVector.h>
#include <lagrange/sampling/internal/get_density_status.h>
#include <lagrange/sampling/random_sample_uniform.h>

#include <lagrange/Mesh.h>
#include <lagrange/utils/assert.h>

#include <tbb/blocked_range.h>
#include <tbb/concurrent_vector.h>
#include <tbb/parallel_for.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <nanoflann.hpp>
#include <lagrange/utils/warnon.h>
// clang-format on

namespace lagrange {
namespace sampling {
LAGRANGE_LEGACY_INLINE
namespace legacy {

template <typename MeshType, typename DensityCallback, typename Rng>
auto random_sample_density_blue_noise(
    const MeshType& input,
    const typename MeshType::Index number_of_sample,
    DensityCallback density_callback,
    Rng&& rng,
    const double bias,
    const double oversample_ratio,
    const double rmax_factor)
    -> std::tuple<
        DensityStatus,
        typename MeshType::VertexArray::PlainObject,
        typename MeshType::AttributeArray,
        Eigen::Matrix<typename MeshType::Index, Eigen::Dynamic, 1>>
{
    constexpr double alpha = 8;

    static_assert(lagrange::MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");

    using std::get;

    using Cloud = typename MeshType::VertexArray::PlainObject;
    using Densities = typename MeshType::AttributeArray;
    using Indices = Eigen::Matrix<typename MeshType::Index, Eigen::Dynamic, 1>;
    using Tree = nanoflann::KDTreeEigenMatrixAdaptor<Cloud, Eigen::Dynamic, nanoflann::metric_L2>;

    using Scalar = typename MeshType::Scalar;
    using Index = typename MeshType::Index;
    using Heap = sampling::internal::HeapWithVector<Eigen::Index, Scalar>;
    using Pair = typename nanoflann::ResultItem<Eigen::Index, Scalar>;
    using SafePairs = tbb::concurrent_vector<Pair>;
    using SafeWeights = std::vector<SafePairs>;

    la_runtime_assert(number_of_sample >= 0);
    la_runtime_assert(alpha > 0);
    la_runtime_assert(input.get_vertex_per_facet() == 3); // only works on triangular mesh
    la_runtime_assert(input.has_facet_attribute("area")); // need triangle areas to proceed

    const auto cloud_size_oversample = static_cast<Index>(oversample_ratio * number_of_sample);
    la_runtime_assert(cloud_size_oversample >= number_of_sample);

    logger().debug("uniform sampling");
    const auto uniform_sampling =
        lagrange::sampling::random_sample_uniform(input, cloud_size_oversample, rng, true);
    const auto& uniform_cloud = get<0>(uniform_sampling);
    const auto& uniform_indices = get<1>(uniform_sampling);
    assert(safe_cast<Index>(uniform_cloud.rows()) == cloud_size_oversample);
    assert(safe_cast<Index>(uniform_cloud.cols()) == input.get_dim());
    assert(safe_cast<Index>(uniform_indices.rows()) == cloud_size_oversample);
    assert(uniform_indices.cols() == 1);

    logger().debug("density callback");
    const Densities& uniform_densities = density_callback(uniform_cloud, uniform_indices);
    la_runtime_assert(safe_cast<Index>(uniform_densities.rows()) == cloud_size_oversample);
    la_runtime_assert(uniform_densities.cols() == 1);

    const auto uniform_densities_status = internal::get_density_status(uniform_densities);
    if (uniform_densities_status != DensityStatus::Valid) {
        return std::make_tuple(uniform_densities_status, Cloud(), Densities(), Indices());
    }

    const auto uniform_targets =
        bias + (1 - bias) * uniform_densities.array() / uniform_densities.maxCoeff();

    std::unique_ptr<Tree> cloud_tree = nullptr;
    {
        cloud_tree = std::make_unique<Tree>(safe_cast<int>(input.get_dim()), uniform_cloud, 10);
        cloud_tree->index_->buildIndex();
    }

    const auto rmax = [&]() -> auto {
        const auto& areas = input.get_facet_attribute("area");
        const auto total_area = areas.sum();
        const auto rmax_ = rmax_factor * sqrt(total_area / 2 / sqrt(3) / number_of_sample);
        return rmax_;
    }();

    SafeWeights weights;
    Heap heap;
    logger().debug("weights and heap construction");
    { // weights and heap construction
        using Neighbors = std::vector<Pair>;
        using TotalWeights = std::vector<Scalar>;

        using Range = tbb::blocked_range<Index>;

        const auto rmax_squared = rmax * rmax;
        const nanoflann::SearchParameters cloud_tree_params{0, false};

        TotalWeights total_weights;
        total_weights.resize(cloud_size_oversample);
        weights.resize(cloud_size_oversample);
        heap.reserve(cloud_size_oversample);

        const auto loop = [&](const Range& range_local) -> void {
            Neighbors neighbors;
            neighbors.reserve(256);

            for (Index kk = range_local.begin(); kk < range_local.end(); kk++) {
                const auto current = uniform_cloud.row(kk);
                const auto target = uniform_targets(kk);

                Scalar weight_accum = 0;

#if !defined(NDEBUG)
                const auto number_of_neighbors = cloud_tree->index_->radiusSearch(
                    current.data(),
                    rmax_squared,
                    neighbors,
                    cloud_tree_params);
                assert(number_of_neighbors == neighbors.size());
#else
                cloud_tree->index_
                    ->radiusSearch(current.data(), rmax_squared, neighbors, cloud_tree_params);
#endif

                for (const auto& neighbor : neighbors) {
                    if ((Index)neighbor.first == kk) {
                        continue;
                    }

#if !defined(NDEBUG)
                    const auto delta = current - uniform_cloud.row(neighbor.first);
                    assert(std::abs(neighbor.second - delta.squaredNorm()) < 1e-9);
#endif

                    const auto weight = pow(1 - target * sqrt(neighbor.second) / rmax, alpha);
                    assert(neighbor.second <= rmax_squared);
                    assert(weight >= 0);
                    assert(weight <= 1);

                    weight_accum += weight;
                    weights[neighbor.first].emplace_back(
                        kk,
                        weight); // tbb::concurrent_vector<>::emplace_back is safe
                }

                total_weights[kk] = weight_accum; // does not require protection
            }
        };

        const Range range_full(0, safe_cast<Index>(uniform_cloud.rows()), 256);
        tbb::parallel_for(range_full, loop);

        for (Index kk = 0, ee = static_cast<Index>(total_weights.size()); kk < ee; kk++) {
            heap.insert_or_update(kk, total_weights[kk]);
        }

        assert(heap.heap_size() == heap.size());
        assert(weights.size() == static_cast<size_t>(uniform_cloud.rows()));
        assert(heap.size() == static_cast<size_t>(uniform_cloud.rows()));
    }

    logger().debug("sample elimination");
    std::vector<bool> is_popped(cloud_size_oversample, false);
    { // sample elimination
        const auto& min_values = heap.min_values();
        while (heap.size() > static_cast<size_t>(number_of_sample)) {
            const auto& current = heap.pop();

            for (const auto& neighbor : weights[current.first]) {
                if (is_popped[neighbor.first]) {
                    continue;
                }

                assert(neighbor.first >= 0);
                assert(static_cast<size_t>(neighbor.first) < min_values.size());
                const auto min_val = min_values[neighbor.first];
                assert(min_val > 0);

                const auto weight_ = min_val - neighbor.second;
                heap.insert_or_update(neighbor.first, weight_);
            }

            assert(is_popped[current.first] == false);
            is_popped[current.first] = true;

            heap.cleanup(2);
        }

        assert(heap.size() == static_cast<size_t>(number_of_sample));
    }

    logger().debug("cleanup");
    Cloud decimated_cloud(number_of_sample, input.get_dim());
    Densities decimated_densities(number_of_sample, 1);
    Indices decimated_indices(number_of_sample, 1);
    Index kk = 0;
    for (Index i = 0; i < cloud_size_oversample; ++i) {
        if (is_popped[i]) {
            continue;
        }
        assert(kk < number_of_sample);
        decimated_cloud.row(kk) = uniform_cloud.row(i);
        decimated_densities.row(kk) = uniform_densities.row(i);
        decimated_indices.row(kk) = uniform_indices.row(i);
        kk++;
    }

    logger().debug("return");
    return std::make_tuple(
        DensityStatus::Valid,
        std::move(decimated_cloud),
        std::move(decimated_densities),
        std::move(decimated_indices));
}

} // namespace legacy
} // namespace sampling
} // namespace lagrange
