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
#include <lagrange/serialization/serialize_scene.h>

#include <lagrange/AttributeValueType.h>
#include <lagrange/Logger.h>
#include <lagrange/cast.h>
#include <lagrange/scene/SceneTypes.h>
#include <lagrange/scene/scene_convert.h>
#include <lagrange/scene/simple_scene_convert.h>
#include <lagrange/serialization/serialize_mesh.h>
#include <lagrange/serialization/serialize_simple_scene.h>
#include <lagrange/utils/assert.h>

#include "CistaScene.h"
#include "compress.h"
#include "detect_type.h"
#include "mesh_convert.h"

#include <cstring>

namespace lagrange::serialization {

using internal::compress_buffer;
using internal::decompress_buffer;
using internal::is_compressed;
using internal::k_cista_mode;
using internal::k_invalid_element;

namespace internal {

// ---------------------------------------------------------------------------
// Value / Extensions conversion
// ---------------------------------------------------------------------------

CistaValue to_cista_value(const scene::Value& value)
{
    CistaValue cv;
    if (value.is_bool()) {
        cv.type = CistaValueType::Bool;
        cv.bool_val = value.get_bool();
    } else if (value.is_int()) {
        cv.type = CistaValueType::Int;
        cv.int_val = value.get_int();
    } else if (value.is_real()) {
        cv.type = CistaValueType::Double;
        cv.double_val = value.get_real();
    } else if (value.is_string()) {
        cv.type = CistaValueType::String;
        const auto& s = value.get_string();
        cv.string_val = data::string(s.data(), s.size());
    } else if (value.is_buffer()) {
        cv.type = CistaValueType::Buffer;
        const auto& buf = value.get_buffer();
        cv.buffer_val.resize(buf.size());
        if (!buf.empty()) {
            std::memcpy(cv.buffer_val.data(), buf.data(), buf.size());
        }
    } else if (value.is_array()) {
        cv.type = CistaValueType::Array;
        const auto& arr = value.get_array();
        cv.array_val.reserve(arr.size());
        for (const auto& elem : arr) {
            cv.array_val.emplace_back(to_cista_value(elem));
        }
    } else if (value.is_object()) {
        cv.type = CistaValueType::Object;
        const auto& obj = value.get_object();
        cv.object_keys.reserve(obj.size());
        cv.object_values.reserve(obj.size());
        for (const auto& [key, val] : obj) {
            cv.object_keys.emplace_back(data::string(key.data(), key.size()));
            cv.object_values.emplace_back(to_cista_value(val));
        }
    }
    return cv;
}

scene::Value from_cista_value(const CistaValue& cv)
{
    switch (cv.type) {
    case CistaValueType::Bool: return scene::Value(cv.bool_val);
    case CistaValueType::Int: return scene::Value(cv.int_val);
    case CistaValueType::Double: return scene::Value(cv.double_val);
    case CistaValueType::String:
        return scene::Value(std::string(cv.string_val.data(), cv.string_val.size()));
    case CistaValueType::Buffer: {
        scene::Value::Buffer buf(cv.buffer_val.size());
        if (!cv.buffer_val.empty()) {
            std::memcpy(buf.data(), cv.buffer_val.data(), cv.buffer_val.size());
        }
        return scene::Value(std::move(buf));
    }
    case CistaValueType::Array: {
        scene::Value::Array arr;
        arr.reserve(cv.array_val.size());
        for (const auto& elem : cv.array_val) {
            arr.emplace_back(from_cista_value(elem));
        }
        return scene::Value(std::move(arr));
    }
    case CistaValueType::Object: {
        scene::Value::Object obj;
        for (size_t i = 0; i < cv.object_keys.size(); ++i) {
            std::string key(cv.object_keys[i].data(), cv.object_keys[i].size());
            obj.emplace(std::move(key), from_cista_value(cv.object_values[i]));
        }
        return scene::Value(std::move(obj));
    }
    }
    return scene::Value(false); // unreachable
}

CistaExtensions to_cista_extensions(const scene::Extensions& ext)
{
    CistaExtensions cext;
    cext.keys.reserve(ext.data.size());
    cext.values.reserve(ext.data.size());
    for (const auto& [key, val] : ext.data) {
        cext.keys.emplace_back(data::string(key.data(), key.size()));
        cext.values.emplace_back(to_cista_value(val));
    }
    return cext;
}

scene::Extensions from_cista_extensions(const CistaExtensions& cext)
{
    scene::Extensions ext;
    for (size_t i = 0; i < cext.keys.size(); ++i) {
        std::string key(cext.keys[i].data(), cext.keys[i].size());
        ext.data.emplace(std::move(key), from_cista_value(cext.values[i]));
    }
    return ext;
}

// ---------------------------------------------------------------------------
// TextureInfo conversion
// ---------------------------------------------------------------------------

CistaTextureInfo to_cista_texture_info(const scene::TextureInfo& ti)
{
    CistaTextureInfo cti;
    cti.index = (ti.index == scene::invalid_element) ? k_invalid_element : ti.index;
    cti.texcoord = ti.texcoord;
    return cti;
}

scene::TextureInfo from_cista_texture_info(const CistaTextureInfo& cti)
{
    scene::TextureInfo ti;
    ti.index = (cti.index == k_invalid_element) ? scene::invalid_element : cti.index;
    ti.texcoord = cti.texcoord;
    return ti;
}

// ---------------------------------------------------------------------------
// Node conversion
// ---------------------------------------------------------------------------

CistaNode to_cista_node(const scene::Node& node)
{
    CistaNode cn;
    cn.name = data::string(node.name.data(), node.name.size());

    // Affine3f = 4x4 float = 16 floats
    const float* tf = node.transform.matrix().data();
    std::copy(tf, tf + 16, cn.transform.begin());

    cn.parent = (node.parent == scene::invalid_element) ? k_invalid_element : node.parent;

    cn.children.reserve(node.children.size());
    for (size_t i = 0; i < node.children.size(); ++i) {
        cn.children.emplace_back(node.children[i]);
    }

    cn.meshes.reserve(node.meshes.size());
    for (size_t i = 0; i < node.meshes.size(); ++i) {
        CistaSceneMeshInstance cmi;
        cmi.mesh = (node.meshes[i].mesh == scene::invalid_element) ? k_invalid_element
                                                                   : node.meshes[i].mesh;
        cmi.materials.reserve(node.meshes[i].materials.size());
        for (size_t j = 0; j < node.meshes[i].materials.size(); ++j) {
            cmi.materials.emplace_back(node.meshes[i].materials[j]);
        }
        cn.meshes.emplace_back(std::move(cmi));
    }

    cn.cameras.reserve(node.cameras.size());
    for (size_t i = 0; i < node.cameras.size(); ++i) {
        cn.cameras.emplace_back(node.cameras[i]);
    }

    cn.lights.reserve(node.lights.size());
    for (size_t i = 0; i < node.lights.size(); ++i) {
        cn.lights.emplace_back(node.lights[i]);
    }

    cn.extensions = to_cista_extensions(node.extensions);
    return cn;
}

scene::Node from_cista_node(const CistaNode& cn)
{
    scene::Node node;
    node.name = std::string(cn.name.data(), cn.name.size());

    std::copy(cn.transform.begin(), cn.transform.end(), node.transform.matrix().data());

    node.parent = (cn.parent == k_invalid_element) ? scene::invalid_element : cn.parent;

    for (const auto& child : cn.children) {
        node.children.push_back(child);
    }

    for (const auto& cmi : cn.meshes) {
        scene::SceneMeshInstance smi;
        smi.mesh = (cmi.mesh == k_invalid_element) ? scene::invalid_element : cmi.mesh;
        for (const auto& mat : cmi.materials) {
            smi.materials.push_back(mat);
        }
        node.meshes.push_back(std::move(smi));
    }

    for (const auto& cam : cn.cameras) {
        node.cameras.push_back(cam);
    }

    for (const auto& light : cn.lights) {
        node.lights.push_back(light);
    }

    node.extensions = from_cista_extensions(cn.extensions);
    return node;
}

// ---------------------------------------------------------------------------
// Image conversion
// ---------------------------------------------------------------------------

CistaImage to_cista_image(const scene::ImageExperimental& img)
{
    CistaImage ci;
    ci.name = data::string(img.name.data(), img.name.size());

    ci.image.width = img.image.width;
    ci.image.height = img.image.height;
    ci.image.num_channels = img.image.num_channels;
    ci.image.element_type = static_cast<uint8_t>(img.image.element_type);
    ci.image.data_bytes.resize(img.image.data.size());
    if (!img.image.data.empty()) {
        std::memcpy(ci.image.data_bytes.data(), img.image.data.data(), img.image.data.size());
    }

    auto uri_str = img.uri.string();
    ci.uri = data::string(uri_str.data(), uri_str.size());

    ci.extensions = to_cista_extensions(img.extensions);
    return ci;
}

scene::ImageExperimental from_cista_image(const CistaImage& ci)
{
    scene::ImageExperimental img;
    img.name = std::string(ci.name.data(), ci.name.size());

    img.image.width = ci.image.width;
    img.image.height = ci.image.height;
    img.image.num_channels = ci.image.num_channels;
    img.image.element_type = static_cast<AttributeValueType>(ci.image.element_type);
    img.image.data.resize(ci.image.data_bytes.size());
    if (!ci.image.data_bytes.empty()) {
        std::memcpy(img.image.data.data(), ci.image.data_bytes.data(), ci.image.data_bytes.size());
    }

    img.uri = fs::path(std::string(ci.uri.data(), ci.uri.size()));

    img.extensions = from_cista_extensions(ci.extensions);
    return img;
}

// ---------------------------------------------------------------------------
// Texture conversion
// ---------------------------------------------------------------------------

CistaTexture to_cista_texture(const scene::Texture& tex)
{
    CistaTexture ct;
    ct.name = data::string(tex.name.data(), tex.name.size());
    ct.image = (tex.image == scene::invalid_element) ? k_invalid_element : tex.image;
    ct.mag_filter = static_cast<int32_t>(tex.mag_filter);
    ct.min_filter = static_cast<int32_t>(tex.min_filter);
    ct.wrap_u = static_cast<uint8_t>(tex.wrap_u);
    ct.wrap_v = static_cast<uint8_t>(tex.wrap_v);

    ct.scale = {tex.scale.x(), tex.scale.y()};
    ct.offset = {tex.offset.x(), tex.offset.y()};
    ct.rotation = tex.rotation;

    ct.extensions = to_cista_extensions(tex.extensions);
    return ct;
}

scene::Texture from_cista_texture(const CistaTexture& ct)
{
    scene::Texture tex;
    tex.name = std::string(ct.name.data(), ct.name.size());
    tex.image = (ct.image == k_invalid_element) ? scene::invalid_element : ct.image;
    tex.mag_filter = static_cast<scene::Texture::TextureFilter>(ct.mag_filter);
    tex.min_filter = static_cast<scene::Texture::TextureFilter>(ct.min_filter);
    tex.wrap_u = static_cast<scene::Texture::WrapMode>(ct.wrap_u);
    tex.wrap_v = static_cast<scene::Texture::WrapMode>(ct.wrap_v);

    tex.scale = Eigen::Vector2f(ct.scale[0], ct.scale[1]);
    tex.offset = Eigen::Vector2f(ct.offset[0], ct.offset[1]);
    tex.rotation = ct.rotation;

    tex.extensions = from_cista_extensions(ct.extensions);
    return tex;
}

// ---------------------------------------------------------------------------
// Material conversion
// ---------------------------------------------------------------------------

CistaMaterial to_cista_material(const scene::MaterialExperimental& mat)
{
    CistaMaterial cm;
    cm.name = data::string(mat.name.data(), mat.name.size());

    cm.base_color_value = {
        mat.base_color_value.x(),
        mat.base_color_value.y(),
        mat.base_color_value.z(),
        mat.base_color_value.w()};
    cm.emissive_value = {mat.emissive_value.x(), mat.emissive_value.y(), mat.emissive_value.z()};
    cm.metallic_value = mat.metallic_value;
    cm.roughness_value = mat.roughness_value;
    cm.alpha_cutoff = mat.alpha_cutoff;
    cm.normal_scale = mat.normal_scale;
    cm.occlusion_strength = mat.occlusion_strength;

    cm.alpha_mode = static_cast<uint8_t>(mat.alpha_mode);
    cm.double_sided = mat.double_sided;
    cm.base_color_texture = to_cista_texture_info(mat.base_color_texture);
    cm.emissive_texture = to_cista_texture_info(mat.emissive_texture);
    cm.metallic_roughness_texture = to_cista_texture_info(mat.metallic_roughness_texture);
    cm.normal_texture = to_cista_texture_info(mat.normal_texture);
    cm.occlusion_texture = to_cista_texture_info(mat.occlusion_texture);

    cm.extensions = to_cista_extensions(mat.extensions);
    return cm;
}

scene::MaterialExperimental from_cista_material(const CistaMaterial& cm)
{
    scene::MaterialExperimental mat;
    mat.name = std::string(cm.name.data(), cm.name.size());

    mat.base_color_value = Eigen::Vector4f(
        cm.base_color_value[0],
        cm.base_color_value[1],
        cm.base_color_value[2],
        cm.base_color_value[3]);
    mat.emissive_value =
        Eigen::Vector3f(cm.emissive_value[0], cm.emissive_value[1], cm.emissive_value[2]);
    mat.metallic_value = cm.metallic_value;
    mat.roughness_value = cm.roughness_value;
    mat.alpha_cutoff = cm.alpha_cutoff;
    mat.normal_scale = cm.normal_scale;
    mat.occlusion_strength = cm.occlusion_strength;

    mat.alpha_mode = static_cast<scene::MaterialExperimental::AlphaMode>(cm.alpha_mode);
    mat.double_sided = cm.double_sided;
    mat.base_color_texture = from_cista_texture_info(cm.base_color_texture);
    mat.emissive_texture = from_cista_texture_info(cm.emissive_texture);
    mat.metallic_roughness_texture = from_cista_texture_info(cm.metallic_roughness_texture);
    mat.normal_texture = from_cista_texture_info(cm.normal_texture);
    mat.occlusion_texture = from_cista_texture_info(cm.occlusion_texture);

    mat.extensions = from_cista_extensions(cm.extensions);
    return mat;
}

// ---------------------------------------------------------------------------
// Light conversion
// ---------------------------------------------------------------------------

CistaLight to_cista_light(const scene::Light& light)
{
    CistaLight cl;
    cl.name = data::string(light.name.data(), light.name.size());
    cl.type = static_cast<uint8_t>(light.type);

    cl.position = {light.position.x(), light.position.y(), light.position.z()};
    cl.direction = {light.direction.x(), light.direction.y(), light.direction.z()};
    cl.up = {light.up.x(), light.up.y(), light.up.z()};
    cl.intensity = light.intensity;
    cl.attenuation_constant = light.attenuation_constant;
    cl.attenuation_linear = light.attenuation_linear;
    cl.attenuation_quadratic = light.attenuation_quadratic;
    cl.attenuation_cubic = light.attenuation_cubic;
    cl.range = light.range;
    cl.color_diffuse = {light.color_diffuse.x(), light.color_diffuse.y(), light.color_diffuse.z()};
    cl.color_specular = {
        light.color_specular.x(),
        light.color_specular.y(),
        light.color_specular.z()};
    cl.color_ambient = {light.color_ambient.x(), light.color_ambient.y(), light.color_ambient.z()};
    if (light.angle_inner_cone) cl.angle_inner_cone = *light.angle_inner_cone;
    if (light.angle_outer_cone) cl.angle_outer_cone = *light.angle_outer_cone;
    cl.size = {light.size.x(), light.size.y()};

    cl.extensions = to_cista_extensions(light.extensions);
    return cl;
}

scene::Light from_cista_light(const CistaLight& cl)
{
    scene::Light light;
    light.name = std::string(cl.name.data(), cl.name.size());
    light.type = static_cast<scene::Light::Type>(cl.type);

    light.position = Eigen::Vector3f(cl.position[0], cl.position[1], cl.position[2]);
    light.direction = Eigen::Vector3f(cl.direction[0], cl.direction[1], cl.direction[2]);
    light.up = Eigen::Vector3f(cl.up[0], cl.up[1], cl.up[2]);
    light.intensity = cl.intensity;
    light.attenuation_constant = cl.attenuation_constant;
    light.attenuation_linear = cl.attenuation_linear;
    light.attenuation_quadratic = cl.attenuation_quadratic;
    light.attenuation_cubic = cl.attenuation_cubic;
    light.range = cl.range;
    light.color_diffuse =
        Eigen::Vector3f(cl.color_diffuse[0], cl.color_diffuse[1], cl.color_diffuse[2]);
    light.color_specular =
        Eigen::Vector3f(cl.color_specular[0], cl.color_specular[1], cl.color_specular[2]);
    light.color_ambient =
        Eigen::Vector3f(cl.color_ambient[0], cl.color_ambient[1], cl.color_ambient[2]);
    light.angle_inner_cone =
        cl.angle_inner_cone.has_value() ? std::optional<float>(*cl.angle_inner_cone) : std::nullopt;
    light.angle_outer_cone =
        cl.angle_outer_cone.has_value() ? std::optional<float>(*cl.angle_outer_cone) : std::nullopt;
    light.size = Eigen::Vector2f(cl.size[0], cl.size[1]);

    light.extensions = from_cista_extensions(cl.extensions);
    return light;
}

// ---------------------------------------------------------------------------
// Camera conversion
// ---------------------------------------------------------------------------

CistaCamera to_cista_camera(const scene::Camera& cam)
{
    CistaCamera cc;
    cc.name = data::string(cam.name.data(), cam.name.size());
    cc.type = static_cast<uint8_t>(cam.type);

    cc.position = {cam.position.x(), cam.position.y(), cam.position.z()};
    cc.up = {cam.up.x(), cam.up.y(), cam.up.z()};
    cc.look_at = {cam.look_at.x(), cam.look_at.y(), cam.look_at.z()};
    cc.near_plane = cam.near_plane;
    if (cam.far_plane) cc.far_plane = *cam.far_plane;
    cc.orthographic_width = cam.orthographic_width;
    cc.aspect_ratio = cam.aspect_ratio;
    cc.horizontal_fov = cam.horizontal_fov;

    cc.extensions = to_cista_extensions(cam.extensions);
    return cc;
}

scene::Camera from_cista_camera(const CistaCamera& cc)
{
    scene::Camera cam;
    cam.name = std::string(cc.name.data(), cc.name.size());
    cam.type = static_cast<scene::Camera::Type>(cc.type);

    cam.position = Eigen::Vector3f(cc.position[0], cc.position[1], cc.position[2]);
    cam.up = Eigen::Vector3f(cc.up[0], cc.up[1], cc.up[2]);
    cam.look_at = Eigen::Vector3f(cc.look_at[0], cc.look_at[1], cc.look_at[2]);
    cam.near_plane = cc.near_plane;
    cam.far_plane = cc.far_plane.has_value() ? std::optional<float>(*cc.far_plane) : std::nullopt;
    cam.orthographic_width = cc.orthographic_width;
    cam.aspect_ratio = cc.aspect_ratio;
    cam.horizontal_fov = cc.horizontal_fov;

    cam.extensions = from_cista_extensions(cc.extensions);
    return cam;
}

// ---------------------------------------------------------------------------
// Skeleton / Animation conversion
// ---------------------------------------------------------------------------

CistaSkeleton to_cista_skeleton(const scene::Skeleton& skel)
{
    CistaSkeleton cs;
    cs.meshes.reserve(skel.meshes.size());
    for (size_t i = 0; i < skel.meshes.size(); ++i) {
        cs.meshes.emplace_back(skel.meshes[i]);
    }
    cs.extensions = to_cista_extensions(skel.extensions);
    return cs;
}

scene::Skeleton from_cista_skeleton(const CistaSkeleton& cs)
{
    scene::Skeleton skel;
    for (const auto& m : cs.meshes) {
        skel.meshes.push_back(m);
    }
    skel.extensions = from_cista_extensions(cs.extensions);
    return skel;
}

CistaAnimation to_cista_animation(const scene::Animation& anim)
{
    CistaAnimation ca;
    ca.name = data::string(anim.name.data(), anim.name.size());
    ca.extensions = to_cista_extensions(anim.extensions);
    return ca;
}

scene::Animation from_cista_animation(const CistaAnimation& ca)
{
    scene::Animation anim;
    anim.name = std::string(ca.name.data(), ca.name.size());
    anim.extensions = from_cista_extensions(ca.extensions);
    return anim;
}

// ---------------------------------------------------------------------------
// Scene conversion
// ---------------------------------------------------------------------------

template <typename Scalar, typename Index>
CistaScene to_cista_scene(const scene::Scene<Scalar, Index>& scene)
{
    CistaScene cs;
    cs.scalar_type_size = sizeof(Scalar);
    cs.index_type_size = sizeof(Index);
    cs.name = data::string(scene.name.data(), scene.name.size());

    // Nodes
    cs.nodes.reserve(scene.nodes.size());
    for (size_t i = 0; i < scene.nodes.size(); ++i) {
        cs.nodes.emplace_back(to_cista_node(scene.nodes[i]));
    }

    // Root nodes
    cs.root_nodes.reserve(scene.root_nodes.size());
    for (size_t i = 0; i < scene.root_nodes.size(); ++i) {
        cs.root_nodes.emplace_back(scene.root_nodes[i]);
    }

    // Meshes
    cs.meshes.reserve(scene.meshes.size());
    for (size_t i = 0; i < scene.meshes.size(); ++i) {
        cs.meshes.emplace_back(to_cista_mesh(scene.meshes[i]));
    }

    // Images
    cs.images.reserve(scene.images.size());
    for (size_t i = 0; i < scene.images.size(); ++i) {
        cs.images.emplace_back(to_cista_image(scene.images[i]));
    }

    // Textures
    cs.textures.reserve(scene.textures.size());
    for (size_t i = 0; i < scene.textures.size(); ++i) {
        cs.textures.emplace_back(to_cista_texture(scene.textures[i]));
    }

    // Materials
    cs.materials.reserve(scene.materials.size());
    for (size_t i = 0; i < scene.materials.size(); ++i) {
        cs.materials.emplace_back(to_cista_material(scene.materials[i]));
    }

    // Lights
    cs.lights.reserve(scene.lights.size());
    for (size_t i = 0; i < scene.lights.size(); ++i) {
        cs.lights.emplace_back(to_cista_light(scene.lights[i]));
    }

    // Cameras
    cs.cameras.reserve(scene.cameras.size());
    for (size_t i = 0; i < scene.cameras.size(); ++i) {
        cs.cameras.emplace_back(to_cista_camera(scene.cameras[i]));
    }

    // Skeletons
    cs.skeletons.reserve(scene.skeletons.size());
    for (size_t i = 0; i < scene.skeletons.size(); ++i) {
        cs.skeletons.emplace_back(to_cista_skeleton(scene.skeletons[i]));
    }

    // Animations
    cs.animations.reserve(scene.animations.size());
    for (size_t i = 0; i < scene.animations.size(); ++i) {
        cs.animations.emplace_back(to_cista_animation(scene.animations[i]));
    }

    // Scene-level extensions
    cs.extensions = to_cista_extensions(scene.extensions);

    return cs;
}

template <typename Scalar, typename Index>
scene::Scene<Scalar, Index> from_cista_scene(const CistaScene& cs)
{
    la_runtime_assert(
        cs.version == scene_format_version(),
        "Unsupported encoding format version: expected " + std::to_string(scene_format_version()) +
            ", got " + std::to_string(cs.version));
    la_runtime_assert(
        cs.scalar_type_size == sizeof(Scalar),
        "Scalar type size mismatch: expected " + std::to_string(sizeof(Scalar)) + ", got " +
            std::to_string(cs.scalar_type_size));
    la_runtime_assert(
        cs.index_type_size == sizeof(Index),
        "Index type size mismatch: expected " + std::to_string(sizeof(Index)) + ", got " +
            std::to_string(cs.index_type_size));

    scene::Scene<Scalar, Index> sc;
    sc.name = std::string(cs.name.data(), cs.name.size());

    // Nodes
    for (const auto& cn : cs.nodes) {
        sc.nodes.push_back(from_cista_node(cn));
    }

    // Root nodes
    for (const auto& rn : cs.root_nodes) {
        sc.root_nodes.push_back(rn);
    }

    // Meshes
    for (const auto& cm : cs.meshes) {
        sc.meshes.push_back(from_cista_mesh<Scalar, Index>(cm));
    }

    // Images
    for (const auto& ci : cs.images) {
        sc.images.push_back(from_cista_image(ci));
    }

    // Textures
    for (const auto& ct : cs.textures) {
        sc.textures.push_back(from_cista_texture(ct));
    }

    // Materials
    for (const auto& cm : cs.materials) {
        sc.materials.push_back(from_cista_material(cm));
    }

    // Lights
    for (const auto& cl : cs.lights) {
        sc.lights.push_back(from_cista_light(cl));
    }

    // Cameras
    for (const auto& cc : cs.cameras) {
        sc.cameras.push_back(from_cista_camera(cc));
    }

    // Skeletons
    for (const auto& cskel : cs.skeletons) {
        sc.skeletons.push_back(from_cista_skeleton(cskel));
    }

    // Animations
    for (const auto& ca : cs.animations) {
        sc.animations.push_back(from_cista_animation(ca));
    }

    // Scene-level extensions
    sc.extensions = from_cista_extensions(cs.extensions);

    return sc;
}

/// Deserialize a CistaScene with native Scalar/Index types, then cast meshes to target types.
template <typename NativeScalar, typename NativeIndex, typename ToScalar, typename ToIndex>
scene::Scene<ToScalar, ToIndex> cast_cista_scene(const CistaScene& cs)
{
    auto native = from_cista_scene<NativeScalar, NativeIndex>(cs);
    scene::Scene<ToScalar, ToIndex> result;
    result.name = std::move(native.name);
    result.nodes = std::move(native.nodes);
    result.root_nodes = std::move(native.root_nodes);
    result.meshes.reserve(native.meshes.size());
    for (auto& mesh : native.meshes) {
        result.meshes.push_back(lagrange::cast<ToScalar, ToIndex>(mesh));
    }
    result.images = std::move(native.images);
    result.textures = std::move(native.textures);
    result.materials = std::move(native.materials);
    result.lights = std::move(native.lights);
    result.cameras = std::move(native.cameras);
    result.skeletons = std::move(native.skeletons);
    result.animations = std::move(native.animations);
    result.extensions = std::move(native.extensions);
    return result;
}

/// Deserialize a CistaScene buffer with runtime dispatch on stored Scalar/Index types, then
/// cast to the requested <ToScalar, ToIndex> types.
template <typename ToScalar, typename ToIndex>
scene::Scene<ToScalar, ToIndex> deserialize_scene_with_cast(span<const uint8_t> buffer)
{
    const auto* cs =
        cista::deserialize<CistaScene, k_cista_mode>(buffer.data(), buffer.data() + buffer.size());
    const uint8_t ss = cs->scalar_type_size;
    const uint8_t is = cs->index_type_size;

    if (ss == sizeof(float) && is == sizeof(uint32_t)) {
        return cast_cista_scene<float, uint32_t, ToScalar, ToIndex>(*cs);
    } else if (ss == sizeof(double) && is == sizeof(uint32_t)) {
        return cast_cista_scene<double, uint32_t, ToScalar, ToIndex>(*cs);
    } else if (ss == sizeof(float) && is == sizeof(uint64_t)) {
        return cast_cista_scene<float, uint64_t, ToScalar, ToIndex>(*cs);
    } else if (ss == sizeof(double) && is == sizeof(uint64_t)) {
        return cast_cista_scene<double, uint64_t, ToScalar, ToIndex>(*cs);
    } else {
        throw std::runtime_error(
            "Unsupported scalar/index type sizes: scalar=" + std::to_string(ss) +
            " index=" + std::to_string(is));
    }
}

} // namespace internal

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

template <typename Scalar, typename Index>
std::vector<uint8_t> serialize_scene(
    const scene::Scene<Scalar, Index>& scene,
    const SerializeOptions& options)
{
    auto cscene = internal::to_cista_scene(scene);

    cista::buf<std::vector<uint8_t>> buf;
    cista::serialize<k_cista_mode>(buf, cscene);

    if (options.compress) {
        return compress_buffer(buf.buf_, options.compression_level, options.num_threads);
    }

    return std::move(buf.buf_);
}

template <typename SceneType>
SceneType deserialize_scene(span<const uint8_t> buffer, const DeserializeOptions& options)
{
    using Scalar = typename SceneType::MeshType::Scalar;
    using Index = typename SceneType::MeshType::Index;

    // Decompress if needed
    std::vector<uint8_t> decompressed_storage;
    span<const uint8_t> data = buffer;
    if (is_compressed(buffer)) {
        decompressed_storage = decompress_buffer(buffer);
        data = span<const uint8_t>(decompressed_storage);
    }

    auto load_native_scene = [&]() -> SceneType {
        const auto* cscene = cista::deserialize<internal::CistaScene, k_cista_mode>(
            data.data(),
            data.data() + data.size());
        if (cscene->scalar_type_size != sizeof(Scalar) ||
            cscene->index_type_size != sizeof(Index)) {
            if (!options.allow_type_cast) {
                throw std::runtime_error(
                    "Scalar/Index type mismatch: buffer has scalar_size=" +
                    std::to_string(cscene->scalar_type_size) +
                    " index_size=" + std::to_string(cscene->index_type_size) +
                    ", expected scalar_size=" + std::to_string(sizeof(Scalar)) +
                    " index_size=" + std::to_string(sizeof(Index)));
            }
            if (!options.quiet) {
                logger().warn(
                    "Casting Scene types: buffer has scalar_size={} index_size={}, "
                    "requested scalar_size={} index_size={}",
                    cscene->scalar_type_size,
                    cscene->index_type_size,
                    sizeof(Scalar),
                    sizeof(Index));
            }
            return internal::deserialize_scene_with_cast<Scalar, Index>(data);
        }
        return internal::from_cista_scene<Scalar, Index>(*cscene);
    };

    if (!options.allow_scene_conversion) {
        return load_native_scene();
    }

    auto type = internal::detect_encoded_type(data);
    switch (type) {
    case internal::EncodedType::Scene: return load_native_scene();
    case internal::EncodedType::Mesh: {
        if (!options.quiet) {
            logger().warn("Buffer contains a Mesh, converting to Scene");
        }
        DeserializeOptions native_opts;
        native_opts.allow_scene_conversion = false;
        auto mesh = deserialize_mesh<SurfaceMesh<Scalar, Index>>(data, native_opts);
        return scene::mesh_to_scene(std::move(mesh));
    }
    case internal::EncodedType::SimpleScene: {
        if (!options.quiet) {
            logger().warn("Buffer contains a SimpleScene, converting to Scene");
        }
        DeserializeOptions native_opts;
        native_opts.allow_scene_conversion = false;
        auto simple_scene =
            deserialize_simple_scene<scene::SimpleScene<Scalar, Index, 3>>(data, native_opts);
        return scene::simple_scene_to_scene(simple_scene);
    }
    default: throw std::runtime_error("Unknown encoded data type in buffer");
    }
}

template <typename Scalar, typename Index>
void save_scene(
    const fs::path& filename,
    const scene::Scene<Scalar, Index>& scene,
    const SerializeOptions& options)
{
    auto buf = serialize_scene(scene, options);
    fs::ofstream ofs(filename, std::ios::binary);
    la_runtime_assert(ofs.good(), "Failed to open file for writing: " + filename.string());
    ofs.write(reinterpret_cast<const char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
    la_runtime_assert(ofs.good(), "Failed to write to file: " + filename.string());
}

template <typename SceneType>
SceneType load_scene(const fs::path& filename, const DeserializeOptions& options)
{
    auto buf = internal::read_file_to_buffer(filename);
    if (is_compressed(buf)) {
        buf = decompress_buffer(buf);
    }
    return deserialize_scene<SceneType>(lagrange::span<const uint8_t>(buf), options);
}

// Explicit template instantiations
#define LA_X_serialization2_sc(_, Scalar, Index)                                        \
    template LA_SERIALIZATION2_API std::vector<uint8_t> serialize_scene<Scalar, Index>( \
        const scene::Scene<Scalar, Index>&,                                             \
        const SerializeOptions&);                                                       \
    template LA_SERIALIZATION2_API scene::Scene<Scalar, Index>                          \
    deserialize_scene<scene::Scene<Scalar, Index>>(                                     \
        span<const uint8_t>,                                                            \
        const DeserializeOptions&);                                                     \
    template LA_SERIALIZATION2_API void save_scene<Scalar, Index>(                      \
        const fs::path&,                                                                \
        const scene::Scene<Scalar, Index>&,                                             \
        const SerializeOptions&);                                                       \
    template LA_SERIALIZATION2_API scene::Scene<Scalar, Index>                          \
    load_scene<scene::Scene<Scalar, Index>>(const fs::path&, const DeserializeOptions&);
LA_SCENE_X(serialization2_sc, 0)
#undef LA_X_serialization2_sc

} // namespace lagrange::serialization
