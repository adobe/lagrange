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
#include <lagrange/python/image_utils.h>
#include <lagrange/python/tensor_utils.h>
#include <lagrange/texproc/TextureRasterizer.h>
#include <lagrange/texproc/extract_mesh_with_alpha_mask.h>
#include <lagrange/texproc/geodesic_dilation.h>
#include <lagrange/texproc/texture_compositing.h>
#include <lagrange/texproc/texture_filtering.h>
#include <lagrange/texproc/texture_stitching.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/invalid.h>
#include <lagrange/utils/range.h>

#include <tbb/parallel_for.h>

#include <numeric>

namespace lagrange::python {

namespace tp = texproc;
namespace nb = nanobind;
using namespace nb::literals;

void populate_texproc_module(nb::module_& m)
{
    using Scalar = double;
    using Index = uint32_t;

    m.def(
        "texture_filtering",
        [](const SurfaceMesh<Scalar, Index>& mesh,
           const ImageTensor<float>& image_,
           double value_weight,
           double gradient_weight,
           double gradient_scale,
           unsigned int quadrature_samples,
           double jitter_epsilon,
           double stiffness_regularization_weight,
           std::optional<std::pair<double, double>> clamp_to_range,
           std::optional<bool> sanity_check) {
            auto image = image::experimental::create_image<float>(
                image_.shape(0),
                image_.shape(1),
                image_.shape(2));
            copy_tensor_to_image_view(image_, image.to_mdspan());

            tp::FilteringOptions options;
            options.value_weight = value_weight;
            options.gradient_weight = gradient_weight;
            options.gradient_scale = gradient_scale;
            options.quadrature_samples = quadrature_samples;
            options.jitter_epsilon = jitter_epsilon;
            options.stiffness_regularization_weight = stiffness_regularization_weight;
            options.clamp_to_range = clamp_to_range;
            options.sanity_check = sanity_check;

            tp::texture_filtering(mesh, image.to_mdspan(), options);

            return image_array_to_tensor(std::move(image));
        },
        "mesh"_a,
        "image"_a,
        "value_weight"_a = tp::FilteringOptions().value_weight,
        "gradient_weight"_a = tp::FilteringOptions().gradient_weight,
        "gradient_scale"_a = tp::FilteringOptions().gradient_scale,
        "quadrature_samples"_a = tp::FilteringOptions().quadrature_samples,
        "jitter_epsilon"_a = tp::FilteringOptions().jitter_epsilon,
        "stiffness_regularization_weight"_a =
            tp::FilteringOptions().stiffness_regularization_weight,
        "clamp_to_range"_a = tp::FilteringOptions().clamp_to_range,
        "sanity_check"_a = tp::FilteringOptions().sanity_check,
        R"("Smooth or sharpen a texture image associated with a mesh.

:param mesh: Input mesh with UV attributes.
:param image: Texture image to filter.
:param value_weight: The weight for fitting the values of the signal.
:param gradient_weight: The weight for fitting the modulated gradients of the signal.
:param gradient_scale: The gradient modulation weight. Use a value of 0 for smoothing, and use a value between [2, 10] for sharpening.
:param quadrature_samples: The number of quadrature samples to use for integration (in {1, 3, 6, 12, 24, 32}).
:param jitter_epsilon: Jitter amount per texel (0 to deactivate).
:param stiffness_regularization_weight: Regularize the stiffness matrix using a combinatorial Laplacian energy.
:param clamp_to_range: Clamp out-of-range texels to the given range (disabled by default).
:param sanity_check: Whether to run solver sanity checks. If unset, defaults to true in debug builds and false in release builds.

:return: The filtered texture image.)");

    m.def(
        "texture_stitching",
        [](const SurfaceMesh<Scalar, Index>& mesh,
           const ImageTensor<float>& image_,
           bool exterior_only,
           unsigned int quadrature_samples,
           double jitter_epsilon,
           double stiffness_regularization_weight,
           std::optional<std::pair<double, double>> clamp_to_range,
           std::optional<bool> sanity_check) {
            auto image = image::experimental::create_image<float>(
                image_.shape(0),
                image_.shape(1),
                image_.shape(2));
            copy_tensor_to_image_view(image_, image.to_mdspan());

            tp::StitchingOptions options;
            options.exterior_only = exterior_only;
            options.quadrature_samples = quadrature_samples;
            options.jitter_epsilon = jitter_epsilon;
            options.stiffness_regularization_weight = stiffness_regularization_weight;
            options.clamp_to_range = clamp_to_range;
            options.sanity_check = sanity_check;

            tp::texture_stitching(mesh, image.to_mdspan(), options);

            return image_array_to_tensor(std::move(image));
        },
        "mesh"_a,
        "image"_a,
        "exterior_only"_a = tp::StitchingOptions().exterior_only,
        "quadrature_samples"_a = tp::StitchingOptions().quadrature_samples,
        "jitter_epsilon"_a = tp::StitchingOptions().jitter_epsilon,
        "stiffness_regularization_weight"_a =
            tp::StitchingOptions().stiffness_regularization_weight,
        "clamp_to_range"_a = tp::StitchingOptions().clamp_to_range,
        "sanity_check"_a = tp::StitchingOptions().sanity_check,
        R"(Smooth or sharpen a texture image associated with a mesh.

:param mesh: Input mesh with UV attributes.
:param image: Texture image to stitch.
:param exterior_only: If true, interior texels are fixed degrees of freedom.
:param quadrature_samples: The number of quadrature samples to use for integration (in {1, 3, 6, 12, 24, 32}).
:param jitter_epsilon: Jitter amount per texel (0 to deactivate).
:param stiffness_regularization_weight: Regularize the stiffness matrix using a combinatorial Laplacian energy.
:param clamp_to_range: Clamp out-of-range texels to the given range (disabled by default).
:param sanity_check: Whether to run solver sanity checks. If unset, defaults to true in debug builds and false in release builds.

:return: The stitched texture image.)");

    m.def(
        "geodesic_dilation",
        [](const SurfaceMesh<Scalar, Index>& mesh,
           const ImageTensor<float>& image_,
           float dilation_radius) {
            tp::DilationOptions options;
            options.dilation_radius = dilation_radius;

            auto image = image::experimental::create_image<float>(
                image_.shape(0),
                image_.shape(1),
                image_.shape(2));
            copy_tensor_to_image_view(image_, image.to_mdspan());

            tp::geodesic_dilation(mesh, image.to_mdspan(), options);

            return image_array_to_tensor(std::move(image));
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

            return image_array_to_tensor(std::move(image));
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

    nb::enum_<tp::GradientNormalization>(
        m,
        "GradientNormalization",
        "How per-view gradient contributions are normalized "
        "when assembling the edge-difference target.")
        .value(
            "PerEdge",
            tp::GradientNormalization::PerEdge,
            "Per-edge weighted average of per-view gradients, matching the ShapeGradientDomain "
            "reference. Produces gradient targets bounded by the range of per-view gradients.")
        .value(
            "PerTexelSqrt",
            tp::GradientNormalization::PerTexelSqrt,
            "Per-texel normalization with geometric-mean combination (legacy behavior). Gradient "
            "weights do not sum to 1 per edge at view boundaries, attenuating gradient targets "
            "in transition regions.")
        .export_values();

    m.def(
        "texture_compositing",
        [](const SurfaceMesh<Scalar, Index>& mesh,
           const std::vector<ImageTensor<float>>& textures,
           const std::vector<ImageTensor<float>>& weights,
           double value_weight,
           unsigned int quadrature_samples,
           double jitter_epsilon,
           std::optional<std::pair<double, double>> clamp_to_range,
           tp::GradientNormalization gradient_normalization,
           bool use_direct_solver,
           double stiffness_regularization_weight,
           bool smooth_low_weight_areas,
           unsigned int num_multigrid_levels,
           unsigned int num_gauss_seidel_iterations,
           unsigned int num_v_cycles,
           std::optional<bool> sanity_check) {
            la_runtime_assert(
                textures.size() == weights.size(),
                "Number of colors and weights images must be the same.");

            std::vector<tp::ConstWeightedTextureView<float>> weighted_textures;
            for (const auto kk : range(textures.size())) {
                const tp::View3Df texture = tensor_to_image_view(textures[kk]);
                const tp::View3Df weight = tensor_to_image_view(weights[kk]);
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
            options.clamp_to_range = clamp_to_range;
            options.gradient_normalization = gradient_normalization;
            options.use_direct_solver = use_direct_solver;
            options.stiffness_regularization_weight = stiffness_regularization_weight;
            options.smooth_low_weight_areas = smooth_low_weight_areas;
            options.solver.num_multigrid_levels = num_multigrid_levels;
            options.solver.num_gauss_seidel_iterations = num_gauss_seidel_iterations;
            options.solver.num_v_cycles = num_v_cycles;
            options.sanity_check = sanity_check;

            auto image = tp::texture_compositing(mesh, weighted_textures, options);

            return image_array_to_tensor(std::move(image));
        },
        "mesh"_a,
        "colors"_a,
        "weights"_a,
        nb::kw_only(),
        "value_weight"_a = tp::CompositingOptions().value_weight,
        "quadrature_samples"_a = tp::CompositingOptions().quadrature_samples,
        "jitter_epsilon"_a = tp::CompositingOptions().jitter_epsilon,
        "clamp_to_range"_a = tp::CompositingOptions().clamp_to_range,
        "gradient_normalization"_a = tp::CompositingOptions().gradient_normalization,
        "use_direct_solver"_a = tp::CompositingOptions().use_direct_solver,
        "stiffness_regularization_weight"_a =
            tp::CompositingOptions().stiffness_regularization_weight,
        "smooth_low_weight_areas"_a = tp::CompositingOptions().smooth_low_weight_areas,
        "num_multigrid_levels"_a = tp::CompositingOptions().solver.num_multigrid_levels,
        "num_gauss_seidel_iterations"_a =
            tp::CompositingOptions().solver.num_gauss_seidel_iterations,
        "num_v_cycles"_a = tp::CompositingOptions().solver.num_v_cycles,
        "sanity_check"_a = tp::CompositingOptions().sanity_check,
        R"(Composite multiple (color, weight) into a single texture given a unwrapped mesh.

:param mesh: Input mesh with UV attributes.
:param colors: List of texture images to composite. Input textures must have the same dimensions.
:param weights: List of confidence weights for each texel. 0 means the texel should be ignored, 1 means the texel should be fully trusted. Input weights must have the same dimensions as colors.
:param value_weight: The weight for fitting the values of the signal.
:param quadrature_samples: The number of quadrature samples to use for integration (in {1, 3, 6, 12, 24, 32}).
:param jitter_epsilon: Jitter amount per texel (0 to deactivate).
:param clamp_to_range: Clamp out-of-range texels to the given range (disabled by default).
:param gradient_normalization: How per-view gradient contributions are normalized into the edge-difference target.
:param use_direct_solver: Use a direct (LDLT) solver instead of the multigrid v-cycle. Exact and supports Laplacian regularization for better conditioning, but uses more memory.
:param stiffness_regularization_weight: Weight for combinatorial Laplacian regularization added to the stiffness matrix, to help stabilize the system in poorly conditioned regions. Set to 0 to disable.
:param smooth_low_weight_areas: When gradient_normalization is PerTexelSqrt, whether to smooth pixels with a low total weight (< 1). When enabled, this will dampen the gradient terms for pixels with a low total weight, resulting in a smoother texture in low-confidence areas.
:param num_multigrid_levels: Number of multigrid levels (ignored when use_direct_solver is true).
:param num_gauss_seidel_iterations: Number of Gauss-Seidel iterations per multigrid level. Must be even (ignored when use_direct_solver is true).
:param num_v_cycles: Number of V-cycles to perform (ignored when use_direct_solver is true).
:param sanity_check: Whether to run solver sanity checks. If unset, defaults to true in debug builds and false in release builds.

:return: The composited texture image.)");

    auto pack_textures_and_weights =
        [](std::vector<std::pair<tp::Array3Df, tp::Array3Df>>& textures_and_weights) {
            std::vector<Tensor<float>> textures;
            std::vector<Tensor<float>> weights;
            textures.reserve(textures_and_weights.size());
            weights.reserve(textures_and_weights.size());
            for (auto& [texture_, weight_] : textures_and_weights) {
                textures.emplace_back(image_array_to_tensor(std::move(texture_)));
                weights.emplace_back(image_array_to_tensor(std::move(weight_)));
            }
            return std::make_tuple(textures, weights);
        };

    auto convert_renders = [](const std::vector<ImageTensor<float>>& renders) {
        std::vector<tp::ConstView3Df> views;
        views.reserve(renders.size());
        for (const auto& render : renders) {
            views.push_back(tensor_to_image_view(render));
        }
        return views;
    };

    auto convert_base_texture =
        [](const std::optional<ImageTensor<float>>& base_texture) -> std::optional<tp::Array3Df> {
        if (!base_texture.has_value()) return std::nullopt;
        const auto view = tensor_to_image_view(*base_texture);
        auto image = image::experimental::create_image<float>(
            view.extent(0),
            view.extent(1),
            view.extent(2));
        copy_tensor_to_image_view(*base_texture, image.to_mdspan());
        return image;
    };

    constexpr auto rasterize_doc =
        R"(Rasterize one (color, weight) per (render, camera) and filter out low-confidence weights.

This function has two overloads:

1. ``rasterize_textures_from_renders(scene, renders, *, base_texture=None, ...)``: extract mesh,
   base texture, and cameras from a scene. ``base_texture`` (if provided) overrides any base texture
   in the scene.
2. ``rasterize_textures_from_renders(mesh, cameras, renders, *, base_texture=None, ...)``: take an
   explicit mesh and a list of CameraTransforms.

:param scene: Scene containing a single mesh (possibly with a base texture), and multiple cameras.
:param mesh: Input mesh with UVs (alternative to scene).
:param cameras: List of CameraTransforms (alternative to scene).
:param renders: List of rendered images, one per camera.
:param base_texture: Optional base texture override. Takes precedence over any texture in the scene.
:param width: Width of the rasterized textures. Must match the width of the base texture if present. Otherwise, defaults to 1024.
:param height: Height of the rasterized textures. Must match the height of the base texture if present. Otherwise, defaults to 1024.
:param low_confidence_ratio: Discard low confidence texels whose weights are < ratio * max_weight.
:param base_confidence: Confidence value for the base texture if present. If set to 0, ignore the base texture. Defaults to 0.3 otherwise.

:return: A pair of lists (textures, weights), one per camera.)";

    m.def(
        "rasterize_textures_from_renders",
        [=](const scene::Scene<Scalar, Index>& scene,
            const std::vector<ImageTensor<float>>& renders,
            const std::optional<ImageTensor<float>>& base_texture,
            const std::optional<size_t> width,
            const std::optional<size_t> height,
            const float low_confidence_ratio,
            const std::optional<float> base_confidence) {
            auto textures_and_weights = tp::rasterize_textures_from_renders(
                scene,
                convert_base_texture(base_texture),
                convert_renders(renders),
                width,
                height,
                low_confidence_ratio,
                base_confidence);
            return pack_textures_and_weights(textures_and_weights);
        },
        "scene"_a,
        "renders"_a,
        nb::kw_only(),
        "base_texture"_a = nb::none(),
        "width"_a = nb::none(),
        "height"_a = nb::none(),
        "low_confidence_ratio"_a = 0.75,
        "base_confidence"_a = nb::none(),
        rasterize_doc);

    m.def(
        "rasterize_textures_from_renders",
        [=](const SurfaceMesh<Scalar, Index>& mesh,
            const std::vector<CameraTransforms>& cameras,
            const std::vector<ImageTensor<float>>& renders,
            const std::optional<ImageTensor<float>>& base_texture,
            const std::optional<size_t> width,
            const std::optional<size_t> height,
            const float low_confidence_ratio,
            const std::optional<float> base_confidence) {
            auto textures_and_weights = tp::rasterize_textures_from_renders(
                mesh,
                convert_base_texture(base_texture),
                cameras,
                convert_renders(renders),
                width,
                height,
                low_confidence_ratio,
                base_confidence);
            return pack_textures_and_weights(textures_and_weights);
        },
        "mesh"_a,
        "cameras"_a,
        "renders"_a,
        nb::kw_only(),
        "base_texture"_a = nb::none(),
        "width"_a = nb::none(),
        "height"_a = nb::none(),
        "low_confidence_ratio"_a = 0.75,
        "base_confidence"_a = nb::none(),
        rasterize_doc);

    m.def(
        "extract_mesh_with_alpha_mask",
        [](const SurfaceMesh32d& mesh,
           const ImageTensor<float>& image_,
           const std::optional<AttributeId> texcoord_id,
           const float alpha_threshold) -> SurfaceMesh32d {
            const auto image = tensor_to_image_view(image_);
            tp::ExtractMeshWithAlphaMaskOptions options;
            if (texcoord_id) options.texcoord_id = *texcoord_id;
            options.alpha_threshold = alpha_threshold;
            return tp::extract_mesh_with_alpha_mask(mesh, image, options);
        },
        "mesh"_a,
        "image"_a,
        "texcoord_id"_a = nb::none(),
        "alpha_threshold"_a = tp::ExtractMeshWithAlphaMaskOptions().alpha_threshold,
        R"(Convert a unwrapped triangle mesh with non-opaque texture to tesselated mesh.

:param mesh: Input mesh.
:param image: RGBA non-opaque texture.
:param texcoord_id: Indexed UV attribute id.
:param alpha_threshold: Opaque mask threshold.

:returns: Tessellated triangle mesh, quad mesh or quad-dominant mesh.)");
}

} // namespace lagrange::python
