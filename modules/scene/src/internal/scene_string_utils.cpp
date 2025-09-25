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

#include <lagrange/AttributeValueType.h>
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/scene/internal/scene_string_utils.h>

#include <spdlog/fmt/ranges.h>
// Support for std::optional<> added in fmt 10.0.0
// Uncomment after updating fmt version in our vcpkg registry...
// #include <spdlog/fmt/std.h>

#include <vector>

namespace lagrange::scene::internal {

std::string to_string(const std::vector<ElementId>& ids)
{
    return fmt::format("[{}]", fmt::join(ids, ", "));
}

std::string to_string(ElementId id)
{
    if (id == invalid_element) {
        return "null";
    } else {
        return std::to_string(id);
    }
}

std::string to_string(AttributeValueType t)
{
    switch (t) {
    case AttributeValueType::e_uint8_t: return "uint8";
    case AttributeValueType::e_int8_t: return "int8";
    case AttributeValueType::e_uint16_t: return "uint16";
    case AttributeValueType::e_int16_t: return "int16";
    case AttributeValueType::e_uint32_t: return "uint32";
    case AttributeValueType::e_int32_t: return "int32";
    case AttributeValueType::e_uint64_t: return "uint64";
    case AttributeValueType::e_int64_t: return "int64";
    case AttributeValueType::e_float: return "float32";
    case AttributeValueType::e_double: return "float64";
    default: return "unknown";
    }
}

std::string to_string(const SceneMeshInstance& mesh_instance, size_t indent)
{
    return fmt::format("{:{}s}mesh: {}\n", "", indent, to_string(mesh_instance.mesh)) +
           fmt::format("{:{}s}materials: {}\n", "", indent, to_string(mesh_instance.materials));
}

std::string to_string(const Node& node, size_t indent)
{
    auto M = node.transform.matrix();
    std::string r = fmt::format("{:{}s}name: {}\n", "", indent, node.name) +
                    fmt::format("{:{}s}transform:\n", "", indent) +
                    fmt::format(
                        "{:{}s}- [ {: 8.3f}, {: 8.3f}, {: 8.3f}, {: 8.3f} ]\n",
                        "",
                        indent,
                        M(0, 0),
                        M(0, 1),
                        M(0, 2),
                        M(0, 3)) +
                    fmt::format(
                        "{:{}s}- [ {: 8.3f}, {: 8.3f}, {: 8.3f}, {: 8.3f} ]\n",
                        "",
                        indent,
                        M(1, 0),
                        M(1, 1),
                        M(1, 2),
                        M(1, 3)) +
                    fmt::format(
                        "{:{}s}- [ {: 8.3f}, {: 8.3f}, {: 8.3f}, {: 8.3f} ]\n",
                        "",
                        indent,
                        M(2, 0),
                        M(2, 1),
                        M(2, 2),
                        M(2, 3)) +
                    fmt::format(
                        "{:{}s}- [ {: 8.3f}, {: 8.3f}, {: 8.3f}, {: 8.3f} ]\n",
                        "",
                        indent,
                        M(3, 0),
                        M(3, 1),
                        M(3, 2),
                        M(3, 3)) +
                    fmt::format("{:{}s}parent: {}\n", "", indent, to_string(node.parent)) +
                    fmt::format("{:{}s}children: {}\n", "", indent, to_string(node.children)) +
                    fmt::format("{:{}s}meshes:\n", "", indent);

    for (const auto& mesh_instance : node.meshes) {
        auto s = to_string(mesh_instance, indent + 2);
        s[indent] = '-';
        r += s;
    }
    r += fmt::format("{:{}s}cameras: {}\n", "", indent, to_string(node.cameras)) +
         fmt::format("{:{}s}lights: {}\n", "", indent, to_string(node.lights));
    if (!node.extensions.empty()) {
        r +=
            fmt::format("{:{}s}extensions:\n", "", indent) + to_string(node.extensions, indent + 2);
    }
    return r;
}

std::string to_string(const ImageBufferExperimental& img_buf, size_t indent)
{
    std::string r =
        fmt::format("{:{}s}width: {}\n", "", indent, img_buf.width) +
        fmt::format("{:{}s}height: {}\n", "", indent, img_buf.height) +
        fmt::format("{:{}s}num_channels: {}\n", "", indent, img_buf.num_channels) +
        fmt::format("{:{}s}element_type: {}\n", "", indent, to_string(img_buf.element_type)) +
        fmt::format("{:{}s}data: \"<binary: {} bytes>\"\n", "", indent, img_buf.data.size());
    return r;
}

std::string to_string(const ImageExperimental& img, size_t indent)
{
    std::string r = fmt::format("{:{}s}name: {}\n", "", indent, img.name) +
                    fmt::format("{:{}s}image:\n{}", "", indent, to_string(img.image, indent + 2)) +
                    fmt::format("{:{}s}uri: {}\n", "", indent, img.uri.string());
    if (!img.extensions.empty()) {
        r +=
            fmt::format("{:{}s}extensions:\n{}", "", indent, to_string(img.extensions, indent + 2));
    }
    return r;
}

std::string to_string(const TextureInfo& tex_info, size_t indent)
{
    return fmt::format("{:{}s}index: {}\n", "", indent, to_string(tex_info.index)) +
           fmt::format("{:{}s}texcoord: {}\n", "", indent, tex_info.texcoord);
}

std::string to_string(const MaterialExperimental::AlphaMode& mode)
{
    switch (mode) {
    case MaterialExperimental::AlphaMode::Opaque: return "Opaque";
    case MaterialExperimental::AlphaMode::Mask: return "Mask";
    case MaterialExperimental::AlphaMode::Blend: return "Blend";
    default: return "UNKNOWN";
    }
}

std::string to_string(const MaterialExperimental& material, size_t indent)
{
    std::string r =
        fmt::format("{:{}s}name: {}\n", "", indent, material.name) +
        fmt::format(
            "{:{}s}base_color_value: [{}, {}, {}, {}]\n",
            "",
            indent,
            material.base_color_value[0],
            material.base_color_value[1],
            material.base_color_value[2],
            material.base_color_value[3]) +
        fmt::format(
            "{:{}s}base_color_texture:\n{}",
            "",
            indent,
            to_string(material.base_color_texture, indent + 2)) +
        fmt::format(
            "{:{}s}emissive_value: [{}, {}, {}]\n",
            "",
            indent,
            material.emissive_value[0],
            material.emissive_value[1],
            material.emissive_value[2]) +
        fmt::format(
            "{:{}s}emissive_texture:\n{}",
            "",
            indent,
            to_string(material.emissive_texture, indent + 2)) +
        fmt::format(
            "{:{}s}metallic_roughness_texture:\n{}",
            "",
            indent,
            to_string(material.metallic_roughness_texture, indent + 2)) +
        fmt::format("{:{}s}metallic_value: {}\n", "", indent, material.metallic_value) +
        fmt::format("{:{}s}roughness_value: {}\n", "", indent, material.roughness_value) +
        fmt::format("{:{}s}alpha_mode: {}\n", "", indent, to_string(material.alpha_mode)) +
        fmt::format("{:{}s}alpha_cutoff: {}\n", "", indent, material.alpha_cutoff) +
        fmt::format("{:{}s}normal_scale: {}\n", "", indent, material.normal_scale) +
        fmt::format(
            "{:{}s}normal_texture:\n{}",
            "",
            indent,
            to_string(material.normal_texture, indent + 2)) +
        fmt::format(
            "{:{}s}occlusion_strength: {}\n",
            "",
            indent,
            to_string(material.occlusion_strength)) +
        fmt::format(
            "{:{}s}occlusion_texture:\n{}",
            "",
            indent,
            to_string(material.occlusion_texture, indent + 2)) +
        fmt::format("{:{}s}double_sided: {}\n", "", indent, material.double_sided);
    if (!material.extensions.empty()) {
        r += fmt::format(
            "{:{}s}extensions:\n{}",
            "",
            indent,
            to_string(material.extensions, indent + 2));
    }
    return r;
}

std::string to_string(const Texture::TextureFilter& filter)
{
    switch (filter) {
    case Texture::TextureFilter::Undefined: return "Undefined";
    case Texture::TextureFilter::Nearest: return "Nearest";
    case Texture::TextureFilter::Linear: return "Linear";
    case Texture::TextureFilter::NearestMipmapNearest: return "NearestMipmapNearest";
    case Texture::TextureFilter::LinearMipmapNearest: return "LinearMipmapNearest";
    case Texture::TextureFilter::NearestMipmapLinear: return "NearestMipmapLinear";
    case Texture::TextureFilter::LinearMipmapLinear: return "LinearMipmapLinear";
    default: return "UNKNOWN";
    }
}

std::string to_string(const Texture::WrapMode& mode)
{
    switch (mode) {
    case Texture::WrapMode::Wrap: return "Wrap";
    case Texture::WrapMode::Clamp: return "Clamp";
    case Texture::WrapMode::Decal: return "Decal";
    case Texture::WrapMode::Mirror: return "Mirror";
    default: return "UNKNOWN";
    }
}

std::string to_string(const Texture& texture, size_t indent)
{
    std::string r =
        fmt::format("{:{}s}name: {}\n", "", indent, texture.name) +
        fmt::format("{:{}s}image: {}\n", "", indent, to_string(texture.image)) +
        fmt::format("{:{}s}mag_filter: {}\n", "", indent, to_string(texture.mag_filter)) +
        fmt::format("{:{}s}min_filter: {}\n", "", indent, to_string(texture.min_filter)) +
        fmt::format("{:{}s}wrap_u: {}\n", "", indent, to_string(texture.wrap_u)) +
        fmt::format("{:{}s}wrap_v: {}\n", "", indent, to_string(texture.wrap_v)) +
        fmt::format("{:{}s}scale: [{}, {}]\n", "", indent, texture.scale[0], texture.scale[1]) +
        fmt::format("{:{}s}offset: [{}, {}]\n", "", indent, texture.offset[0], texture.offset[1]) +
        fmt::format("{:{}s}rotation: {}\n", "", indent, texture.rotation);
    if (!texture.extensions.empty()) {
        r += fmt::format(
            "{:{}s}extensions:\n{}",
            "",
            indent,
            to_string(texture.extensions, indent + 2));
    }
    return r;
}

std::string to_string(const Light::Type& type)
{
    switch (type) {
    case Light::Type::Undefined: return "Undefined";
    case Light::Type::Directional: return "Directional";
    case Light::Type::Point: return "Point";
    case Light::Type::Spot: return "Spot";
    case Light::Type::Ambient: return "Ambient";
    case Light::Type::Area: return "Area";
    default: return "UNKNOWN";
    }
}

std::string to_string(const Light& light, size_t indent)
{
    std::string r =
        fmt::format("{:{}s}name: {}\n", "", indent, light.name) +
        fmt::format("{:{}s}type: {}\n", "", indent, to_string(light.type)) +
        fmt::format(
            "{:{}s}position: [{}, {}, {}]\n",
            "",
            indent,
            light.position[0],
            light.position[1],
            light.position[2]) +
        fmt::format(
            "{:{}s}direction: [{}, {}, {}]\n",
            "",
            indent,
            light.direction[0],
            light.direction[1],
            light.direction[2]) +
        fmt::format("{:{}s}up: [{}, {}, {}]\n", "", indent, light.up[0], light.up[1], light.up[2]) +
        fmt::format("{:{}s}intensity: {}\n", "", indent, light.intensity) +
        fmt::format("{:{}s}attenuation_constant: {}\n", "", indent, light.attenuation_constant) +
        fmt::format("{:{}s}attenuation_linear: {}\n", "", indent, light.attenuation_linear) +
        fmt::format("{:{}s}attenuation_quadratic: {}\n", "", indent, light.attenuation_quadratic) +
        fmt::format("{:{}s}attenuation_cubic: {}\n", "", indent, light.attenuation_cubic) +
        fmt::format("{:{}s}range: {}\n", "", indent, light.range) +
        fmt::format(
            "{:{}s}color_diffuse: [{}, {}, {}]\n",
            "",
            indent,
            light.color_diffuse[0],
            light.color_diffuse[1],
            light.color_diffuse[2]) +
        fmt::format(
            "{:{}s}color_specular: [{}, {}, {}]\n",
            "",
            indent,
            light.color_specular[0],
            light.color_specular[1],
            light.color_specular[2]) +
        fmt::format(
            "{:{}s}color_ambient: [{}, {}, {}]\n",
            "",
            indent,
            light.color_ambient[0],
            light.color_ambient[1],
            light.color_ambient[2]) +
        fmt::format("{:{}s}angle_inner_cone: {}\n", "", indent, light.angle_inner_cone) +
        fmt::format("{:{}s}angle_outer_cone: {}\n", "", indent, light.angle_outer_cone) +
        fmt::format("{:{}s}size: [{}, {}]\n", "", indent, light.size[0], light.size[1]);
    if (!light.extensions.empty()) {
        r += fmt::format(
            "{:{}s}extensions:\n{}",
            "",
            indent,
            to_string(light.extensions, indent + 2));
    }
    return r;
}

std::string to_string(const Camera::Type& type)
{
    switch (type) {
    case Camera::Type::Perspective: return "Perspective";
    case Camera::Type::Orthographic: return "Orthographic";
    default: return "UNKNOWN";
    }
}

std::string to_string(const Camera& camera, size_t indent)
{
    std::string r =
        fmt::format("{:{}s}name: {}\n", "", indent, camera.name) +
        fmt::format(
            "{:{}s}position: [{}, {}, {}]\n",
            "",
            indent,
            camera.position[0],
            camera.position[1],
            camera.position[2]) +
        fmt::format(
            "{:{}s}up: [{}, {}, {}]\n",
            "",
            indent,
            camera.up[0],
            camera.up[1],
            camera.up[2]) +
        fmt::format(
            "{:{}s}look_at: [{}, {}, {}]\n",
            "",
            indent,
            camera.look_at[0],
            camera.look_at[1],
            camera.look_at[2]) +
        fmt::format("{:{}s}near_plane: {}\n", "", indent, camera.near_plane) +
        fmt::format(
            "{:{}s}far_plane: {}\n",
            "",
            indent,
            camera.far_plane.value_or(std::numeric_limits<float>::infinity())) +
        fmt::format("{:{}s}type: {}\n", "", indent, to_string(camera.type)) +
        fmt::format("{:{}s}orthographic_width: {}\n", "", indent, camera.orthographic_width) +
        fmt::format("{:{}s}aspect_ratio: {}\n", "", indent, camera.aspect_ratio) +
        fmt::format("{:{}s}horizontal_fov: {}\n", "", indent, camera.horizontal_fov);
    if (!camera.extensions.empty()) {
        r += fmt::format(
            "{:{}s}extensions:\n{}",
            "",
            indent,
            to_string(camera.extensions, indent + 2));
    }
    return r;
}

std::string to_string(const Animation& animation, size_t indent)
{
    std::string r = fmt::format("{:{}s}name: {}\n", "", indent, animation.name);
    if (!animation.extensions.empty()) {
        r += fmt::format(
            "{:{}s}extensions:\n{}",
            "",
            indent,
            to_string(animation.extensions, indent + 2));
    }
    return r;
}

std::string to_string(const Skeleton& skeleton, size_t indent)
{
    std::string r = fmt::format("{:{}s}meshes: {}\n", "", indent, to_string(skeleton.meshes));
    if (!skeleton.extensions.empty()) {
        r += fmt::format(
            "{:{}s}extensions:\n{}",
            "",
            indent,
            to_string(skeleton.extensions, indent + 2));
    }
    return r;
}

template <typename Scalar, typename Index>
std::string to_string(const Scene<Scalar, Index>& scene, size_t indent)
{
    std::string r = fmt::format("{:{}s}name: {}\n", "", indent, scene.name);

    if (!scene.nodes.empty()) {
        r += fmt::format("{:{}s}nodes:\n", "", indent);
        for (const auto& node : scene.nodes) {
            std::string node_str = to_string(node, indent + 2);
            node_str[indent] = '-';
            r += node_str;
        }
    }

    if (!scene.root_nodes.empty()) {
        r += fmt::format("{:{}s}root_nodes: {}\n", "", indent, to_string(scene.root_nodes));
    }

    if (!scene.meshes.empty()) {
        r += fmt::format("{:{}s}meshes:\n", "", indent);
        for (const auto& mesh : scene.meshes) {
            r += fmt::format(
                "{:{}s}- \"<SurfaceMesh: {} vertices, {} facets>\"\n",
                "",
                indent + 2,
                mesh.get_num_vertices(),
                mesh.get_num_facets());
        }
    }

    if (!scene.images.empty()) {
        r += fmt::format("{:{}s}images:\n", "", indent);
        for (const auto& img : scene.images) {
            std::string img_str = to_string(img, indent + 2);
            img_str[indent] = '-';
            r += img_str;
        }
    }

    if (!scene.textures.empty()) {
        r += fmt::format("{:{}s}textures:\n", "", indent);
        for (const auto& tex : scene.textures) {
            std::string tex_str = to_string(tex, indent + 2);
            tex_str[indent] = '-';
            r += tex_str;
        }
    }

    if (!scene.materials.empty()) {
        r += fmt::format("{:{}s}materials:\n", "", indent);
        for (const auto& mat : scene.materials) {
            std::string mat_str = to_string(mat, indent + 2);
            mat_str[indent] = '-';
            r += mat_str;
        }
    }

    if (!scene.lights.empty()) {
        r += fmt::format("{:{}s}lights:\n", "", indent);
        for (const auto& light : scene.lights) {
            std::string light_str = to_string(light, indent + 2);
            light_str[indent] = '-';
            r += light_str;
        }
    }

    if (!scene.cameras.empty()) {
        r += fmt::format("{:{}s}cameras:\n", "", indent);
        for (const auto& camera : scene.cameras) {
            std::string camera_str = to_string(camera, indent + 2);
            camera_str[indent] = '-';
            r += camera_str;
        }
    }

    if (!scene.skeletons.empty()) {
        r += fmt::format("{:{}s}skeletons:\n", "", indent);
        for (const auto& skeleton : scene.skeletons) {
            std::string skeleton_str = to_string(skeleton, indent + 2);
            skeleton_str[indent] = '-';
            r += skeleton_str;
        }
    }

    if (!scene.animations.empty()) {
        r += fmt::format("{:{}s}animations:\n", "", indent);
        for (const auto& animation : scene.animations) {
            std::string animation_str = to_string(animation, indent + 2);
            animation_str[indent] = '-';
            r += animation_str;
        }
    }

    if (!scene.extensions.empty()) {
        r += fmt::format("{:{}s}extensions:\n", "", indent) +
             to_string(scene.extensions, indent + 2);
    }
    return r;
}

std::string to_string(const Value& value, size_t indent)
{
    if (value.is_bool()) {
        return value.get_bool() ? "true" : "false";
    } else if (value.is_int()) {
        return std::to_string(value.get_int());
    } else if (value.is_real()) {
        return std::to_string(value.get_real());
    } else if (value.is_string()) {
        return value.get_string();
    } else if (value.is_buffer()) {
        return fmt::format("\"<binary: {} bytes>\"", value.get_buffer().size());
    } else if (value.is_array()) {
        const auto& arr = value.get_array();
        if (arr.empty()) {
            return "[]";
        } else {
            std::vector<std::string> value_strs;
            bool contains_object_or_array = false;
            for (size_t i = 0; i < value.size(); i++) {
                if (arr[i].is_object() || arr[i].is_array()) {
                    contains_object_or_array = true;
                    value_strs.push_back(to_string(arr[i], indent + 2));
                } else {
                    value_strs.push_back(to_string(arr[i], indent));
                }
            }
            if (contains_object_or_array) {
                std::string r = "\n";
                for (size_t i = 0; i < value_strs.size(); i++) {
                    if (value_strs[i].find('\n') != std::string::npos) {
                        r += fmt::format("{:{}s}- {}\n", "", indent, value_strs[i]);
                    } else {
                        value_strs[i][indent] = '-';
                        r += value_strs[i];
                    }
                }
                while (!r.empty() && r.back() == '\n') {
                    r.pop_back();
                }
                return r;
            } else {
                return fmt::format("[{}]", fmt::join(value_strs, ", "));
            }
        }
    } else if (value.is_object()) {
        std::string r = "\n";
        for (const auto& [key, val] : value.get_object()) {
            r += fmt::format("{:{}s}{}: {}\n", "", indent, key, to_string(val, indent + 2));
        }
        while (!r.empty() && r.back() == '\n') {
            r.pop_back();
        }
        return r;
    } else {
        return "null";
    }
}

std::string to_string(const Extensions& extensions, size_t indent)
{
    std::string r;
    for (const auto& [key, value] : extensions.data) {
        r += fmt::format("{:{}s}{}: {}\n", "", indent, key, to_string(value, indent + 2));
    }
    return r;
}

#define LA_X_to_string(_, Scalar, Index) \
    template std::string to_string(const Scene<Scalar, Index>&, size_t);
LA_SURFACE_MESH_X(to_string, 0)

} // namespace lagrange::scene::internal
