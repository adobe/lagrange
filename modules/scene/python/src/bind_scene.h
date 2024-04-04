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

#include <lagrange/AttributeValueType.h>
#include <lagrange/Logger.h>
#include <lagrange/python/tensor_utils.h>
#include <lagrange/scene/Scene.h>
#include <lagrange/scene/scene_utils.h>
#include <lagrange/utils/assert.h>

#include "bind_value.h"

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <Eigen/Core>
#include <nanobind/nanobind.h>
#include <nanobind/eigen/dense.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/filesystem.h>
#include <nanobind/stl/array.h>
#include <nanobind/stl/variant.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/bind_vector.h>
#include <nanobind/stl/bind_map.h>
#include <nanobind/trampoline.h>
#include <lagrange/utils/warnon.h>
// clang-format on


namespace lagrange::python {

namespace nb = nanobind;

#pragma GCC visibility push(hidden)
struct UserDataConverterTrampoline : public lagrange::scene::UserDataConverter
{
    NB_TRAMPOLINE(lagrange::scene::UserDataConverter, 5);

    bool is_supported(const std::string& key) const override
    {
        NB_OVERRIDE_PURE(is_supported, key);
    }
    bool can_read(const std::string& key) const override { NB_OVERRIDE(can_read, key); }
    bool can_write(const std::string& key) const override { NB_OVERRIDE(can_write, key); }
    std::any read(const lagrange::scene::Value& value) const override { NB_OVERRIDE(read, value); }
    lagrange::scene::Value write(const std::any& value) const override
    {
        NB_OVERRIDE(write, value);
    }
};
#pragma GCC visibility pop

void bind_scene(nb::module_& m)
{
    using namespace lagrange::scene;
    using Scalar = double;
    using Index = uint32_t;
    using SceneType = Scene<Scalar, Index>;

    nb::bind_vector<std::vector<Node>>(m, "NodeList");
    nb::bind_vector<std::vector<ElementId>>(m, "ElementIdList");
    nb::bind_vector<std::vector<SceneMeshInstance>>(m, "SceneMeshInstanceList");
    nb::bind_vector<std::vector<SurfaceMesh<Scalar, Index>>>(m, "SurfaceMeshList");
    nb::bind_vector<std::vector<ImageExperimental>>(m, "ImageList");
    nb::bind_vector<std::vector<Texture>>(m, "TextureList");
    nb::bind_vector<std::vector<MaterialExperimental>>(m, "MaterialList");
    nb::bind_vector<std::vector<Light>>(m, "LightList");
    nb::bind_vector<std::vector<Camera>>(m, "CameraList");
    nb::bind_vector<std::vector<Skeleton>>(m, "SkeletonList");
    nb::bind_vector<std::vector<Animation>>(m, "AnimationList");


    nb::bind_vector<std::vector<lagrange::scene::Value>>(m, "ValueList");
    nb::bind_vector<std::vector<unsigned char>>(m, "BufferList");
    nb::bind_map<std::unordered_map<std::string, lagrange::scene::Value>>(m, "ValueUnorderedMap");
    nb::bind_map<std::map<std::string, lagrange::scene::Value>>(m, "ValueMap");
    // nb::bind_map<std::unordered_map<std::string, std::any>>(m, "anyMap");


    nb::class_<lagrange::scene::Extensions>(m, "Extensions")
        .def_prop_ro("size", &Extensions::size)
        .def_prop_ro("empty", &Extensions::empty)
        .def_rw("data", &Extensions::data);
    // .def_prop_rw(
    //     "user_data",
    //     [](const Extensions& ext) -> std::unordered_map<std::string, void*> {
    //         std::unordered_map<std::string, void*> ret;
    //         for (const auto& [key, val] : ext.user_data) {
    //             ret.insert({key, std::any_cast<void*>(val)});
    //         }
    //         return ret;
    //     },
    //     [](Extensions& ext, std::unordered_map<std::string, void*> data) -> void {
    //         ext.user_data.clear();
    //         for (const auto& [key, val] : data) {
    //             ext.user_data.insert({key, val});
    //         }
    //     });

    // nb::class_<UserDataConverter, UserDataConverterTrampoline>(m, "UserExtension")
    //     .def("is_supported", &UserDataConverter::is_supported)
    //     .def("can_read", &UserDataConverter::can_read)
    //     .def("can_write", &UserDataConverter::can_write)
    //     .def("read", &UserDataConverter::read)
    //     .def("write", &UserDataConverter::write);

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
            [](Node& node) {
                return nb::ndarray<nb::numpy, float, nb::f_contig, nb::shape<4, 4>>(
                    node.transform.data(),
                    {4, 4},
                    nb::find(node),
                    {1, 4});
            },
            [](Node& node, nb::ndarray<nb::numpy, const float, nb::shape<4, 4>> t) -> void {
                auto view = t.view<float, nb::ndim<2>>();
                // Explicit 2D indexing because the input ndarray can be either row or column major.
                for (size_t i = 0; i < 4; i++) {
                    for (size_t j = 0; j < 4; j++) {
                        node.transform.data()[i + j * 4] = view(i, j);
                    }
                }
            },
            "The affine transform associated with this node")
        .def_rw("parent", &Node::parent)
        .def_rw("children", &Node::children)
        .def_rw("meshes", &Node::meshes)
        .def_rw("cameras", &Node::cameras)
        .def_rw("lights", &Node::lights)
        .def_rw("extensions", &Node::extensions);

    nb::class_<ImageBufferExperimental> image_buffer(m, "ImageBuffer");
    image_buffer.def(nb::init<>())
        .def_ro("width", &ImageBufferExperimental::width, "Image width")
        .def_ro("height", &ImageBufferExperimental::height, "Image height")
        .def_ro(
            "num_channels",
            &ImageBufferExperimental::num_channels,
            "Number of channels in each pixel")
        .def_prop_rw(
            "data",
            [](ImageBufferExperimental& self) {
                size_t shape[3] = {self.height, self.width, self.num_channels};
                switch (self.element_type) {
                case AttributeValueType::e_int8_t:
                    return nb::cast(
                        nb::ndarray<int8_t, nb::numpy, nb::c_contig, nb::device::cpu>(
                            reinterpret_cast<int8_t*>(self.data.data()),
                            3,
                            shape,
                            nb::find(self)),
                        nb::rv_policy::reference_internal);
                case AttributeValueType::e_uint8_t:
                    return nb::cast(
                        nb::ndarray<uint8_t, nb::numpy, nb::c_contig, nb::device::cpu>(
                            reinterpret_cast<uint8_t*>(self.data.data()),
                            3,
                            shape,
                            nb::find(self)),
                        nb::rv_policy::reference_internal);
                case AttributeValueType::e_int16_t:
                    return nb::cast(
                        nb::ndarray<int16_t, nb::numpy, nb::c_contig, nb::device::cpu>(
                            reinterpret_cast<int16_t*>(self.data.data()),
                            3,
                            shape,
                            nb::find(self)),
                        nb::rv_policy::reference_internal);
                case AttributeValueType::e_uint16_t:
                    return nb::cast(
                        nb::ndarray<uint16_t, nb::numpy, nb::c_contig, nb::device::cpu>(
                            reinterpret_cast<uint16_t*>(self.data.data()),
                            3,
                            shape,
                            nb::find(self)),
                        nb::rv_policy::reference_internal);
                case AttributeValueType::e_int32_t:
                    return nb::cast(
                        nb::ndarray<int32_t, nb::numpy, nb::c_contig, nb::device::cpu>(
                            reinterpret_cast<int32_t*>(self.data.data()),
                            3,
                            shape,
                            nb::find(self)),
                        nb::rv_policy::reference_internal);
                case AttributeValueType::e_uint32_t:
                    return nb::cast(
                        nb::ndarray<uint32_t, nb::numpy, nb::c_contig, nb::device::cpu>(
                            reinterpret_cast<uint32_t*>(self.data.data()),
                            3,
                            shape,
                            nb::find(self)),
                        nb::rv_policy::reference_internal);
                case AttributeValueType::e_int64_t:
                    return nb::cast(
                        nb::ndarray<int64_t, nb::numpy, nb::c_contig, nb::device::cpu>(
                            reinterpret_cast<int64_t*>(self.data.data()),
                            3,
                            shape,
                            nb::find(self)),
                        nb::rv_policy::reference_internal);
                case AttributeValueType::e_uint64_t:
                    return nb::cast(
                        nb::ndarray<uint64_t, nb::numpy, nb::c_contig, nb::device::cpu>(
                            reinterpret_cast<uint64_t*>(self.data.data()),
                            3,
                            shape,
                            nb::find(self)),
                        nb::rv_policy::reference_internal);
                case AttributeValueType::e_float:
                    return nb::cast(
                        nb::ndarray<float, nb::numpy, nb::c_contig, nb::device::cpu>(
                            reinterpret_cast<float*>(self.data.data()),
                            3,
                            shape,
                            nb::find(self)),
                        nb::rv_policy::reference_internal);
                case AttributeValueType::e_double:
                    return nb::cast(
                        nb::ndarray<double, nb::numpy, nb::c_contig, nb::device::cpu>(
                            reinterpret_cast<double*>(self.data.data()),
                            3,
                            shape,
                            nb::find(self)),
                        nb::rv_policy::reference_internal);
                default: throw nb::type_error("Unsupported image buffer `dtype`!");
                }
            },
            [](ImageBufferExperimental& self,
               nb::ndarray<nb::numpy, nb::c_contig, nb::device::cpu> tensor) {
                la_runtime_assert(tensor.ndim() == 3);
                self.width = tensor.shape(1);
                self.height = tensor.shape(0);
                self.num_channels = tensor.shape(2);
                auto dtype = tensor.dtype();
                if (dtype == nb::dtype<int8_t>()) {
                    self.element_type = AttributeValueType::e_int8_t;
                } else if (dtype == nb::dtype<uint8_t>()) {
                    self.element_type = AttributeValueType::e_uint8_t;
                } else if (dtype == nb::dtype<int16_t>()) {
                    self.element_type = AttributeValueType::e_int16_t;
                } else if (dtype == nb::dtype<uint16_t>()) {
                    self.element_type = AttributeValueType::e_uint16_t;
                } else if (dtype == nb::dtype<int32_t>()) {
                    self.element_type = AttributeValueType::e_int32_t;
                } else if (dtype == nb::dtype<uint32_t>()) {
                    self.element_type = AttributeValueType::e_uint32_t;
                } else if (dtype == nb::dtype<int64_t>()) {
                    self.element_type = AttributeValueType::e_int64_t;
                } else if (dtype == nb::dtype<uint64_t>()) {
                    self.element_type = AttributeValueType::e_uint64_t;
                } else if (dtype == nb::dtype<float>()) {
                    self.element_type = AttributeValueType::e_float;
                } else if (dtype == nb::dtype<double>()) {
                    self.element_type = AttributeValueType::e_double;
                } else {
                    throw nb::type_error("Unsupported input tensor `dtype`!");
                }
                self.data.resize(tensor.nbytes());
                std::copy(
                    reinterpret_cast<uint8_t*>(tensor.data()),
                    reinterpret_cast<uint8_t*>(tensor.data()) + tensor.nbytes(),
                    self.data.data());
            },
            "Raw image data.")
        .def_prop_ro(
            "dtype",
            [](ImageBufferExperimental& self) -> std::optional<nb::type_object> {
                auto np = nb::module_::import_("numpy");
                switch (self.element_type) {
                case AttributeValueType::e_int8_t: return np.attr("int8");
                case AttributeValueType::e_int16_t: return np.attr("int16");
                case AttributeValueType::e_int32_t: return np.attr("int32");
                case AttributeValueType::e_int64_t: return np.attr("int64");
                case AttributeValueType::e_uint8_t: return np.attr("uint8");
                case AttributeValueType::e_uint16_t: return np.attr("uint16");
                case AttributeValueType::e_uint32_t: return np.attr("uint32");
                case AttributeValueType::e_uint64_t: return np.attr("uint64");
                case AttributeValueType::e_float: return np.attr("float32");
                case AttributeValueType::e_double: return np.attr("float64");
                default: logger().warn("Image buffer has an unknown dtype."); return std::nullopt;
                }
            },
            "The element data type of the image buffer.");

    nb::class_<ImageExperimental> image(m, "Image");
    image.def(nb::init<>())
        .def_rw("name", &ImageExperimental::name, "Name of the image object")
        .def_rw("image", &ImageExperimental::image, "Image buffer")
        .def_rw("uri", &ImageExperimental::uri, "URI of the image file")
        .def_rw(
            "extensions",
            &ImageExperimental::extensions,
            "Additional data associated with the image")
        .def("__repr__", [](const ImageExperimental& self) {
            return fmt::format("Image ('{}')", self.name);
        });

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
        .def_rw("double_sided", &MaterialExperimental::double_sided)
        .def_rw("extensions", &MaterialExperimental::extensions);

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
        .def_rw("rotation", &Texture::rotation)
        .def_rw("extensions", &Texture::extensions);

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
        .def_rw("size", &Light::size)
        .def_rw("extensions", &Light::extensions);

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
            "vfov"_a)
        .def_rw("extensions", &Camera::extensions);

    nb::enum_<Camera::Type>(camera, "Type")
        .value("Perspective", Camera::Type::Perspective)
        .value("Orthographic", Camera::Type::Orthographic);

    nb::class_<Animation>(m, "Animation", "")
        .def(nb::init<>())
        .def("__repr__", [](const Animation& a) { return fmt::format("Animation('{}')", a.name); })
        .def_rw("name", &Animation::name)
        .def_rw("extensions", &Animation::extensions);


    nb::class_<Skeleton>(m, "Skeleton", "")
        .def(nb::init<>())
        .def_rw("meshes", &Skeleton::meshes)
        .def_rw("extensions", &Skeleton::extensions);


    nb::class_<SceneType>(m, "Scene", "A 3D scene")
        .def(nb::init<>())
        .def("__repr__", [](const SceneType& s) { return fmt::format("Scene('{}')", s.name); })
        .def_rw("name", &SceneType::name)
        .def_rw("nodes", &SceneType::nodes)
        .def_rw("root_nodes", &SceneType::root_nodes)
        .def_rw("meshes", &SceneType::meshes)
        .def_rw("images", &SceneType::images)
        .def_rw("textures", &SceneType::textures)
        .def_rw("materials", &SceneType::materials)
        .def_rw("lights", &SceneType::lights)
        .def_rw("cameras", &SceneType::cameras)
        .def_rw("skeletons", &SceneType::skeletons)
        .def_rw("animations", &SceneType::animations)
        .def_rw("extensions", &SceneType::extensions)
        .def(
            "add",
            [](SceneType& self,
               std::variant<
                   Node,
                   SceneType::MeshType,
                   ImageExperimental,
                   Texture,
                   MaterialExperimental,
                   Light,
                   Camera,
                   Skeleton,
                   Animation> element) {
                return std::visit(
                    [&](auto&& value) {
                        using T = std::decay_t<decltype(value)>;
                        return self.add(std::forward<T>(value));
                    },
                    element);
            },
            "element"_a,
            R"(Add an element to the scene.

:param element: The element to add to the scene. E.g. node, mesh, image, texture, material, light, camera, skeleton, or animation.

:returns: The id of the added element.)")
        .def(
            "add_child",
            &SceneType::add_child,
            "parent_id"_a,
            "child_id"_a,
            R"(Add a child node to a parent node.

:param parent_id: The parent node id.
:param child_id: The child node id.

:returns: The id of the added child node.)");

    m.def(
        "compute_global_node_transform",
        [](const SceneType& scene, size_t node_idx) {
            auto t = utils::compute_global_node_transform<Scalar, Index>(scene, node_idx);
            return nb::ndarray<nb::numpy, float, nb::f_contig, nb::shape<4, 4>>(
                t.data(),
                {4, 4},
                nb::handle(), // owner
                {1, 4});
        },
        nb::rv_policy::copy,
        "scene"_a,
        "node_idx"_a,
        R"(Compute the global transform associated with a node.

:param scene: The input node.
:param node_idx: The index of the taget node.

:returns: The global transform of the target node, which is the combination of transforms from this node all the way to the root.
    )");
}

} // namespace lagrange::python
