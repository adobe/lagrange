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

#include "../shared/shared_utils.h"

#include <lagrange/Logger.h>
#include <lagrange/SurfaceMesh.h>
#include <lagrange/cast.h>
#include <lagrange/cast_attribute.h>
#include <lagrange/image/Array3D.h>
#include <lagrange/image/View3D.h>
#include <lagrange/python/binding.h>
#include <lagrange/python/tensor_utils.h>
#include <lagrange/texproc/TextureRasterizer.h>
#include <lagrange/texproc/geodesic_dilation.h>
#include <lagrange/texproc/texture_compositing.h>
#include <lagrange/texproc/texture_filtering.h>
#include <lagrange/texproc/texture_stitching.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/invalid.h>
#include <lagrange/utils/range.h>

#include <tbb/parallel_for.h>

#include <iostream>
#include <numeric>

namespace lagrange::python {

namespace tp = texproc;
namespace nb = nanobind;
using namespace nb::literals;

using TextureShape = nb::shape<-1, -1, -1>;

template <typename Scalar>
using TextureTensor = nb::ndarray<Scalar, TextureShape, nb::numpy, nb::c_contig, nb::device::cpu>;

static_assert(!std::is_same_v<TextureTensor<float>, Tensor<float>>);
static_assert(std::is_same_v<tp::Array3Df, image::experimental::Array3D<float>>);
static_assert(std::is_same_v<tp::View3Df, image::experimental::View3D<float>>);

tp::View3Df tensor_to_mdspan(const TextureTensor<float>& tensor)
{
    // Numpy indexes tensors as (row, col, channel), but our mdspan uses (x, y, channel)
    // coordinates, so we need to transpose the first two dimensions.
    const image::experimental::dextents<size_t, 3> shape{
        tensor.shape(1),
        tensor.shape(0),
        tensor.shape(2),
    };
    const std::array<size_t, 3> strides{
        static_cast<size_t>(tensor.stride(1)),
        static_cast<size_t>(tensor.stride(0)),
        static_cast<size_t>(tensor.stride(2)),
    };
    const image::experimental::layout_stride::mapping mapping{shape, strides};
    image::experimental::View3D<float> view{
        static_cast<float*>(tensor.data()),
        mapping,
    };
    return view;
}

nb::object mdarray_to_tensor(const tp::Array3Df& array_)
{
    // Numpy indexes tensors as (row, col, channel), but our mdspan uses (x, y, channel)
    // coordinates, so we need to transpose the first two dimensions.
    auto array = const_cast<tp::Array3Df&>(array_);
    auto tensor = Tensor<float>(
        static_cast<float*>(array.data()),
        {
            array.extent(1),
            array.extent(0),
            array.extent(2),
        },
        nb::handle(),
        {
            static_cast<int64_t>(array.stride(1)),
            static_cast<int64_t>(array.stride(0)),
            static_cast<int64_t>(array.stride(2)),
        });
    return tensor.cast();
}

void populate_texproc_module(nb::module_& m)
{
    using Scalar = double;
    using Index = uint32_t;

    m.def(
        "texture_filtering",
        [](const SurfaceMesh<Scalar, Index>& mesh,
           const TextureTensor<float>& image_,
           double value_weight,
           double gradient_weight,
           double gradient_scale,
           unsigned int quadrature_samples,
           double jitter_epsilon) {
            auto image = image::experimental::create_image<float>(
                image_.shape(0),
                image_.shape(1),
                image_.shape(2));
            std::copy(image_.data(), image_.data() + image_.size(), image.data());

            tp::FilteringOptions options;
            options.value_weight = value_weight;
            options.gradient_weight = gradient_weight;
            options.gradient_scale = gradient_scale;
            options.quadrature_samples = quadrature_samples;
            options.jitter_epsilon = jitter_epsilon;

            tp::texture_filtering(mesh, image.to_mdspan(), options);

            return mdarray_to_tensor(image);
        },
        "mesh"_a,
        "image"_a,
        "value_weight"_a = tp::FilteringOptions().value_weight,
        "gradient_weight"_a = tp::FilteringOptions().gradient_weight,
        "gradient_scale"_a = tp::FilteringOptions().gradient_scale,
        "quadrature_samples"_a = tp::FilteringOptions().quadrature_samples,
        "jitter_epsilon"_a = tp::FilteringOptions().jitter_epsilon,
        R"("Smooth or sharpen a texture image associated with a mesh.

:param mesh: Input mesh with UV attributes.
:param image: Texture image to filter.
:param value_weight: The weight for fitting the values of the signal.
:param gradient_weight: The weight for fitting the modulated gradients of the signal.
:param gradient_scale: The gradient modulation weight. Use a value of 0 for smoothing, and use a value between [2, 10] for sharpening.
:param quadrature_samples: The number of quadrature samples to use for integration (in {1, 3, 6, 12, 24, 32}).
:param jitter_epsilon: Jitter amount per texel (0 to deactivate).

:return: The filtered texture image.)");

    m.def(
        "texture_stitching",
        [](const SurfaceMesh<Scalar, Index>& mesh,
           const TextureTensor<float>& image_,
           bool exterior_only,
           unsigned int quadrature_samples,
           double jitter_epsilon) {
            auto image = image::experimental::create_image<float>(
                image_.shape(0),
                image_.shape(1),
                image_.shape(2));
            std::copy(image_.data(), image_.data() + image_.size(), image.data());

            tp::StitchingOptions options;
            options.exterior_only = exterior_only;
            options.quadrature_samples = quadrature_samples;
            options.jitter_epsilon = jitter_epsilon;

            tp::texture_stitching(mesh, image.to_mdspan(), options);

            return mdarray_to_tensor(image);
        },
        "mesh"_a,
        "image"_a,
        "exterior_only"_a = tp::StitchingOptions().exterior_only,
        "quadrature_samples"_a = tp::StitchingOptions().quadrature_samples,
        "jitter_epsilon"_a = tp::StitchingOptions().jitter_epsilon,
        R"(Smooth or sharpen a texture image associated with a mesh.

:param mesh: Input mesh with UV attributes.
:param image: Texture image to stitch.
:param exterior_only: If true, interior texels are fixed degrees of freedom.
:param quadrature_samples: The number of quadrature samples to use for integration (in {1, 3, 6, 12, 24, 32}).
:param jitter_epsilon: Jitter amount per texel (0 to deactivate).

:return: The stitched texture image.)");

    m.def(
        "geodesic_dilation",
        [](const SurfaceMesh<Scalar, Index>& mesh,
           const TextureTensor<float>& image_,
           float dilation_radius) {
            tp::DilationOptions options;
            options.dilation_radius = dilation_radius;

            auto image = image::experimental::create_image<float>(
                image_.shape(0),
                image_.shape(1),
                image_.shape(2));
            std::copy(image_.data(), image_.data() + image_.size(), image.data());

            tp::geodesic_dilation(mesh, image.to_mdspan(), options);

            return mdarray_to_tensor(image);
        },
        "mesh"_a,
        "image"_a,
        "dilation_radius"_a = tp::DilationOptions().dilation_radius,
        R"(Extend pixels of a texture beyond the defined UV mesh by walking along the 3D surface.

:param mesh: Input mesh with UV attributes.
:param image: Texture to extend beyond UV mesh boundaries.
:param dilation_radius: The radius by which the texture should be dilated into the gutter.

:return: The dilated texture image.)");

    m.def(
        "geodesic_position",
        [](const SurfaceMesh<Scalar, Index>& mesh,
           unsigned int width,
           unsigned int height,
           float dilation_radius) {
            tp::DilationOptions options;
            options.dilation_radius = dilation_radius;
            options.output_position_map = true;

            auto image = image::experimental::create_image<float>(width, height, 3);
            std::iota(
                image.data(),
                image.data() + image.size(),
                std::numeric_limits<float>::infinity());

            tp::geodesic_dilation(mesh, image.to_mdspan(), options);

            return mdarray_to_tensor(image);
        },
        "mesh"_a,
        "width"_a,
        "height"_a,
        "dilation_radius"_a = tp::DilationOptions().dilation_radius,
        R"(Computes a dilated position map to extend a texture beyond the defined UV mesh by walking along the 3D surface.

:param mesh: Input mesh with UV attributes.
:param width: Width of the output position map.
:param height: Height of the output position map.
:param dilation_radius: The radius by which the texture should be dilated into the gutter.

:return: The dilated position map.)");

    m.def(
        "texture_compositing",
        [](const SurfaceMesh<Scalar, Index>& mesh,
           const std::vector<TextureTensor<float>>& textures,
           const std::vector<TextureTensor<float>>& weights,
           double value_weight,
           unsigned int quadrature_samples,
           double jitter_epsilon,
           bool smooth_low_weight_areas,
           unsigned int num_multigrid_levels,
           unsigned int num_gauss_seidel_iterations,
           unsigned int num_v_cycles) {
            la_runtime_assert(
                textures.size() == weights.size(),
                "Number of colors and weights images must be the same.");

            std::vector<tp::ConstWeightedTextureView<float>> weighted_textures;
            for (const auto kk : range(textures.size())) {
                const tp::View3Df texture = tensor_to_mdspan(textures[kk]);
                const tp::View3Df weight = tensor_to_mdspan(weights[kk]);
                weighted_textures.emplace_back(
                    tp::ConstWeightedTextureView<float>{
                        texture,
                        weight,
                    });
            }

            tp::CompositingOptions options;
            options.value_weight = value_weight;
            options.quadrature_samples = quadrature_samples;
            options.jitter_epsilon = jitter_epsilon;
            options.smooth_low_weight_areas = smooth_low_weight_areas;
            options.solver.num_multigrid_levels = num_multigrid_levels;
            options.solver.num_gauss_seidel_iterations = num_gauss_seidel_iterations;
            options.solver.num_v_cycles = num_v_cycles;

            auto image = tp::texture_compositing(mesh, weighted_textures, options);

            return mdarray_to_tensor(image);
        },
        "mesh"_a,
        "colors"_a,
        "weights"_a,
        "value_weight"_a = tp::CompositingOptions().value_weight,
        "quadrature_samples"_a = tp::CompositingOptions().quadrature_samples,
        "jitter_epsilon"_a = tp::CompositingOptions().jitter_epsilon,
        "smooth_low_weight_areas"_a = tp::CompositingOptions().smooth_low_weight_areas,
        "num_multigrid_levels"_a = tp::CompositingOptions().solver.num_multigrid_levels,
        "num_gauss_seidel_iterations"_a =
            tp::CompositingOptions().solver.num_gauss_seidel_iterations,
        "num_v_cycles"_a = tp::CompositingOptions().solver.num_v_cycles,
        R"(Composite multiple (color, weight) into a single texture given a unwrapped mesh.

:param mesh: Input mesh with UV attributes.
:param colors: List of texture images to composite. Input textures must have the same dimensions.
:param weights: List of confidence weights for each texel. 0 means the texel should be ignored, 1 means the texel should be fully trusted. Input weights must have the same dimensions as colors.
:param value_weight: The weight for fitting the values of the signal.
:param quadrature_samples: The number of quadrature samples to use for integration (in {1, 3, 6, 12, 24, 32}).
:param jitter_epsilon: Jitter amount per texel (0 to deactivate).
:param smooth_low_weight_areas: Whether to smooth pixels with a low total weight (< 1). When enabled, this will not dampen the gradient terms for pixels with a low total weight, resulting in a smoother texture in low-confidence areas.
:param num_multigrid_levels: Number of multigrid levels.
:param num_gauss_seidel_iterations: Number of Gauss-Seidel iterations per multigrid level.
:param num_v_cycles: Number of V-cycles to perform.

:return: The composited texture image.)");

    m.def(
        "rasterize_textures_from_renders",
        [](const scene::Scene<Scalar, Index>& scene,
           const std::vector<TextureTensor<float>>& renders,
           const std::optional<size_t> width,
           const std::optional<size_t> height,
           const float low_confidence_ratio,
           const std::optional<float> base_confidence) {
            std::vector<tp::View3Df> views;
            for (const auto& render : renders) {
                views.push_back(tensor_to_mdspan(render));
            }

            auto textures_and_weights = tp::rasterize_textures_from_renders(
                scene,
                std::nullopt,
                views,
                width,
                height,
                low_confidence_ratio,
                base_confidence);

            std::vector<nb::object> textures;
            std::vector<nb::object> weights;
            for (auto& [texture_, weight_] : textures_and_weights) {
                auto texture = mdarray_to_tensor(texture_);
                auto weight = mdarray_to_tensor(weight_);
                textures.emplace_back(texture);
                weights.emplace_back(weight);
            }

            return std::make_tuple(textures, weights);
        },
        "scene"_a,
        "renders"_a,
        "width"_a = nb::none(),
        "height"_a = nb::none(),
        "low_confidence_ratio"_a = 0.75,
        "base_confidence"_a = nb::none(),
        R"(Rasterize one (color, weight) per (render, camera) and filter our low-confidence weights.

:param scene: Scene containing a single mesh (possibly with a base texture), and multiple cameras.
:param renders: List of rendered images, one per camera.
:param width: Width of the rasterized textures. Must match the width of the base texture if present. Otherwise, defaults to 1024.
:param height: Height of the rasterized textures. Must match the height of the base texture if present. Otherwise, defaults to 1024.
:param low_confidence_ratio: Discard low confidence texels whose weights are < ratio * max_weight.
:param base_confidence: Confidence value for the base texture if present in the scene. If set to 0, ignore the base texture of the mesh. Defaults to 0.3 otherwise.

:return: A pair of lists (textures, weights), one per camera.)");
}

} // namespace lagrange::python
