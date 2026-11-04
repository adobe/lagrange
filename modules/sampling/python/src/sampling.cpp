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

#include <lagrange/python/binding.h>

#include <lagrange/sampling/DensityError.h>
#include <lagrange/sampling/DensityStatus.h>
#include <lagrange/sampling/sample_blue_noise.h>
#include <lagrange/sampling/sample_epsilon.h>
#include <lagrange/sampling/sample_uniform.h>


#include <Eigen/Core>

#include <algorithm>
#include <optional>
#include <string>

namespace lagrange::python {

namespace nb = nanobind;
using namespace nb::literals;

void populate_sampling_module(nb::module_& m)
{
    using Scalar = double;
    using Index = uint32_t;

    nb::exception<sampling::DensityError>(m, "DensityError");

    auto warn_deprecated = [](const char* old_name, const char* new_name) {
        std::string msg =
            std::string("`") + old_name + "` is deprecated; use `" + new_name + "` instead.";
        if (PyErr_WarnEx(PyExc_DeprecationWarning, msg.c_str(), 1) < 0) {
            throw nb::python_error();
        }
    };

    auto impl_sample_uniform = [](const SurfaceMesh<Scalar, Index>& mesh,
                                  size_t num_samples,
                                  size_t seed) {
        sampling::UniformSamplingOptions options;
        options.seed = seed;
        options.num_samples = num_samples;
        auto result = sampling::sample_uniform(mesh, options);

        const Index dim = mesh.get_dimension();
        Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> samples(
            num_samples,
            dim);
        std::copy(result.samples.begin(), result.samples.end(), samples.data());

        Eigen::Matrix<Index, Eigen::Dynamic, 1> facet_indices(num_samples);
        std::copy(result.facet_indices.begin(), result.facet_indices.end(), facet_indices.data());

        return std::make_pair(std::move(samples), std::move(facet_indices));
    };
    m.def(
        "sample_uniform",
        impl_sample_uniform,
        "mesh"_a,
        "num_samples"_a,
        "seed"_a = 0,
        R"(Compute uniform random samples on a mesh.

:param mesh: Input mesh
:param num_samples: Number of samples
:param seed: Random seed

:return: A pair of (samples, facet_indices) where `samples` is a matrix of sampled points, and `facet_indices` is a vector of the source facets of the samples.)");
    m.def(
        "uniform_sample",
        [impl_sample_uniform,
         warn_deprecated](const SurfaceMesh<Scalar, Index>& mesh, size_t num_samples, size_t seed) {
            warn_deprecated("uniform_sample", "sample_uniform");
            return impl_sample_uniform(mesh, num_samples, seed);
        },
        "mesh"_a,
        "num_samples"_a,
        "seed"_a = 0,
        "Deprecated alias for :func:`sample_uniform`.");

    using BlueNoiseSamplingOptions = sampling::BlueNoiseSamplingOptions<Scalar, Index>;
    auto impl_sample_blue_noise = [](const SurfaceMesh<Scalar, Index>& mesh,
                                     size_t num_samples,
                                     size_t seed,
                                     double bias,
                                     double oversample_ratio,
                                     double rmax_factor,
                                     std::optional<std::string_view> density_attribute) {
        BlueNoiseSamplingOptions options;
        options.num_samples = num_samples;
        options.seed = seed;
        options.bias = bias;
        options.oversample_ratio = oversample_ratio;
        options.rmax_factor = rmax_factor;
        if (density_attribute.has_value()) {
            options.density = density_attribute.value();
        }

        auto result = sampling::sample_blue_noise(mesh, options);

        const Index dim = mesh.get_dimension();
        Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> samples(
            result.facet_indices.size(),
            dim);
        std::copy(result.samples.begin(), result.samples.end(), samples.data());

        Eigen::Matrix<Index, Eigen::Dynamic, 1> facet_indices(result.facet_indices.size());
        std::copy(result.facet_indices.begin(), result.facet_indices.end(), facet_indices.data());

        Eigen::Matrix<Scalar, Eigen::Dynamic, 1> densities(result.densities.size());
        std::copy(result.densities.begin(), result.densities.end(), densities.data());

        return std::make_tuple(std::move(samples), std::move(facet_indices), std::move(densities));
    };
    m.def(
        "sample_blue_noise",
        impl_sample_blue_noise,
        "mesh"_a,
        "num_samples"_a,
        "seed"_a = BlueNoiseSamplingOptions().seed,
        "bias"_a = BlueNoiseSamplingOptions().bias,
        "oversample_ratio"_a = BlueNoiseSamplingOptions().oversample_ratio,
        "rmax_factor"_a = BlueNoiseSamplingOptions().rmax_factor,
        "density_attribute"_a = nb::none(),
        R"(Compute blue noise (Poisson disk) samples on a mesh.

:param mesh: Input triangle mesh
:param num_samples: Number of desired output samples
:param seed: Random seed
:param bias: Bias between 0 (prescribed density) and 1 (uniform density)
:param oversample_ratio: Oversampling ratio (increase for better quality)
:param rmax_factor: Neighborhood size ratio (increase for better quality)
:param density_attribute: Optional attribute name for per-facet or per-vertex density

:raises DensityError: If the density contains invalid values (non-finite, negative, or zero sum).

:return: A tuple of (samples, facet_indices, densities) where `samples` is a matrix of sampled points,
    `facet_indices` is a vector of the source facets,
    and `densities` is a vector of density values at each sample.)");
    m.def(
        "blue_noise_sample",
        [impl_sample_blue_noise, warn_deprecated](
            const SurfaceMesh<Scalar, Index>& mesh,
            size_t num_samples,
            size_t seed,
            double bias,
            double oversample_ratio,
            double rmax_factor,
            std::optional<std::string_view> density_attribute) {
            warn_deprecated("blue_noise_sample", "sample_blue_noise");
            return impl_sample_blue_noise(
                mesh,
                num_samples,
                seed,
                bias,
                oversample_ratio,
                rmax_factor,
                density_attribute);
        },
        "mesh"_a,
        "num_samples"_a,
        "seed"_a = BlueNoiseSamplingOptions().seed,
        "bias"_a = BlueNoiseSamplingOptions().bias,
        "oversample_ratio"_a = BlueNoiseSamplingOptions().oversample_ratio,
        "rmax_factor"_a = BlueNoiseSamplingOptions().rmax_factor,
        "density_attribute"_a = nb::none(),
        "Deprecated alias for :func:`sample_blue_noise`.");

    using EpsilonSamplingOptions = sampling::EpsilonSamplingOptions;
    auto impl_sample_epsilon = [](const SurfaceMesh<Scalar, Index>& mesh,
                                  std::string_view lfs_attribute,
                                  Scalar epsilon,
                                  size_t seed,
                                  size_t max_failed_attempts,
                                  bool compute_barycentric,
                                  bool ensure_vertex_coverage) {
        EpsilonSamplingOptions options;
        options.seed = seed;
        options.max_failed_attempts = max_failed_attempts;
        options.compute_barycentric = compute_barycentric;
        options.ensure_vertex_coverage = ensure_vertex_coverage;

        auto result = sampling::sample_epsilon(mesh, lfs_attribute, epsilon, options);

        const Index dim = mesh.get_dimension();
        const size_t num_samples = result.facet_indices.size();
        Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> samples(
            num_samples,
            dim);
        std::copy(result.samples.begin(), result.samples.end(), samples.data());

        Eigen::Matrix<Index, Eigen::Dynamic, 1> facet_indices(num_samples);
        std::copy(result.facet_indices.begin(), result.facet_indices.end(), facet_indices.data());

        Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> barycentric(
            result.barycentric.size() / 3,
            3);
        std::copy(result.barycentric.begin(), result.barycentric.end(), barycentric.data());

        return std::make_tuple(
            std::move(samples),
            std::move(facet_indices),
            std::move(barycentric));
    };
    m.def(
        "sample_epsilon",
        impl_sample_epsilon,
        "mesh"_a,
        "lfs_attribute"_a,
        "epsilon"_a,
        "seed"_a = EpsilonSamplingOptions().seed,
        "max_failed_attempts"_a = EpsilonSamplingOptions().max_failed_attempts,
        "compute_barycentric"_a = EpsilonSamplingOptions().compute_barycentric,
        "ensure_vertex_coverage"_a = EpsilonSamplingOptions().ensure_vertex_coverage,
        R"(Compute an epsilon sampling of a triangle mesh graded by a local feature size field.

Sampling aims for every surface point to lie within `epsilon * lfs` of some sample, where `lfs` is
the local feature size interpolated from the given per-vertex attribute. Sampling terminates after
`max_failed_attempts` consecutive rejected darts, so coverage is probabilistic, not strict; pass a
larger `max_failed_attempts` or set `ensure_vertex_coverage=True` for more complete coverage.
Sampling is denser where the feature size is small. The total number of samples is not specified;
it emerges from `epsilon` and the feature size field.

:param mesh: Input triangle mesh
:param lfs_attribute: Name of a per-vertex, single-channel, strictly-positive scalar attribute holding the local feature size
:param epsilon: Sampling density constant (strictly positive)
:param seed: Random seed
:param max_failed_attempts: Number of consecutive rejected darts before termination
:param compute_barycentric: Whether to also compute barycentric coordinates for each sample
:param ensure_vertex_coverage: Deterministically add any mesh vertex not covered within epsilon * lfs as a sample

:return: A tuple of (samples, facet_indices, barycentric) where `samples` is a matrix of sampled points,
    `facet_indices` is a vector of the source facets of the samples, and `barycentric` is a matrix of
    barycentric coordinates (one row per sample). `barycentric` is empty unless `compute_barycentric` is True.)");
}

} // namespace lagrange::python
