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
#pragma once

#include <lagrange/Logger.h>
#include <lagrange/scene/Scene.h>
#include <lagrange/scene/scene_utils.h>
#include <lagrange/utils/assert.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <Eigen/Core>
#include <nanobind/nanobind.h>
#include <nanobind/eigen/dense.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/filesystem.h>
#include <nanobind/stl/vector.h>
#include <nanobind/stl/array.h>
#include <nanobind/stl/shared_ptr.h> // for ImageLegacy::data
#include <lagrange/utils/warnon.h>
// clang-format on

namespace lagrange::python {

namespace nb = nanobind;

namespace detail {
std::array<std::array<float, 4>, 4> affine3d_to_array(const Eigen::Affine3f& t)
{
    std::array<std::array<float, 4>, 4> data;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            data[i][j] = t(i, j);
        }
    }
    return data;
};
Eigen::Affine3f array_to_affine3d(const std::array<std::array<float, 4>, 4> data)
{
    Eigen::Affine3f t;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            t(i, j) = data[i][j];
        }
    }
    return t;
};
} // namespace detail

void bind_scene(nb::module_& m)
{
    using namespace lagrange::scene;
    using Scalar = double;
    using Index = uint32_t;
    using SceneType = Scene<Scalar, Index>;


    nb::class_<SceneMeshInstance>(m, "SceneMeshInstance", "Mesh and material index of a node")
        .def(nb::init<>())
        .def_rw("mesh", &SceneMeshInstance::mesh)
        .def_rw("materials", &SceneMeshInstance::materials);

    nb::class_<Node>(m, "Node")
        .def(nb::init<>())
        .def("__repr__", [](const Node& node) { return fmt::format("Node('{}')", node.name); })
        .def_rw("name", &Node::name)
        .def_prop_rw(
            "transform",
            // nanobind does not know the Eigen::Affine3f type.
            // Eigen::Matrix4f compiles but returns bad data, so here's 2d arrays instead.
            [](const Node& node) -> std::array<std::array<float, 4>, 4> {
                return detail::affine3d_to_array(node.transform);
            },
            [](Node& node, std::array<std::array<float, 4>, 4> t) -> void {
                node.transform = detail::array_to_affine3d(t);
            })
        .def_rw("parent", &Node::parent)
        .def_rw("children", &Node::children)
        .def_rw("meshes", &Node::meshes)
        .def_rw("cameras", &Node::cameras)
        .def_rw("lights", &Node::lights);

    nb::class_<ImageLegacy> image_legacy(m, "ImageLegacy");
    image_legacy.def(nb::init<>())
        .def(
            "__repr__",
            [](const ImageLegacy& image) {
                return fmt::format(
                    "ImageLegacy('{}')",
                    image.name.empty() ? image.uri : image.name);
            })
        .def_rw("name", &ImageLegacy::name)
        .def_rw("width", &ImageLegacy::width)
        .def_rw("height", &ImageLegacy::height)
        .def_rw("precision", &ImageLegacy::precision)
        .def_rw("channel", &ImageLegacy::channel)
        .def_rw("data", &ImageLegacy::data)
        .def_rw("uri", &ImageLegacy::uri)
        .def_rw("type", &ImageLegacy::type)
        .def_prop_ro("num_channels", &ImageLegacy::get_num_channels)
        .def_prop_ro("element_size", &ImageLegacy::get_element_size);
    nb::enum_<ImageLegacy::Type>(image_legacy, "Type")
        .value("Jpeg", ImageLegacy::Type::Jpeg)
        .value("Png", ImageLegacy::Type::Png)
        .value("Bmp", ImageLegacy::Type::Bmp)
        .value("Gif", ImageLegacy::Type::Gif)
        .value("Unknown", ImageLegacy::Type::Unknown);

    nb::class_<TextureInfo>(m, "TextureInfo")
        .def(nb::init<>())
        .def_rw("index", &TextureInfo::index)
        .def_rw("texcoord", &TextureInfo::texcoord);

    nb::class_<MaterialExperimental> material(m, "Material");
    material.def(nb::init<>())
        .def(
            "__repr__",
            [](const MaterialExperimental& mat) { return fmt::format("Material('{}')", mat.name); })
        .def_rw("name", &MaterialExperimental::name)
        .def_rw("base_color_value", &MaterialExperimental::base_color_value)
        .def_rw("base_color_texture", &MaterialExperimental::base_color_texture)
        .def_rw("alpha_mode", &MaterialExperimental::alpha_mode)
        .def_rw("alpha_cutoff", &MaterialExperimental::alpha_cutoff)
        .def_rw("emissive_value", &MaterialExperimental::emissive_value)
        .def_rw("emissive_texture", &MaterialExperimental::emissive_texture)
        .def_rw("metallic_value", &MaterialExperimental::metallic_value)
        .def_rw("roughness_value", &MaterialExperimental::roughness_value)
        .def_rw("metallic_roughness_texture", &MaterialExperimental::metallic_roughness_texture)
        .def_rw("normal_texture", &MaterialExperimental::normal_texture)
        .def_rw("normal_scale", &MaterialExperimental::normal_scale)
        .def_rw("occlusion_texture", &MaterialExperimental::occlusion_texture)
        .def_rw("occlusion_strength", &MaterialExperimental::occlusion_strength)
        .def_rw("double_sided", &MaterialExperimental::double_sided);
    nb::enum_<MaterialExperimental::AlphaMode>(material, "AlphaMode")
        .value("Opaque", MaterialExperimental::AlphaMode::Opaque)
        .value("Mask", MaterialExperimental::AlphaMode::Mask)
        .value("Blend", MaterialExperimental::AlphaMode::Blend);


    nb::class_<Texture> texture(m, "Texture", "Texture");
    texture.def(nb::init<>())
        .def("__repr__", [](const Texture& t) { return fmt::format("Texture('{}')", t.name); })
        .def_rw("name", &Texture::name)
        .def_rw("image", &Texture::image)
        .def_rw("mag_filter", &Texture::mag_filter)
        .def_rw("min_filter", &Texture::min_filter)
        .def_rw("wrap_u", &Texture::wrap_u)
        .def_rw("wrap_v", &Texture::wrap_v)
        .def_rw("scale", &Texture::scale)
        .def_rw("offset", &Texture::offset)
        .def_rw("rotation", &Texture::rotation);
    nb::enum_<Texture::WrapMode>(texture, "WrapMode")
        .value("Wrap", Texture::WrapMode::Wrap)
        .value("Clamp", Texture::WrapMode::Clamp)
        .value("Decal", Texture::WrapMode::Decal)
        .value("Mirror", Texture::WrapMode::Mirror);
    nb::enum_<Texture::TextureFilter>(texture, "TextureFilter")
        .value("Undefined", Texture::TextureFilter::Undefined)
        .value("Nearest", Texture::TextureFilter::Nearest)
        .value("Linear", Texture::TextureFilter::Linear)
        .value("NearestMimpapNearest", Texture::TextureFilter::NearestMimpapNearest)
        .value("LinearMipmapNearest", Texture::TextureFilter::LinearMipmapNearest)
        .value("NearestMipmapLinear", Texture::TextureFilter::NearestMipmapLinear)
        .value("LinearMipmapLinear", Texture::TextureFilter::LinearMipmapLinear);

    nb::class_<Light> light(m, "Light", "Light");
    light.def(nb::init<>())
        .def("__repr__", [](const Light& l) { return fmt::format("Light('{}')", l.name); })
        .def_rw("name", &Light::name)
        .def_rw("type", &Light::type)
        .def_rw("position", &Light::position)
        .def_rw("direction", &Light::direction)
        .def_rw("up", &Light::up)
        .def_rw("intensity", &Light::intensity)
        .def_rw("attenuation_constant", &Light::attenuation_constant)
        .def_rw("attenuation_linear", &Light::attenuation_linear)
        .def_rw("attenuation_quadratic", &Light::attenuation_quadratic)
        .def_rw("attenuation_cubic", &Light::attenuation_cubic)
        .def_rw("range", &Light::range)
        .def_rw("color_diffuse", &Light::color_diffuse)
        .def_rw("color_specular", &Light::color_specular)
        .def_rw("color_ambient", &Light::color_ambient)
        .def_rw("angle_inner_cone", &Light::angle_inner_cone)
        .def_rw("angle_outer_cone", &Light::angle_outer_cone)
        .def_rw("size", &Light::size);
    nb::enum_<Light::Type>(light, "Type")
        .value("Undefined", Light::Type::Undefined)
        .value("Directional", Light::Type::Directional)
        .value("Point", Light::Type::Point)
        .value("Spot", Light::Type::Spot)
        .value("Ambient", Light::Type::Ambient)
        .value("Area", Light::Type::Area);

    nb::class_<Camera> camera(m, "Camera", "Camera");
    camera.def(nb::init<>())
        .def("__repr__", [](const Camera& c) { return fmt::format("Camera('{}')", c.name); })
        .def_rw("name", &Camera::name)
        .def_rw("position", &Camera::position)
        .def_rw("up", &Camera::up)
        .def_rw("look_at", &Camera::look_at)
        .def_rw("near_plane", &Camera::near_plane)
        .def_rw("far_plane", &Camera::far_plane)
        .def_rw("type", &Camera::type)
        .def_rw("aspect_ratio", &Camera::aspect_ratio)
        .def_rw("horizontal_fov", &Camera::horizontal_fov)
        .def_rw("orthographic_width", &Camera::orthographic_width)
        .def_prop_ro("get_vertical_fov", &Camera::get_vertical_fov)
        .def_prop_ro(
            "set_horizontal_fov_from_vertical_fov",
            &Camera::set_horizontal_fov_from_vertical_fov,
            "vfov"_a);
    nb::enum_<Camera::Type>(camera, "Type")
        .value("Perspective", Camera::Type::Perspective)
        .value("Orthographic", Camera::Type::Orthographic);

    nb::class_<Animation>(m, "Animation", "")
        .def(nb::init<>())
        .def("__repr__", [](const Animation& a) { return fmt::format("Animation('{}')", a.name); })
        .def_rw("name", &Animation::name);

    nb::class_<Skeleton>(m, "Skeleton", "").def(nb::init<>()).def_rw("meshes", &Skeleton::meshes);

    nb::class_<SceneType>(m, "Scene", "A 3D scene")
        .def(nb::init<>())
        .def("__repr__", [](const SceneType& s) { return fmt::format("Scene('{}')", s.name); })
        .def_rw("name", &SceneType::name)
        .def_rw("nodes", &SceneType::nodes)
        .def_rw("meshes", &SceneType::meshes)
        .def_rw("images", &SceneType::images)
        .def_rw("textures", &SceneType::textures)
        .def_rw("materials", &SceneType::materials)
        .def_rw("lights", &SceneType::lights)
        .def_rw("cameras", &SceneType::cameras)
        .def_rw("skeletons", &SceneType::skeletons)
        .def_rw("animations", &SceneType::animations);

    m.def("add_child", &utils::add_child<Scalar, Index>);

    m.def("add_mesh", &utils::add_mesh<Scalar, Index>);

    m.def("compute_global_node_transform", [](const SceneType& scene, size_t node_idx) {
        return detail::affine3d_to_array(
            utils::compute_global_node_transform<Scalar, Index>(scene, node_idx));
    });
}

} // namespace lagrange::python
