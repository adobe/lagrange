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
#pragma once

#include <lagrange/AttributeValueType.h>
#include <lagrange/Logger.h>
#include <lagrange/image/Array3D.h>
#include <lagrange/image/View3D.h>
#include <lagrange/scene/Scene.h>
#include <lagrange/scene/scene_utils.h>
#include <lagrange/transform_mesh.h>

#include <algorithm>

namespace lagrange::scene::internal {

using Array3Df = image::experimental::Array3D<float>;
using View3Df = image::experimental::View3D<float>;
using ConstView3Df = image::experimental::View3D<const float>;

// FIXME this strips non-color channel, other variants of this function don't.
inline Array3Df convert_from(const ImageBufferExperimental& image)
{
    size_t nc = std::min(image.num_channels, size_t(3));
    auto result = image::experimental::create_image<float>(image.width, image.height, nc);

    auto copy_buffer = [&](auto scalar) {
        using T = std::decay_t<decltype(scalar)>;
        constexpr bool IsChar = std::is_integral_v<T> && sizeof(T) == 1;
        la_runtime_assert(sizeof(T) * 8 == image.get_bits_per_element());
        auto rawbuf = reinterpret_cast<const T*>(image.data.data());
        for (size_t y = 0, i = 0; y < image.height; ++y) {
            for (size_t x = 0; x < image.width; ++x) {
                for (size_t c = 0; c < image.num_channels; ++c) {
                    if (c >= nc) {
                        ++i;
                        continue;
                    }
                    if constexpr (IsChar) {
                        result(x, y, c) = static_cast<float>(rawbuf[i++]) / 255.f;
                    } else {
                        result(x, y, c) = rawbuf[i++];
                    }
                }
            }
        }
    };

    switch (image.element_type) {
    case AttributeValueType::e_uint8_t: copy_buffer(uint8_t()); break;
    case AttributeValueType::e_int8_t: copy_buffer(int8_t()); break;
    case AttributeValueType::e_uint32_t: copy_buffer(uint32_t()); break;
    case AttributeValueType::e_int32_t: copy_buffer(int32_t()); break;
    case AttributeValueType::e_float: copy_buffer(float()); break;
    case AttributeValueType::e_double: copy_buffer(double()); break;
    default: throw std::runtime_error("Unsupported image scalar type");
    }

    return result;
}

// Convert a float Array3D image to an ImageBufferExperimental (uint8, row-major y,x,c order).
// Note: convert_from() truncates to 3 channels, so this is only a true round-trip inverse for
// images with <= 3 channels.
inline ImageBufferExperimental convert_to(const ConstView3Df& image)
{
    ImageBufferExperimental result;
    result.width = image.extent(0);
    result.height = image.extent(1);
    const size_t num_channels = image.extent(2);
    la_runtime_assert(
        num_channels == 1 || num_channels == 3 || num_channels == 4,
        "ImageBufferExperimental requires 1, 3, or 4 channels");
    result.num_channels = num_channels;
    result.element_type = AttributeValueType::e_uint8_t;
    result.data.resize(result.width * result.height * result.num_channels);
    for (size_t y = 0, i = 0; y < result.height; ++y) {
        for (size_t x = 0; x < result.width; ++x) {
            for (size_t c = 0; c < result.num_channels; ++c) {
                result.data[i++] =
                    static_cast<unsigned char>(std::clamp(image(x, y, c), 0.0f, 1.0f) * 255.0f);
            }
        }
    }
    return result;
}

struct SingleMeshToSceneOptions
{
    MaterialExperimental::AlphaMode alpha_mode = MaterialExperimental::AlphaMode::Opaque;
    float alpha_cutoff = 0.5f;
};

// Create a scene containing a single mesh with a base color texture.
// This is the inverse of single_mesh_from_scene().
template <typename Scalar, typename Index>
Scene<Scalar, Index> single_mesh_to_scene(
    SurfaceMesh<Scalar, Index> mesh,
    const ConstView3Df& image,
    const SingleMeshToSceneOptions& options = {})
{
    Scene<Scalar, Index> scene;

    auto mesh_id = scene.add(std::move(mesh));

    ImageExperimental scene_image;
    scene_image.name = "base_color";
    scene_image.image = convert_to(image);
    auto image_id = scene.add(std::move(scene_image));

    Texture texture;
    texture.name = "base_color";
    texture.image = image_id;
    auto texture_id = scene.add(std::move(texture));

    MaterialExperimental material;
    material.name = "material";
    material.alpha_mode = options.alpha_mode;
    material.alpha_cutoff = options.alpha_cutoff;
    material.base_color_texture.index = texture_id;
    material.base_color_texture.texcoord = 0;
    auto material_id = scene.add(std::move(material));

    Node node;
    node.name = "mesh";
    SceneMeshInstance instance;
    instance.mesh = mesh_id;
    instance.materials.push_back(material_id);
    node.meshes.push_back(std::move(instance));
    auto node_id = scene.add(std::move(node));
    scene.root_nodes.push_back(node_id);

    return scene;
}

// Extract a single uv unwrapped mesh and optionally its base color tensor from a scene.
template <typename Scalar, typename Index>
std::tuple<SurfaceMesh<Scalar, Index>, std::optional<Array3Df>> single_mesh_from_scene(
    const Scene<Scalar, Index>& scene)
{
    using ElementId = scene::ElementId;

    // Find mesh nodes in the scene
    std::vector<ElementId> mesh_node_ids;
    for (ElementId node_id = 0; node_id < scene.nodes.size(); ++node_id) {
        const auto& node = scene.nodes[node_id];
        if (!node.meshes.empty()) {
            mesh_node_ids.push_back(node_id);
        }
    }

    if (mesh_node_ids.size() != 1) {
        throw std::runtime_error(
            fmt::format(
                "Input scene contains {} mesh nodes. Expected exactly 1 mesh node.",
                mesh_node_ids.size()));
    }
    const auto& mesh_node = scene.nodes[mesh_node_ids.front()];

    if (mesh_node.meshes.size() != 1) {
        throw std::runtime_error(
            fmt::format(
                "Input scene has a mesh node with {} instance per node. Expected "
                "exactly 1 instance per node",
                mesh_node.meshes.size()));
    }
    const auto& mesh_instance = mesh_node.meshes.front();

    [[maybe_unused]] const auto mesh_id = mesh_instance.mesh;
    la_debug_assert(mesh_id < scene.meshes.size());
    SurfaceMesh<Scalar, Index> mesh = scene.meshes[mesh_instance.mesh];
    {
        // Apply node local->world transform
        auto world_from_mesh = utils::compute_global_node_transform(scene, mesh_node_ids.front())
                                   .template cast<Scalar>();
        transform_mesh(mesh, world_from_mesh);
    }

    // Find base texture if available
    if (auto num_mats = mesh_instance.materials.size(); num_mats != 1) {
        logger().warn(
            "Mesh node has {} materials. Expected exactly 1 material. Ignoring materials.",
            num_mats);
        return {mesh, std::nullopt};
    }
    const auto& material = scene.materials[mesh_instance.materials.front()];
    if (material.base_color_texture.texcoord != 0) {
        logger().warn(
            "Mesh node material texcoord is {} != 0. Expected 0. Ignoring texcoord.",
            material.base_color_texture.texcoord);
    }
    const auto texture_id = material.base_color_texture.index;
    la_debug_assert(texture_id < scene.textures.size());
    const auto& texture = scene.textures[texture_id];

    const auto image_id = texture.image;
    la_debug_assert(image_id < scene.images.size());
    const auto& image_ = scene.images[image_id].image;
    Array3Df image = convert_from(image_);

    return {mesh, image};
}

} // namespace lagrange::scene::internal
