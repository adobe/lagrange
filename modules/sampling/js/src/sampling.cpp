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

#include "bind_types.h"

#include <lagrange/sampling/sample_blue_noise.h>
#include <lagrange/sampling/sample_epsilon.h>
#include <lagrange/sampling/sample_uniform.h>

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <string>

namespace {

using namespace lagrange;
using namespace lagrange::js::bind;
using val = emscripten::val;

} // namespace

EMSCRIPTEN_BINDINGS(lagrange_sampling)
{
    using namespace emscripten;

    function(
        "sampleUniform",
        +[](const MeshType& mesh, val opts) -> val {
            sampling::UniformSamplingOptions o;
            apply_opt(opts, "numSamples", o.num_samples);
            apply_opt(opts, "seed", o.seed);

            auto result = sampling::sample_uniform<Scalar, Index>(mesh, o);

            val out = val::object();
            out.set(
                "positions",
                val::global("Float64Array")
                    .new_(typed_memory_view(result.samples.size(), result.samples.data())));
            out.set(
                "facetIndices",
                val::global("Uint32Array")
                    .new_(typed_memory_view(
                        result.facet_indices.size(),
                        result.facet_indices.data())));
            out.set("numSamples", result.facet_indices.size());
            return out;
        });

    function(
        "sampleBlueNoise",
        +[](const MeshType& mesh, val opts) -> val {
            sampling::BlueNoiseSamplingOptions<Scalar, Index> o;
            apply_opt(opts, "numSamples", o.num_samples);
            apply_opt(opts, "seed", o.seed);
            apply_opt(opts, "bias", o.bias);
            apply_opt(opts, "oversampleRatio", o.oversample_ratio);
            apply_opt(opts, "rmaxFactor", o.rmax_factor);

            auto result = sampling::sample_blue_noise<Scalar, Index>(mesh, o);

            val out = val::object();
            out.set(
                "positions",
                val::global("Float64Array")
                    .new_(typed_memory_view(result.samples.size(), result.samples.data())));
            out.set(
                "facetIndices",
                val::global("Uint32Array")
                    .new_(typed_memory_view(
                        result.facet_indices.size(),
                        result.facet_indices.data())));
            out.set(
                "densities",
                val::global("Float64Array")
                    .new_(typed_memory_view(result.densities.size(), result.densities.data())));
            out.set("numSamples", result.facet_indices.size());
            return out;
        });

    function(
        "sampleEpsilon",
        +[](const MeshType& mesh, const std::string& lfsAttribute, Scalar epsilon, val opts)
            -> val {
            sampling::EpsilonSamplingOptions o;
            apply_opt(opts, "seed", o.seed);
            apply_opt(opts, "maxFailedAttempts", o.max_failed_attempts);
            apply_opt(opts, "computeBarycentric", o.compute_barycentric);
            apply_opt(opts, "ensureVertexCoverage", o.ensure_vertex_coverage);

            auto result = sampling::sample_epsilon<Scalar, Index>(mesh, lfsAttribute, epsilon, o);

            val out = val::object();
            out.set(
                "positions",
                val::global("Float64Array")
                    .new_(typed_memory_view(result.samples.size(), result.samples.data())));
            out.set(
                "facetIndices",
                val::global("Uint32Array")
                    .new_(typed_memory_view(
                        result.facet_indices.size(),
                        result.facet_indices.data())));
            out.set(
                "barycentric",
                val::global("Float64Array")
                    .new_(typed_memory_view(result.barycentric.size(), result.barycentric.data())));
            out.set("numSamples", result.facet_indices.size());
            return out;
        });
}
