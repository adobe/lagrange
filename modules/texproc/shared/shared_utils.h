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
#pragma once

// NOTE: These shared utils are used in our CLI examples and Python bindings. They depend on the
// lagrange::scene module. But we do not want to create a strong dependency between
// lagrange::texproc and lagrange::scene, so this file is included directly via relative path in the
// examples and Python bindings C++ files. To avoid confusion with internal src/ files, we place
// this file in a separate "shared/" folder.

#include <lagrange/Logger.h>
#include <lagrange/scene/internal/shared_utils.h>
#include <lagrange/texproc/TextureRasterizer.h>

#include <tbb/parallel_for.h>

namespace lagrange::texproc {

using scene::internal::Array3Df;
using scene::internal::ConstView3Df;
using scene::internal::View3Df;

template <typename Scalar, typename Index>
std::vector<CameraOptions> cameras_from_scene(const scene::Scene<Scalar, Index>& scene)
{
    using ElementId = scene::ElementId;

    std::vector<CameraOptions> cameras;
    for (ElementId node_id = 0; node_id < scene.nodes.size(); ++node_id) {
        const auto& node = scene.nodes[node_id];
        if (!node.cameras.empty()) {
            auto world_from_node = scene::utils::compute_global_node_transform(scene, node_id);
            for (auto camera_id : node.cameras) {
                const auto& scene_camera = scene.cameras[camera_id];
                CameraOptions camera;
                camera.view_transform =
                    scene::utils::camera_view_transform(scene_camera, world_from_node);
                camera.projection_transform =
                    scene::utils::camera_projection_transform(scene_camera);
                cameras.push_back(camera);
            }
        }
    }

    return cameras;
}

template <typename Scalar, typename Index>
std::vector<std::pair<Array3Df, Array3Df>> rasterize_textures_from_renders(
    const lagrange::scene::Scene<Scalar, Index>& scene,
    std::optional<Array3Df> base_texture_in,
    const std::vector<ConstView3Df>& renders,
    const std::optional<size_t> tex_width,
    const std::optional<size_t> tex_height,
    const float low_confidence_ratio,
    const std::optional<float> base_confidence)
{
    // Load mesh, base texture and cameras from input scene
    auto [mesh, base_texture] = scene::internal::single_mesh_from_scene(scene);
    auto cameras = cameras_from_scene(scene);
    lagrange::logger().info("Found {} cameras in the input scene", cameras.size());

    if (base_texture_in.has_value()) {
        if (base_texture.has_value()) {
            lagrange::logger().warn(
                "Input scene already contains a base texture. Overriding with user-provided "
                "texture.");
        }
        base_texture = std::move(base_texture_in);
    }

    // Load rendered images to unproject
    la_runtime_assert(!renders.empty(), "No rendered images to unproject");
    for (const auto& render : renders) {
        size_t img_width = render.extent(0);
        size_t img_height = render.extent(1);
        size_t img_channels = render.extent(2);
        la_runtime_assert(img_width == renders.front().extent(0), "Render width must all be equal");
        la_runtime_assert(
            img_height == renders.front().extent(1),
            "Render height must all be equal");
        la_runtime_assert(
            img_channels == renders.front().extent(2),
            "Render num channels must all be equal");
    }
    la_runtime_assert(
        renders.size() == cameras.size(),
        "Number of renders must match number of cameras");

    std::vector<std::pair<Array3Df, Array3Df>> textures_and_weights;

    // Use base texture with a low confidence
    if (base_confidence.has_value() && base_confidence.value() == 0) {
        if (base_texture.has_value()) {
            lagrange::logger().warn(
                "Base confidence is 0, ignoring base texture in the input scene.");
        }
    } else {
        if (base_texture.has_value()) {
            const float default_confidence = base_confidence.value_or(0.3f);
            lagrange::logger().info(
                "Using base texture with uniform confidence: {}",
                default_confidence);
            const auto base_image = base_texture.value();
            auto base_weights = lagrange::image::experimental::create_image<float>(
                base_image.extent(0),
                base_image.extent(1),
                1);
            for (size_t i = 0; i < base_weights.extent(0); ++i) {
                for (size_t j = 0; j < base_weights.extent(1); ++j) {
                    base_weights(i, j, 0) = default_confidence;
                }
            }
            textures_and_weights.emplace_back(base_image, std::move(base_weights));
        } else {
            if (base_confidence.has_value()) {
                lagrange::logger().warn(
                    "No base texture was found in the input scene. Ignoring user-provided base "
                    "confidence: {}",
                    base_confidence.value());
            }
        }
    }

    // Check base texture size against expected size
    TextureRasterizerOptions rasterizer_options;
    if (textures_and_weights.empty()) {
        rasterizer_options.width = tex_width.value_or(1024);
        rasterizer_options.height = tex_height.value_or(1024);
        lagrange::logger().info(
            "No base texture found. Using rasterization size: {}x{}",
            rasterizer_options.width,
            rasterizer_options.height);
    } else {
        const auto& base_image = textures_and_weights.front().first;
        la_runtime_assert(!tex_width.has_value() || base_image.extent(0) == tex_width.value());
        la_runtime_assert(!tex_height.has_value() || base_image.extent(1) == tex_height.value());
        rasterizer_options.width = base_image.extent(0);
        rasterizer_options.height = base_image.extent(1);
        la_runtime_assert(
            renders.front().extent(2) == base_image.extent(2),
            fmt::format(
                "Input render image num channels (={}) must match base texture num channels (={})",
                renders.front().extent(2),
                base_image.extent(2)));
        lagrange::logger().info(
            "Using base texture size for rasterization: {}x{}",
            rasterizer_options.width,
            rasterizer_options.height);
    }

    // Unproject render in texture space and generate confidence map for each camera
    size_t offset = textures_and_weights.size();
    textures_and_weights.resize(offset + cameras.size());
    const TextureRasterizer<Scalar, Index> rasterizer(mesh, rasterizer_options);
    lagrange::logger().info("Computing confidence maps for {} cameras", cameras.size());
    tbb::parallel_for(size_t(0), cameras.size(), [&](size_t i) {
        textures_and_weights[offset + i] =
            rasterizer.weighted_texture_from_render(renders[i], cameras[i]);
    });

    // Filter confidence across all cameras at each pixel
    lagrange::logger().info(
        "Filtering low confidence values using ratio threshold: {}",
        low_confidence_ratio);
    filter_low_confidences(textures_and_weights, low_confidence_ratio);

    return textures_and_weights;
}

} // namespace lagrange::texproc
