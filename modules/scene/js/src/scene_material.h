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

#include "scene_binding_utils.h"

#include <lagrange/scene/Scene.h>

#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace lagrange::js::bind {

struct TextureUpdate
{
    std::optional<scene::ElementId> index;
    std::optional<int> texcoord;
};

// Omitted fields stay unchanged. Stage exposed values for an atomic update without copying
// extensions.
struct MaterialUpdate
{
    std::optional<std::string> name;
    std::optional<Eigen::Vector4f> base_color_value;
    std::optional<Eigen::Vector3f> emissive_value;
    std::optional<float> metallic_value;
    std::optional<float> roughness_value;
    std::optional<scene::MaterialExperimental::AlphaMode> alpha_mode;
    std::optional<float> alpha_cutoff;
    std::optional<float> normal_scale;
    std::optional<float> occlusion_strength;
    std::optional<bool> double_sided;
    std::optional<TextureUpdate> base_color_texture;
    std::optional<TextureUpdate> emissive_texture;
    std::optional<TextureUpdate> metallic_roughness_texture;
    std::optional<TextureUpdate> normal_texture;
    std::optional<TextureUpdate> occlusion_texture;
};

inline val copy_material_texture(const scene::TextureInfo& texture)
{
    val result = val::object();
    result.set(
        "index",
        texture.index == scene::invalid_element ? val::null() : val(double(texture.index)));
    result.set("texcoord", texture.texcoord);
    return result;
}

inline TextureUpdate
read_material_texture(const SceneType& scene, const val& input, const char* field)
{
    if (input.isNull()) return {scene::invalid_element, 0};
    TextureUpdate update;
    if (input.typeOf().as<std::string>() != "object") {
        throw std::invalid_argument(std::string(field) + " must be a texture object or null");
    }
    if (has_scene_field(input, "index")) {
        const auto index = input["index"];
        update.index = index.isNull() ? scene::invalid_element
                                      : read_scene_id(index, scene.textures.size(), "texture");
    }
    if (has_scene_field(input, "texcoord")) {
        update.texcoord = static_cast<int>(read_scene_id(
            input["texcoord"],
            size_t(std::numeric_limits<int>::max()) + 1,
            "texture texcoord"));
    }
    return update;
}

inline scene::MaterialExperimental::AlphaMode read_material_alpha_mode(const val& input)
{
    if (input.typeOf().as<std::string>() == "string") {
        const auto mode = input.as<std::string>();
        if (mode == "opaque") return scene::MaterialExperimental::AlphaMode::Opaque;
        if (mode == "mask") return scene::MaterialExperimental::AlphaMode::Mask;
        if (mode == "blend") return scene::MaterialExperimental::AlphaMode::Blend;
    }
    throw std::invalid_argument("alphaMode must be 'opaque', 'mask', or 'blend'");
}

inline std::string_view material_alpha_mode_name(scene::MaterialExperimental::AlphaMode mode)
{
    switch (mode) {
    case scene::MaterialExperimental::AlphaMode::Opaque: return "opaque";
    case scene::MaterialExperimental::AlphaMode::Mask: return "mask";
    case scene::MaterialExperimental::AlphaMode::Blend: return "blend";
    }
    throw std::logic_error("Invalid native material alpha mode");
}

inline MaterialUpdate read_material_update(const SceneType& scene, const val& opts)
{
    MaterialUpdate update;
    if (opts.isUndefined()) return update;
    if (opts.isNull() || opts.typeOf().as<std::string>() != "object") {
        throw std::invalid_argument("Material options must be an object");
    }
    if (has_scene_field(opts, "name")) {
        const auto name = opts["name"];
        if (name.typeOf().as<std::string>() != "string") {
            throw std::invalid_argument("Material name must be a string");
        }
        update.name = name.as<std::string>();
    }
    if (has_scene_field(opts, "baseColorValue")) {
        update.base_color_value.emplace();
        read_scene_vector(opts["baseColorValue"], *update.base_color_value, "baseColorValue");
    }
    if (has_scene_field(opts, "emissiveValue")) {
        update.emissive_value.emplace();
        read_scene_vector(opts["emissiveValue"], *update.emissive_value, "emissiveValue");
    }
    if (has_scene_field(opts, "metallicValue")) {
        update.metallic_value = read_scene_float(opts["metallicValue"], "metallicValue");
    }
    if (has_scene_field(opts, "roughnessValue")) {
        update.roughness_value = read_scene_float(opts["roughnessValue"], "roughnessValue");
    }
    if (has_scene_field(opts, "alphaMode")) {
        update.alpha_mode = read_material_alpha_mode(opts["alphaMode"]);
    }
    if (has_scene_field(opts, "alphaCutoff")) {
        update.alpha_cutoff = read_scene_float(opts["alphaCutoff"], "alphaCutoff");
    }
    if (has_scene_field(opts, "normalScale")) {
        update.normal_scale = read_scene_float(opts["normalScale"], "normalScale");
    }
    if (has_scene_field(opts, "occlusionStrength")) {
        update.occlusion_strength =
            read_scene_float(opts["occlusionStrength"], "occlusionStrength");
    }
    if (has_scene_field(opts, "doubleSided")) {
        const auto double_sided = opts["doubleSided"];
        if (double_sided.typeOf().as<std::string>() != "boolean") {
            throw std::invalid_argument("doubleSided must be a boolean");
        }
        update.double_sided = double_sided.as<bool>();
    }
    if (has_scene_field(opts, "baseColorTexture")) {
        update.base_color_texture =
            read_material_texture(scene, opts["baseColorTexture"], "baseColorTexture");
    }
    if (has_scene_field(opts, "emissiveTexture")) {
        update.emissive_texture =
            read_material_texture(scene, opts["emissiveTexture"], "emissiveTexture");
    }
    if (has_scene_field(opts, "metallicRoughnessTexture")) {
        update.metallic_roughness_texture = read_material_texture(
            scene,
            opts["metallicRoughnessTexture"],
            "metallicRoughnessTexture");
    }
    if (has_scene_field(opts, "normalTexture")) {
        update.normal_texture =
            read_material_texture(scene, opts["normalTexture"], "normalTexture");
    }
    if (has_scene_field(opts, "occlusionTexture")) {
        update.occlusion_texture =
            read_material_texture(scene, opts["occlusionTexture"], "occlusionTexture");
    }
    return update;
}

inline void apply_texture_update(scene::TextureInfo& texture, const TextureUpdate& update)
{
    if (update.index) texture.index = *update.index;
    if (update.texcoord) texture.texcoord = *update.texcoord;
}

inline void apply_material_update(scene::MaterialExperimental& material, MaterialUpdate&& update)
{
    // All validation has finished. Moving the staged name avoids allocation during the commit.
    if (update.name) material.name = std::move(*update.name);
    if (update.base_color_value) material.base_color_value = *update.base_color_value;
    if (update.emissive_value) material.emissive_value = *update.emissive_value;
    if (update.metallic_value) material.metallic_value = *update.metallic_value;
    if (update.roughness_value) material.roughness_value = *update.roughness_value;
    if (update.alpha_mode) material.alpha_mode = *update.alpha_mode;
    if (update.alpha_cutoff) material.alpha_cutoff = *update.alpha_cutoff;
    if (update.normal_scale) material.normal_scale = *update.normal_scale;
    if (update.occlusion_strength) material.occlusion_strength = *update.occlusion_strength;
    if (update.double_sided) material.double_sided = *update.double_sided;
    if (update.base_color_texture) {
        apply_texture_update(material.base_color_texture, *update.base_color_texture);
    }
    if (update.emissive_texture) {
        apply_texture_update(material.emissive_texture, *update.emissive_texture);
    }
    if (update.metallic_roughness_texture) {
        apply_texture_update(
            material.metallic_roughness_texture,
            *update.metallic_roughness_texture);
    }
    if (update.normal_texture)
        apply_texture_update(material.normal_texture, *update.normal_texture);
    if (update.occlusion_texture) {
        apply_texture_update(material.occlusion_texture, *update.occlusion_texture);
    }
}

/// Return an owned snapshot of every exposed PBR material field.
inline val get_material(const SceneType& scene, const val& id)
{
    const auto& material = scene.materials[read_scene_id(id, scene.materials.size(), "material")];
    val result = val::object();
    result.set("name", material.name);
    result.set("baseColorValue", copy_scene_vector(material.base_color_value));
    result.set("emissiveValue", copy_scene_vector(material.emissive_value));
    result.set("metallicValue", material.metallic_value);
    result.set("roughnessValue", material.roughness_value);
    // The view names a null-terminated string literal; use Embind's allocation-free C-string
    // bridge.
    result.set("alphaMode", val(material_alpha_mode_name(material.alpha_mode).data()));
    result.set("alphaCutoff", material.alpha_cutoff);
    result.set("normalScale", material.normal_scale);
    result.set("occlusionStrength", material.occlusion_strength);
    result.set("doubleSided", material.double_sided);
    result.set("baseColorTexture", copy_material_texture(material.base_color_texture));
    result.set("emissiveTexture", copy_material_texture(material.emissive_texture));
    result.set(
        "metallicRoughnessTexture",
        copy_material_texture(material.metallic_roughness_texture));
    result.set("normalTexture", copy_material_texture(material.normal_texture));
    result.set("occlusionTexture", copy_material_texture(material.occlusion_texture));
    return result;
}

/// Add a default material with optional validated overrides.
inline scene::ElementId add_material(SceneType& scene, const val& opts)
{
    scene::MaterialExperimental material;
    apply_material_update(material, read_material_update(scene, opts));
    return scene.add(std::move(material));
}

/// Atomically apply exposed fields without copying or modifying native extensions.
inline void set_material(SceneType& scene, const val& id, const val& opts)
{
    const auto material_id = read_scene_id(id, scene.materials.size(), "material");
    auto update = read_material_update(scene, opts);
    // JavaScript getters may have grown scene.materials. Acquire the reference after all callbacks.
    apply_material_update(scene.materials[material_id], std::move(update));
}

} // namespace lagrange::js::bind
