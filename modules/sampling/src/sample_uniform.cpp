/*
 * Copyright 2023 Adobe. All rights reserved.
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

#include <lagrange/SurfaceMesh.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/compute_area.h>
#include <lagrange/sampling/api.h>
#include <lagrange/sampling/sample_uniform.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/safe_cast.h>
#include <lagrange/utils/span.h>
#include <lagrange/utils/tracy.h>
#include <lagrange/utils/warning.h>
#include <lagrange/views.h>

#include <pcg_random.hpp>

namespace lagrange::sampling {

// Suppress deprecation warnings for the legacy overload implementation and internal calls.
LA_IGNORE_DEPRECATION_WARNING_BEGIN

template <typename Scalar, typename Index>
void sample_uniform(
    const SurfaceMesh<Scalar, Index>& mesh,
    const size_t num_samples,
    span<Scalar> sample_positions,
    span<Index> sample_facet_indices,
    const UniformSamplingOptions& options)
{
    LAGRANGE_ZONE_SCOPED;
    la_runtime_assert(
        num_samples == options.num_samples || options.num_samples == 0,
        "num_samples in UniformSamplingOptions must be either 0 or equal to the num_samples "
        "argument.");
    la_runtime_assert(
        num_samples * static_cast<size_t>(mesh.get_dimension()) == sample_positions.size());
    bool compute_facet_indices = sample_facet_indices.size() > 0;
    la_runtime_assert(
        !compute_facet_indices || num_samples == safe_cast<Index>(sample_facet_indices.size()));
    la_runtime_assert(mesh.is_triangle_mesh(), "Only triangle mesh is supported for now.");

    pcg32 rng(options.seed);

    auto mesh_copy = mesh;
    auto area_attr_id = compute_facet_area(mesh_copy);
    auto areas = attribute_vector_view<Scalar>(mesh_copy, area_attr_id);

    auto vertices = vertex_view(mesh);
    auto triangles = facet_view(mesh);
    Eigen::Map<Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>>
        sample_buffer(sample_positions.data(), num_samples, mesh.get_dimension());
    if (compute_facet_indices) {
        Eigen::Map<Eigen::Matrix<Index, Eigen::Dynamic, 1>> index_buffer(
            sample_facet_indices.data(),
            num_samples);
        uniform_sample(
            vertices,
            triangles,
            areas,
            num_samples,
            rng,
            sample_buffer,
            index_buffer,
            true);
    } else {
        Eigen::Matrix<Index, Eigen::Dynamic, 1> index_buffer;
        uniform_sample(
            vertices,
            triangles,
            areas,
            num_samples,
            rng,
            sample_buffer,
            index_buffer,
            false);
    }
}

template <typename Scalar, typename Index>
UniformSamplingResult<Scalar, Index> sample_uniform(
    const SurfaceMesh<Scalar, Index>& mesh,
    const UniformSamplingOptions& options)
{
    LAGRANGE_ZONE_SCOPED;
    la_runtime_assert(
        options.num_samples > 0,
        "num_samples must be set in UniformSamplingOptions.");
    const size_t num_samples = options.num_samples;
    UniformSamplingResult<Scalar, Index> result;
    result.samples.resize(num_samples * static_cast<size_t>(mesh.get_dimension()));
    result.facet_indices.resize(num_samples);
    sample_uniform(
        mesh,
        num_samples,
        span<Scalar>(result.samples),
        span<Index>(result.facet_indices),
        options);
    return result;
}

#define LA_X_sample_uniform(_, Scalar, Index)                                     \
    template LA_SAMPLING_API void sample_uniform(                                 \
        const SurfaceMesh<Scalar, Index>&,                                        \
        size_t,                                                                   \
        span<Scalar>,                                                             \
        span<Index>,                                                              \
        const UniformSamplingOptions&);                                           \
    template LA_SAMPLING_API UniformSamplingResult<Scalar, Index> sample_uniform( \
        const SurfaceMesh<Scalar, Index>&,                                        \
        const UniformSamplingOptions&);
LA_SURFACE_MESH_X(sample_uniform, 0)

LA_IGNORE_DEPRECATION_WARNING_END


} // namespace lagrange::sampling
