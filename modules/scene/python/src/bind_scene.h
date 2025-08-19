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
#include <lagrange/scene/internal/scene_string_utils.h>
#include <lagrange/scene/scene_utils.h>
#include <lagrange/utils/assert.h>

#include "bind_value.h"

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <Eigen/Core>
#include <nanobind/eigen/dense.h>
#include <nanobind/eval.h>
#include <nanobind/nanobind.h>
#include <nanobind/stl/array.h>
#include <nanobind/stl/bind_map.h>
#include <nanobind/stl/bind_vector.h>
#include <nanobind/stl/filesystem.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/variant.h>
#include <nanobind/trampoline.h>
#include <lagrange/utils/warnon.h>
// clang-format on


namespace lagrange::python {

namespace nb = nanobind;

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


    nb::class_<lagrange::scene::Extensions>(m, "Extensions")
        .def("__repr__", [](const lagrange::scene::Extensions& self) { return scene::internal::to_string(self); })
        .def_prop_ro("size", &Extensions::size)
        .def_prop_ro("empty", &Extensions::empty)
        .def_rw("data", &Extensions::data);

    nb::class_<SceneMeshInstance>(m, "SceneMeshInstance", "Pairs a mesh with its materials (zero, one, or more)")
        .def(nb::init<>())
        .def(
            "__repr__",
            [](const SceneMeshInstance& self) { return scene::internal::to_string(self); })
        .def_prop_rw(
            "mesh",
            [](SceneMeshInstance& self) -> std::optional<ElementId> {
                if (self.mesh != invalid_element)
                    return self.mesh;
                else
                    return {};
            },
            [](SceneMeshInstance& self, ElementId mesh) { self.mesh = mesh; },
            "Mesh index. Has to be a valid index in the scene.meshes vector (None if invalid)")
        .def_rw("materials", &SceneMeshInstance::materials, "Material indices in the scene.materials vector. This is typically a single material index. When a single mesh uses multiple materials, the AttributeName::material_id facet attribute should be defined.");

    nb::class_<Node>(m, "Node", "Represents a node in the scene hierarchy")
        .def(nb::init<>())
        .def("__repr__", [](const Node& self) { return scene::internal::to_string(self); })
        .def_rw("name", &Node::name, "Node name. May not be unique and can be empty")
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
            "Transform of the node, relative to its parent")
        .def_prop_rw(
            "parent",
            [](Node& node) -> std::optional<ElementId> {
                if (node.parent != invalid_element)
                    return node.parent;
                else
                    return {};
            },
            [](Node& node, ElementId parent) { node.parent = parent; },
            "Parent index. May be invalid if the node has no parent (e.g. the root)")
        .def_rw("children", &Node::children, "Children indices. May be empty")
        .def_rw("meshes", &Node::meshes, "List of meshes contained in this node")
        .def_rw("cameras", &Node::cameras, "List of cameras contained in this node")
        .def_rw("lights", &Node::lights, "List of lights contained in this node")
        .def_rw("extensions", &Node::extensions);

    nb::class_<ImageBufferExperimental> image_buffer(m, "ImageBuffer", "Minimalistic image data structure that stores the raw image data");
    image_buffer.def(nb::init<>())
        .def(
            "__repr__",
            [](const ImageBufferExperimental& self) { return scene::internal::to_string(self); })
        .def_ro("width", &ImageBufferExperimental::width, "Image width")
        .def_ro("height", &ImageBufferExperimental::height, "Image height")
        .def_ro(
            "num_channels",
            &ImageBufferExperimental::num_channels,
            "Number of image channels (must be 1, 3, or 4)")
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
            "Raw buffer of size (width * height * num_channels * num_bits_per_element / 8) bytes containing image data")
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
            "The scalar type of the elements in the buffer");

    nb::class_<ImageExperimental> image(m, "Image", "Image structure that can store either image data or reference to an image file");
    image.def(nb::init<>())
        .def(
            "__repr__",
            [](const ImageExperimental& self) { return scene::internal::to_string(self); })
        .def_rw("name", &ImageExperimental::name, "Image name. Not guaranteed to be unique and can be empty")
        .def_rw("image", &ImageExperimental::image, "Image data")
        .def_prop_rw(
            "uri",
            [](const ImageExperimental& self) -> std::optional<std::string> {
                if (self.uri.empty())
                    return {};
                else
                    return self.uri.string();
            },
            [](ImageExperimental& self, std::optional<std::string> uri) {
                if (uri.has_value())
                    self.uri = fs::path(uri.value());
                else
                    self.uri = fs::path();
            },
            "Image file path. This path is relative to the file that contains the scene. It is only valid if image data should be mapped to an external file")
        .def_rw(
            "extensions",
            &ImageExperimental::extensions,
            "Image extensions");

    nb::class_<TextureInfo>(m, "TextureInfo", "Pair of texture index (which texture to use) and texture coordinate index (which set of UVs to use)")
        .def(nb::init<>())
        .def("__repr__", [](const TextureInfo& self) { return scene::internal::to_string(self); })
        .def_prop_rw(
            "index",
            [](const TextureInfo& self) -> std::optional<ElementId> {
                if (self.index != invalid_element)
                    return self.index;
                else
                    return {};
            },
            [](TextureInfo& self, std::optional<ElementId> index) {
                if (index.has_value())
                    self.index = index.value();
                else
                    self.index = invalid_element;
            },
            "Texture index. Index in scene.textures vector. `None` if not set")
        .def_rw("texcoord", &TextureInfo::texcoord, "Index of UV coordinates. Usually stored in the mesh as `texcoord_x` attribute where x is this variable. This is typically 0");

    nb::class_<MaterialExperimental> material(m, "Material", "PBR material, based on the gltf specification. This is subject to change, to support more material models");
    material.def(nb::init<>())
        .def(
            "__repr__",
            [](const MaterialExperimental& self) { return scene::internal::to_string(self); })
        .def_rw("name", &MaterialExperimental::name, "Material name. May not be unique, and can be empty")
        .def_rw("base_color_value", &MaterialExperimental::base_color_value, "Base color value")
        .def_rw("base_color_texture", &MaterialExperimental::base_color_texture, "Base color texture")
        .def_rw("alpha_mode", &MaterialExperimental::alpha_mode, "The alpha mode specifies how to interpret the alpha value of the base color")
        .def_rw("alpha_cutoff", &MaterialExperimental::alpha_cutoff, "Alpha cutoff value")
        .def_rw("emissive_value", &MaterialExperimental::emissive_value, "Emissive color value")
        .def_rw("emissive_texture", &MaterialExperimental::emissive_texture, "Emissive texture")
        .def_rw("metallic_value", &MaterialExperimental::metallic_value, "Metallic value")
        .def_rw("roughness_value", &MaterialExperimental::roughness_value, "Roughness value")
        .def_rw("metallic_roughness_texture", &MaterialExperimental::metallic_roughness_texture, "Metalness and roughness are packed together in a single texture. Green channel has roughness, blue channel has metalness")
        .def_rw("normal_texture", &MaterialExperimental::normal_texture, "Normal texture")
        .def_rw("normal_scale", &MaterialExperimental::normal_scale, "Normal scaling factor. normal = normalize(<sampled tex value> * 2 - 1) * vec3(scale, scale, 1)")
        .def_rw("occlusion_texture", &MaterialExperimental::occlusion_texture, "Occlusion texture")
        .def_rw("occlusion_strength", &MaterialExperimental::occlusion_strength, "Occlusion strength. color = lerp(color, color * <sampled tex value>, strength)")
        .def_rw("double_sided", &MaterialExperimental::double_sided, "Whether the material is double-sided")
        .def_rw("extensions", &MaterialExperimental::extensions, "Material extensions");

    nb::enum_<MaterialExperimental::AlphaMode>(material, "AlphaMode", "Alpha mode")
        .value("Opaque", MaterialExperimental::AlphaMode::Opaque, "Alpha is ignored, and rendered output is opaque")
        .value("Mask", MaterialExperimental::AlphaMode::Mask, "Output is either opaque or transparent depending on the alpha value and the alpha_cutoff value")
        .value("Blend", MaterialExperimental::AlphaMode::Blend, "Alpha value is used to composite source and destination");


    nb::class_<Texture> texture(m, "Texture", "Texture");
    texture.def(nb::init<>())
        .def("__repr__", [](const Texture& self) { return scene::internal::to_string(self); })
        .def_rw("name", &Texture::name, "Texture name")
        .def_prop_rw(
            "image",
            [](Texture& self) -> std::optional<ElementId> {
                if (self.image != invalid_element)
                    return self.image;
                else
                    return {};
            },
            [](Texture& self, ElementId img) { self.image = img; },
            "Index of image in scene.images vector (None if invalid)")
        .def_rw("mag_filter", &Texture::mag_filter, "Texture magnification filter, used when texture appears larger on screen than the source image")
        .def_rw("min_filter", &Texture::min_filter, "Texture minification filter, used when the texture appears smaller on screen than the source image")
        .def_rw("wrap_u", &Texture::wrap_u, "Texture wrap mode for U coordinate")
        .def_rw("wrap_v", &Texture::wrap_v, "Texture wrap mode for V coordinate")
        .def_rw("scale", &Texture::scale, "Texture scale")
        .def_rw("offset", &Texture::offset, "Texture offset")
        .def_rw("rotation", &Texture::rotation, "Texture rotation")
        .def_rw("extensions", &Texture::extensions, "Texture extensions");

    nb::enum_<Texture::WrapMode>(texture, "WrapMode", "Texture wrap mode")
        .value("Wrap", Texture::WrapMode::Wrap, "u|v becomes u%1|v%1")
        .value("Clamp", Texture::WrapMode::Clamp, "Coordinates outside [0, 1] are clamped to the nearest value")
        .value("Decal", Texture::WrapMode::Decal, "If the texture coordinates for a pixel are outside [0, 1], the texture is not applied")
        .value("Mirror", Texture::WrapMode::Mirror, "Mirror wrap mode");
    nb::enum_<Texture::TextureFilter>(texture, "TextureFilter", "Texture filter mode")
        .value("Undefined", Texture::TextureFilter::Undefined, "Undefined filter")
        .value("Nearest", Texture::TextureFilter::Nearest, "Nearest neighbor filtering")
        .value("Linear", Texture::TextureFilter::Linear, "Linear filtering")
        .value("NearestMipmapNearest", Texture::TextureFilter::NearestMipmapNearest, "Nearest mipmap nearest filtering")
        .value("LinearMipmapNearest", Texture::TextureFilter::LinearMipmapNearest, "Linear mipmap nearest filtering")
        .value("NearestMipmapLinear", Texture::TextureFilter::NearestMipmapLinear, "Nearest mipmap linear filtering")
        .value("LinearMipmapLinear", Texture::TextureFilter::LinearMipmapLinear, "Linear mipmap linear filtering");

    nb::class_<Light> light(m, "Light", "Light");
    light.def(nb::init<>())
        .def("__repr__", [](const Light& self) { return scene::internal::to_string(self); })
        .def_rw("name", &Light::name, "Light name")
        .def_rw("type", &Light::type, "Light type")
        .def_rw("position", &Light::position, "Light position. Note that the light is part of the scene graph, and has an associated transform in its node. This value is relative to the coordinate system defined by the node")
        .def_rw("direction", &Light::direction, "Light direction")
        .def_rw("up", &Light::up, "Light up vector")
        .def_rw("intensity", &Light::intensity, "Light intensity")
        .def_rw("attenuation_constant", &Light::attenuation_constant, "Attenuation constant. Intensity of light at a given distance 'd' is: intensity / (attenuation_constant + attenuation_linear * d + attenuation_quadratic * d * d + attenuation_cubic * d * d * d)")
        .def_rw("attenuation_linear", &Light::attenuation_linear, "Linear attenuation factor")
        .def_rw("attenuation_quadratic", &Light::attenuation_quadratic, "Quadratic attenuation factor")
        .def_rw("attenuation_cubic", &Light::attenuation_cubic, "Cubic attenuation factor")
        .def_rw("range", &Light::range, "Range is defined for point and spot lights. It defines a distance cutoff at which the light intensity is to be considered zero. When the value is 0, range is assumed to be infinite")
        .def_rw("color_diffuse", &Light::color_diffuse, "Diffuse color")
        .def_rw("color_specular", &Light::color_specular, "Specular color")
        .def_rw("color_ambient", &Light::color_ambient, "Ambient color")
        .def_rw("angle_inner_cone", &Light::angle_inner_cone, "Inner angle of a spot light's light cone. 2PI for point lights, undefined for directional lights")
        .def_rw("angle_outer_cone", &Light::angle_outer_cone, "Outer angle of a spot light's light cone. 2PI for point lights, undefined for directional lights")
        .def_rw("size", &Light::size, "Size of area light source")
        .def_rw("extensions", &Light::extensions, "Light extensions");

    nb::enum_<Light::Type>(light, "Type", "Light type")
        .value("Undefined", Light::Type::Undefined, "Undefined light type")
        .value("Directional", Light::Type::Directional, "Directional light")
        .value("Point", Light::Type::Point, "Point light")
        .value("Spot", Light::Type::Spot, "Spot light")
        .value("Ambient", Light::Type::Ambient, "Ambient light")
        .value("Area", Light::Type::Area, "Area light");

    nb::class_<Camera> camera(m, "Camera", "Camera");
    camera.def(nb::init<>())
        .def("__repr__", [](const Camera& self) { return scene::internal::to_string(self); })
        .def_rw("name", &Camera::name, "Camera name")
        .def_rw("position", &Camera::position, "Camera position. Note that the camera is part of the scene graph, and has an associated transform in its node. This value is relative to the coordinate system defined by the node")
        .def_rw("up", &Camera::up, "Camera up vector")
        .def_rw("look_at", &Camera::look_at, "Camera look-at point")
        .def_rw("near_plane", &Camera::near_plane, "Distance of the near clipping plane. This value cannot be 0")
        .def_rw("far_plane", &Camera::far_plane, "Distance of the far clipping plane")
        .def_rw("type", &Camera::type, "Camera type")
        .def_rw("aspect_ratio", &Camera::aspect_ratio, "Screen aspect ratio. This is the value of width / height of the screen. aspect_ratio = tan(horizontal_fov / 2) / tan(vertical_fov / 2)")
        .def_rw("horizontal_fov", &Camera::horizontal_fov, "Horizontal field of view angle, in radians. This is the angle between the left and right borders of the viewport. It should not be greater than Pi. fov is only defined when the camera type is perspective, otherwise it should be 0")
        .def_rw("orthographic_width", &Camera::orthographic_width, "Half width of the orthographic view box. Or horizontal magnification. This is only defined when the camera type is orthographic, otherwise it should be 0")
        .def_prop_ro("get_vertical_fov", &Camera::get_vertical_fov, "Get the vertical field of view. Make sure aspect_ratio is set before calling this")
        .def_prop_ro(
            "set_horizontal_fov_from_vertical_fov",
            &Camera::set_horizontal_fov_from_vertical_fov,
            "vfov"_a,
            "Set horizontal fov from vertical fov. Make sure aspect_ratio is set before calling this")
        .def_rw("extensions", &Camera::extensions, "Camera extensions");

    nb::enum_<Camera::Type>(camera, "Type", "Camera type")
        .value("Perspective", Camera::Type::Perspective, "Perspective projection")
        .value("Orthographic", Camera::Type::Orthographic, "Orthographic projection");

    nb::class_<Animation>(m, "Animation", "Animation")
        .def(nb::init<>())
        .def("__repr__", [](const Animation& self) { return scene::internal::to_string(self); })
        .def_rw("name", &Animation::name, "Animation name")
        .def_rw("extensions", &Animation::extensions, "Animation extensions");


    nb::class_<Skeleton>(m, "Skeleton", "Skeleton")
        .def(nb::init<>())
        .def("__repr__", [](const Skeleton& self) { return scene::internal::to_string(self); })
        .def_rw("meshes", &Skeleton::meshes, "This skeleton is used to deform those meshes. This will typically contain one value, but can have zero or multiple meshes. The value is the index in the scene meshes")
        .def_rw("extensions", &Skeleton::extensions, "Skeleton extensions");


    nb::class_<SceneType>(m, "Scene", "A 3D scene")
        .def(nb::init<>())
        .def(
            "__repr__",
            [](const SceneType& self) { return scene::internal::to_string(self); })
        .def_rw("name", &SceneType::name, "Name of the scene")
        .def_rw("nodes", &SceneType::nodes, "Scene nodes. This is a list of nodes, the hierarchy information is contained by each node having a list of children as indices to this vector")
        .def_rw("root_nodes", &SceneType::root_nodes, "Root nodes. This is typically one. Must be at least one")
        .def_rw("meshes", &SceneType::meshes, "Scene meshes")
        .def_rw("images", &SceneType::images, "Images")
        .def_rw("textures", &SceneType::textures, "Textures. They can reference images")
        .def_rw("materials", &SceneType::materials, "Materials. They can reference textures")
        .def_rw("lights", &SceneType::lights, "Lights in the scene")
        .def_rw("cameras", &SceneType::cameras, "Cameras. The first camera (if any) is the default camera view")
        .def_rw("skeletons", &SceneType::skeletons, "Scene skeletons")
        .def_rw("animations", &SceneType::animations, "Animations (unused for now)")
        .def_rw("extensions", &SceneType::extensions, "Scene extensions")
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
            R"(Add a child node to a parent node. The parent-child relationship will be updated for both nodes.

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

:param scene: The input scene.
:param node_idx: The index of the target node.

:returns: The global transform of the target node, which is the combination of transforms from this node all the way to the root.
    )");
}

} // namespace lagrange::python
