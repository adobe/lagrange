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

#include <lagrange/SurfaceMesh.h>
#include <lagrange/image/ImageView.h>
#include <lagrange/utils/invalid.h>
#include <lagrange/scene/SceneExtension.h>

#include <Eigen/Geometry>

#include <string>
#include <vector>

namespace lagrange {
namespace scene {

using ElementId = size_t;
constexpr ElementId invalid_element = lagrange::invalid<ElementId>();

// Used in Node, it pairs a mesh with its materials (zero, one, or more).
struct SceneMeshInstance
{
    // Mesh index. Has to be a valid index in the scene.meshes vector.
    ElementId mesh = lagrange::invalid<ElementId>();

    // Material indices in the scene.materials vector.
    // When a single mesh uses multiple materials,
    // the AttributeName::material_id facet attribute should be defined.
    std::vector<ElementId> materials;
};

// Represents a node in the scene hierarchy.
struct Node
{
    // Note that the node name may not be unique, and can be empty.
    std::string name;

    // Transform of the node, relative to its parent.
    Eigen::Affine3f transform = Eigen::Affine3f::Identity();

    // parent index. May be invalid if the node has no parent (e.g. the root).
    ElementId parent = lagrange::invalid<ElementId>();

    // children indices. May be empty.
    std::vector<ElementId> children;

    // List of meshes contained in this node.
    // Node that some file formats only allow 1 mesh per node (gltf). In this case we treat
    // multiple meshes as one mesh with multiple primitives, and only one material per mesh is
    // allowed.
    std::vector<SceneMeshInstance> meshes;

    // List of cameras contained in this node.
    std::vector<ElementId> cameras;

    // List of lights contained in this node.
    std::vector<ElementId> lights;

    Extensions extensions;
};

//
// Based on the image module, which is due for a rework, so the image data will change in the
// future.
//
// Describes a single image. Note that it may not actually contain image data, and only
// have a reference to a file on disk. This will always happen when LoadOptions.save_images ==
// false. In this case, width/height/precision/channel/data will contain the default values.
//
struct ImageLegacy
{
    // Node that the name may not be unique, and can be empty
    std::string name;

    // Image width.
    size_t width = 0;

    // Image height.
    size_t height = 0;

    // Image precision. You can also use `ImageLegacy.get_element_size` to get the byte-size of the
    // elements.
    image::ImagePrecision precision = image::ImagePrecision::unknown;

    // Image channels. You can also use `ImageLegacy.get_channel_count` to get this as a number.
    image::ImageChannel channel = image::ImageChannel::unknown;

    // Image pixel data. Check ImageStorage for details. This will change in the future.
    std::shared_ptr<image::ImageStorage> data;

    // URI or IRI of the image. Optional, can be empty.
    // Relative paths are relative to the main file asset.
    // Note that you should never have to read from disk, as the data is above.
    // During export, if this is non-empty, and it is supported by the file format,
    // then the image will be saved as an external asset with this filename.
    std::string uri;

    // Image file type. Can be unknown.
    enum class Type { Jpeg, Png, Bmp, Gif, Unknown };
    Type type = Type::Unknown;

    // ==== Utility functions ====
    // Get image channel count as a number rather than an enum.
    int get_num_channels() const
    {
        if (channel == image::ImageChannel::unknown) return -1;
        return static_cast<int>(channel);
    }

    // Returns element byte size.
    int get_element_size() const
    {
        switch (precision) {
        case image::ImagePrecision::int8: return sizeof(char);
        case image::ImagePrecision::uint8: return sizeof(unsigned char);
        case image::ImagePrecision::int32: return sizeof(int);
        case image::ImagePrecision::uint32: return sizeof(unsigned int);
        case image::ImagePrecision::float16: return sizeof(short);
        case image::ImagePrecision::float32: return sizeof(float);
        case image::ImagePrecision::float64: return sizeof(double);
        default: return -1;
        }
    }

    Extensions extensions;
};

// Pair of texture index (which texture to use) and texture coordinate index (which set of UVs to
// use).
struct TextureInfo
{
    // Texture index. Index in scene.textures vector.
    ElementId index = lagrange::invalid<ElementId>();

    // Index of UV coordinates. Usually stored in the mesh as `texcoord_x` attribute where x is this
    // variable. This is typically 0.
    int texcoord = 0;
};

// PBR material, based on the gltf specification.
// This is subject to change, to support more material models.
struct MaterialExperimental
{
    // Note that material name may not be unique, and can be empty.
    std::string name;

    Eigen::Vector4f base_color_value = Eigen::Vector4f::Ones();
    TextureInfo base_color_texture;

    Eigen::Vector3f emissive_value = Eigen::Vector3f::Zero();
    TextureInfo emissive_texture;

    // metalness and roughness are packed together in a single texture.
    // green channel has roughness, blue channel has metalness.
    TextureInfo metallic_roughness_texture;
    float metallic_value = 1.f;
    float roughness_value = 1.f;

    // The alpha mode specifies how to interpret the alpha value of the base color.
    enum class AlphaMode {
        Opaque, // Alpha is ignored, and rendered output is opaque
        Mask, // Output is either opaque or transparent depending on the alpha value and the
              // alpha_cutoff value.
        Blend // Alpha value is used to composite source and destination.
    };
    AlphaMode alpha_mode = AlphaMode::Opaque;
    float alpha_cutoff = 0.5f;


    // normal = normalize(<sampled tex value> * 2 - 1) * vec3(scale, scale, 1)
    float normal_scale = 1.f;
    TextureInfo normal_texture;

    // color = lerp(color, color * <sampled tex value>, strength)
    float occlusion_strength = 1.f;
    TextureInfo occlusion_texture;

    bool double_sided = false;

    Extensions extensions;
};

struct Texture
{
    std::string name;
    ElementId image = invalid<ElementId>(); // index of image in scene.images vector

    enum TextureFilter : int {
        Undefined = 0,
        Nearest = 9728,
        Linear = 9729,
        NearestMimpapNearest = 9984,
        LinearMipmapNearest = 9985,
        NearestMipmapLinear = 9986,
        LinearMipmapLinear = 9987
    };
    // Texture magnification filter, used when texture appears larger on screen than the source
    // image. Allowed values are UNDEFINED, NEAREST, LINEAR
    TextureFilter mag_filter = TextureFilter::Undefined;

    // Texture minification filter, used when the texture appears smaller on screen than the source
    // image. Allowed values are: UNDEFINED, NEAREST, LINEAR, NEAREST_MIPMAP_NEAREST,
    // LINEAR_MIPMAP_NEAREST, NEAREST_MIPMAP_LINEAR, LINEAR_MIPMAP_LINEAR
    TextureFilter min_filter = TextureFilter::Undefined;

    enum class WrapMode {
        Wrap, // u|v becomes u%1|v%1
        Clamp, // coordinates outside [0, 1] are clamped to the nearest value
        Decal, // If the texture coordinates for a pixel are outside [0, 1], the texture is not
               // applied
        Mirror
    };
    WrapMode wrap_u = WrapMode::Wrap;
    WrapMode wrap_v = WrapMode::Wrap;

    Eigen::Vector2f scale = Eigen::Vector2f::Ones();
    Eigen::Vector2f offset = Eigen::Vector2f::Zero();
    float rotation = 0.0f;

    Extensions extensions;
};

struct Light
{
    std::string name;

    enum class Type { Undefined, Directional, Point, Spot, Ambient, Area };
    Type type = Type::Undefined;

    // note that the light is part of the scene graph, and has an associated transform in its node.
    // The values below (position, up, look_at) are relative to the coordinate system defined by the
    // node.
    Eigen::Vector3f position = Eigen::Vector3f::Zero();
    Eigen::Vector3f direction = Eigen::Vector3f(0, 1, 0);
    Eigen::Vector3f up = Eigen::Vector3f(0, 0, 1);

    // Attenuation factor. Intensity of light at a given distance 'd' is:
    // intensity / (attenuation_constant
    // + attenuation_linear * d
    // + attenuation_quadratic * d * d
    // + attenuation_cubic * d * d * d)
    float intensity = 1.f;
    float attenuation_constant = 1.f;
    float attenuation_linear = 0.f;
    float attenuation_quadratic = 0.f;
    float attenuation_cubic = 0.f;

    // Range is defined for point and spot lights. It defines a distance cutoff at which the
    // light intensity is to be considered zero, so the light does not affects objects beyond this
    // range. When the value is 0, range is assumed to be infinite.
    float range = 0.0;

    // Colors
    Eigen::Vector3f color_diffuse = Eigen::Vector3f::Zero();
    Eigen::Vector3f color_specular = Eigen::Vector3f::Zero();
    Eigen::Vector3f color_ambient = Eigen::Vector3f::Zero();

    // inner and outer angle of a spot light's light cone
    // they are both 2PI for point lights, and undefined for directional lights.
    float angle_inner_cone;
    float angle_outer_cone;

    // size of area light source
    Eigen::Vector2f size = Eigen::Vector2f::Zero();

    Extensions extensions;
};

struct Camera
{
    // note that the camera is part of the scene graph, and has an associated transform in its node.
    // The values below (position, up, look_at) are relative to the coordinate system defined by the
    // node.
    std::string name;
    Eigen::Vector3f position = Eigen::Vector3f::Zero();
    Eigen::Vector3f up = Eigen::Vector3f(0, 1, 0);
    Eigen::Vector3f look_at = Eigen::Vector3f(0, 0, 1);

    // Distance of the near clipping plane. This value cannot be 0.
    float near_plane = 0.1f;

    // Distance of the far clipping plane.
    float far_plane = 1000.f;

    enum class Type { Perspective, Orthographic };
    Type type = Type::Perspective;

    // Half width of the orthographic view box. Or horizontal magnification.
    //
    // This is only defined when the camera type is orthographic, otherwise it should be 0.
    float orthographic_width = 0.f;

    // Screen aspect ratio. This is the value of width / height of the screen.
    //
    // aspect_ratio = tan(horizontal_fov / 2) / tan(vertical_fov / 2)
    //
    // So we can compute any of those 3 variables from any 2. We store 2 (aspect_ratio and
    // horizontal_fov) and provide utilities below to compute any of them from the other 2.
    float aspect_ratio = 1.f;

    // horizontal field of view angle, in radians.
    //
    // This is the angle between the left and right borders of the viewport.
    // It should not be greater than Pi.
    //
    // fov is only defined when the camera type is perspective, otherwise it should be 0.
    float horizontal_fov = (float)M_PI_2;

    // convenience methods to get or set the vertical fov instead.
    // make sure aspect_ratio is set before calling those!
    float get_vertical_fov() const
    {
        return 2.f * std::atan(std::tan(horizontal_fov * 0.5f) / aspect_ratio);
    }
    void set_horizontal_fov_from_vertical_fov(float vfov)
    {
        horizontal_fov = 2.f * std::atan(std::tan(vfov * 0.5f) * aspect_ratio);
    }
    void set_aspect_ratio_from_fov(float vfov, float hfov)
    {
        aspect_ratio = std::tan(hfov * 0.5f) / std::tan(vfov * 0.5f);
    }

    Extensions extensions;
};

struct Animation
{
    std::string name;
    // TODO

    Extensions extensions;
};

struct Skeleton
{
    // This skeleton is used to deform those meshes.
    // This will typically contain one value, but can have zero or multiple meshes.
    // The value is the index in the scene meshes.
    std::vector<ElementId> meshes;
    // TODO

    Extensions extensions;
};

template <typename Scalar, typename Index>
struct Scene
{
    using MeshType = SurfaceMesh<Scalar, Index>;

    // Name of the scene
    std::string name;

    // Scene nodes. This is a list of nodes, the hierarchy information is contained by each
    // node having a list of children as indices to this vector.
    std::vector<Node> nodes;

    // Root nodes. This is typically one. Must be at least one.
    std::vector<ElementId> root_nodes;

    // Scene meshes.
    std::vector<MeshType> meshes;

    // Images.
    std::vector<ImageLegacy> images;

    // Textures. They can reference images;
    std::vector<Texture> textures;

    // Materials. They can reference textures.
    std::vector<MaterialExperimental> materials;

    // Lights in the scene.
    std::vector<Light> lights;

    // Cameras. The first camera (if any) is the default camera view.
    std::vector<Camera> cameras;

    // Scene skeletons.
    std::vector<Skeleton> skeletons;

    // Unused for now.
    std::vector<Animation> animations;

    // Extensions.
    Extensions extensions;
};

using Scene32f = Scene<float, uint32_t>;
using Scene32d = Scene<double, uint32_t>;
using Scene64f = Scene<float, uint64_t>;
using Scene64d = Scene<double, uint64_t>;

} // namespace scene
} // namespace lagrange
