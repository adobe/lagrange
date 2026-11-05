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
#include <lagrange/common.h>
#include <lagrange/fs/filesystem.h>
#include <lagrange/internal/SafeVector.h>
#include <lagrange/internal/constants.h>
#include <lagrange/scene/SceneExtension.h>
#include <lagrange/scene/api.h>
#include <lagrange/utils/invalid.h>

#include <Eigen/Geometry>

#include <optional>
#include <string>
#include <vector>

namespace lagrange {

///
/// Enum describing at runtime the value type of an attribute. This can be accessed from the base
/// attribute class and enables safe downcasting without global RTTI.
///
enum class AttributeValueType : uint8_t;

} // namespace lagrange

namespace lagrange::scene {

///
/// @addtogroup module-scene
/// @{
///

/// Zero-based index into a scene element vector. Each element type has its own index space.
using ElementId = size_t;
/// Sentinel for an absent scene element reference.
constexpr ElementId invalid_element = lagrange::invalid<ElementId>();

/// A mesh reference and its instance-specific material bindings, attached to a Node.
struct LA_SCENE_API SceneMeshInstance
{
    /// Index into Scene::meshes, or invalid_element when unset. The instance does not own the mesh.
    ElementId mesh = invalid_element;

    /// Global indices into Scene::materials. A mesh's one-channel `material_id` facet attribute
    /// stores local indices into this list, not global scene material indices. Without that
    /// attribute, scene-to-mesh conversion assigns the first material to every facet; an empty
    /// list means no material.
    SafeVector<ElementId> materials;
};

/// A scene-graph node. Element references are zero-based indices into the owning Scene's vectors.
struct LA_SCENE_API Node
{
    /// Node name; may be empty or non-unique.
    std::string name;

    /// Maps node-local coordinates to parent coordinates (or world coordinates for a root).
    Eigen::Affine3f transform = Eigen::Affine3f::Identity();

    /// Index into Scene::nodes, or invalid_element when the node has no parent.
    ElementId parent = invalid_element;

    /// Child indices into Scene::nodes; keep consistent with each child's parent field.
    SafeVector<ElementId> children;

    /// Mesh instances attached to this node, all using this node's transform. Format-specific
    /// restrictions (such as one material per glTF primitive) are handled during I/O.
    SafeVector<SceneMeshInstance> meshes;

    /// Camera indices into Scene::cameras.
    SafeVector<ElementId> cameras;

    /// Light indices into Scene::lights.
    SafeVector<ElementId> lights;

    /// Application- or format-specific node data.
    Extensions extensions;
};

///
/// Minimal image buffer storing interleaved channel values in row-major (y, x, channel) order.
///
struct LA_SCENE_API ImageBufferExperimental
{
    /// Image width.
    size_t width = 0;

    /// Image height.
    size_t height = 0;

    /// Number of image channels (must be 1, 3, or 4).
    size_t num_channels = 4;

    /// Scalar type of each channel value; must be set before interpreting data.
    AttributeValueType element_type;

    /// Raw image data, with width * height * num_channels channel values of element_type.
    std::vector<unsigned char> data;

    ///
    /// Gets the scalar element size in bits, or zero for an unsupported element_type.
    /// @warning Floating-point cases currently return sizeof(float) or sizeof(double) in bytes,
    ///          unlike the bit counts returned for integer types.
    ///
    size_t get_bits_per_element() const;
};

///
/// Image data and/or a reference to an external image file.
///
struct LA_SCENE_API ImageExperimental
{
    /// Image name. Not guaranteed to be unique and can be empty.
    std::string name;

    /// Image data.
    ImageBufferExperimental image;

    /// External image file path, optionally retained alongside loaded image data. Relative paths
    /// are relative to the file containing the scene.
    fs::path uri;

    /// Image extensions.
    Extensions extensions;
};


/// Texture reference with the index of the mesh UV set to sample.
struct LA_SCENE_API TextureInfo
{
    /// Index into Scene::textures, or invalid_element when no texture is assigned.
    ElementId index = invalid_element;

    /// Zero-based UV set index, typically zero. Importers commonly name sets `texcoord_0`,
    /// `texcoord_1`, etc.; the attribute must have AttributeUsage::UV.
    int texcoord = 0;
};

/// Experimental metallic-roughness PBR material based on glTF; the material model may change.
struct LA_SCENE_API MaterialExperimental
{
    /// Material name; may be empty or non-unique.
    std::string name;

    /// RGBA base-color factor, multiplied by base_color_texture when present.
    Eigen::Vector4f base_color_value = Eigen::Vector4f::Ones();
    /// Texture supplying base color and opacity.
    TextureInfo base_color_texture;

    /// RGB emissive factor, multiplied by emissive_texture when present.
    Eigen::Vector3f emissive_value = Eigen::Vector3f::Zero();
    /// Texture supplying emissive color.
    TextureInfo emissive_texture;

    /// Packed texture: green stores roughness, blue stores metalness.
    TextureInfo metallic_roughness_texture;
    /// Metalness factor, multiplied by the packed texture's blue channel when present.
    float metallic_value = 1.f;
    /// Roughness factor, multiplied by the packed texture's green channel when present.
    float roughness_value = 1.f;

    /// Interpretation of the base-color alpha component.
    enum class AlphaMode {
        Opaque, ///< Ignore alpha and render as opaque.
        Mask, ///< Render as opaque or transparent according to alpha_cutoff.
        Blend ///< Use alpha to composite source and destination.
    };
    AlphaMode alpha_mode = AlphaMode::Opaque;
    /// Alpha threshold used by AlphaMode::Mask.
    float alpha_cutoff = 0.5f;


    /// Scale of the sampled tangent-space normal's X and Y components.
    float normal_scale = 1.f;
    /// Tangent-space normal map.
    TextureInfo normal_texture;

    /// Occlusion strength: result = lerp(color, color * sampled red channel, strength).
    float occlusion_strength = 1.f;
    /// Texture supplying occlusion in its red channel.
    TextureInfo occlusion_texture;

    /// Whether both sides of each surface should be rendered.
    bool double_sided = false;

    Extensions extensions;
};

/// Image reference, sampling parameters, and texture-coordinate transform.
struct LA_SCENE_API Texture
{
    /// Texture name; may be empty or non-unique.
    std::string name;
    /// Index into Scene::images, or invalid_element if absent.
    ElementId image = invalid_element;

    /// Sampling filter; values match the glTF sampler filter enums.
    enum class TextureFilter : int {
        Undefined = 0, ///< No filter specified.
        Nearest = 9728, ///< Nearest-neighbor sampling.
        Linear = 9729, ///< Linear interpolation between adjacent texels.
        NearestMipmapNearest = 9984, ///< Nearest texel from the closest mipmap level.
        LinearMipmapNearest = 9985, ///< Linear texel interpolation in the closest mipmap level.
        NearestMipmapLinear = 9986, ///< Nearest texel blended between the two closest mipmaps.
        LinearMipmapLinear = 9987 ///< Trilinear interpolation between texels and mipmap levels.
    };
    /// Magnification filter; allowed values are Undefined, Nearest, and Linear.
    TextureFilter mag_filter = TextureFilter::Undefined;

    /// Minification filter; accepts any TextureFilter value.
    TextureFilter min_filter = TextureFilter::Undefined;

    /// Behavior outside the unit texture-coordinate interval.
    enum class WrapMode {
        Wrap, ///< Repeat periodically outside [0, 1].
        Clamp, ///< Clamp coordinates to [0, 1].
        Decal, ///< Do not apply the texture outside [0, 1].
        Mirror ///< Repeat with alternating mirrored intervals.
    };
    WrapMode wrap_u = WrapMode::Wrap;
    WrapMode wrap_v = WrapMode::Wrap;

    /// Texture-coordinate scale.
    Eigen::Vector2f scale = Eigen::Vector2f::Ones();
    /// Texture-coordinate offset.
    Eigen::Vector2f offset = Eigen::Vector2f::Zero();
    /// Texture-coordinate rotation.
    float rotation = 0.0f;

    Extensions extensions;
};

/// Light parameters in the coordinate system of each node referencing this light.
struct LA_SCENE_API Light
{
    /// Light name; may be empty or non-unique.
    std::string name;

    /// Light type; determines which type-specific parameters are used.
    enum class Type { Undefined, Directional, Point, Spot, Ambient, Area };
    Type type = Type::Undefined;

    /// Position in node-local coordinates.
    Eigen::Vector3f position = Eigen::Vector3f::Zero();
    /// Emission direction in node-local coordinates.
    Eigen::Vector3f direction = Eigen::Vector3f(0, 1, 0);
    /// Up vector in node-local coordinates.
    Eigen::Vector3f up = Eigen::Vector3f(0, 0, 1);

    /// Light intensity; divided by attenuation_constant + attenuation_linear * d +
    /// attenuation_quadratic * d^2 + attenuation_cubic * d^3 at distance d.
    float intensity = 1.f;
    /// Constant attenuation coefficient.
    float attenuation_constant = 1.f;
    /// Linear attenuation coefficient, multiplied by the distance d.
    float attenuation_linear = 0.f;
    /// Quadratic attenuation coefficient, multiplied by d^2.
    float attenuation_quadratic = 0.f;
    /// Cubic attenuation coefficient, multiplied by d^3.
    float attenuation_cubic = 0.f;

    /// Distance cutoff for point and spot lights; zero means infinite range.
    float range = 0.0;

    /// Diffuse RGB light color.
    Eigen::Vector3f color_diffuse = Eigen::Vector3f::Zero();
    /// Specular RGB light color.
    Eigen::Vector3f color_specular = Eigen::Vector3f::Zero();
    /// Ambient RGB light color.
    Eigen::Vector3f color_ambient = Eigen::Vector3f::Zero();

    /// Inner spotlight cone angle in radians, measured from the emission axis.
    /// Not used for other light types.
    std::optional<float> angle_inner_cone;
    /// Outer spotlight cone angle in radians, measured from the emission axis.
    /// Not used for other light types.
    std::optional<float> angle_outer_cone;

    /// Area-light source size.
    Eigen::Vector2f size = Eigen::Vector2f::Zero();

    /// Application- or format-specific light data.
    Extensions extensions;
};

/// Camera parameters in the coordinate system of each node referencing this camera.
struct LA_SCENE_API Camera
{
    /// Camera name; may be empty or non-unique.
    std::string name;
    /// Camera position in node-local coordinates.
    Eigen::Vector3f position = Eigen::Vector3f::Zero();
    /// Up vector in node-local coordinates.
    Eigen::Vector3f up = Eigen::Vector3f(0, 1, 0);
    /// Look-at target point in node-local coordinates, not a direction vector.
    Eigen::Vector3f look_at = Eigen::Vector3f(0, 0, -1);

    /// Near clipping-plane distance; must be positive for projection.
    float near_plane = 0.1f;

    /// Far clipping-plane distance. Required for orthographic projection; std::nullopt gives an
    /// infinite far plane for perspective projection. A finite value must exceed near_plane.
    std::optional<float> far_plane = 1000.f;

    /// Projection type.
    enum class Type { Perspective, Orthographic };
    Type type = Type::Perspective;

    /// Half-width of the orthographic view box (horizontal magnification); positive for
    /// orthographic projection and unused for perspective projection.
    float orthographic_width = 0.f;

    /// Viewport width / height; must be positive. For perspective cameras this equals
    /// tan(horizontal_fov / 2) / tan(vertical_fov / 2).
    float aspect_ratio = 1.f;

    /// Perspective horizontal field of view in radians, between the left and right viewport
    /// borders. Must be in (0, pi) for projection; unused for orthographic projection.
    float horizontal_fov = (float)lagrange::internal::pi_2;

    /// Gets the vertical field of view in radians using the current aspect_ratio.
    float get_vertical_fov() const
    {
        return 2.f * std::atan(std::tan(horizontal_fov * 0.5f) / aspect_ratio);
    }
    /// Sets horizontal_fov from a vertical field of view in radians and the current aspect_ratio.
    void set_horizontal_fov_from_vertical_fov(float vfov)
    {
        horizontal_fov = 2.f * std::atan(std::tan(vfov * 0.5f) * aspect_ratio);
    }
    /// Sets aspect_ratio from vertical and horizontal fields of view in radians.
    void set_aspect_ratio_from_fov(float vfov, float hfov)
    {
        aspect_ratio = std::tan(hfov * 0.5f) / std::tan(vfov * 0.5f);
    }

    /// Application- or format-specific camera data.
    Extensions extensions;
};

/// Animation metadata; animation channels are not represented by this type.
struct LA_SCENE_API Animation
{
    /// Animation name; may be empty or non-unique.
    std::string name;
    // TODO

    /// Application- or format-specific animation data.
    Extensions extensions;
};

/// Skeleton metadata; joint and skinning data are not represented by this type.
struct LA_SCENE_API Skeleton
{
    /// Indices into Scene::meshes for meshes deformed by this skeleton; may be empty.
    SafeVector<ElementId> meshes;
    // TODO

    /// Application- or format-specific skeleton data.
    Extensions extensions;
};

/// Scene container owning meshes, graph nodes, and rendering metadata. References between
/// elements are indices into the corresponding vectors and must remain valid when vectors change.
/// The node hierarchy must be acyclic, with matching parent/children links and root_nodes.
/// Node transforms use single precision independently of the mesh scalar type.
///
/// @tparam Scalar  Mesh scalar type.
/// @tparam Index   Mesh index type, distinct from the scene's ElementId index type.
template <typename Scalar, typename Index>
struct Scene
{
    using MeshType = SurfaceMesh<Scalar, Index>;

    /// Scene name.
    std::string name;

    /// Nodes indexed by Node::parent, Node::children, and root_nodes.
    SafeVector<Node> nodes;

    /// Root-node indices into nodes. May be empty for an empty scene.
    SafeVector<ElementId> root_nodes;

    /// Source meshes, including meshes that may have no instances in the node graph.
    SafeVector<MeshType> meshes;

    /// Images referenced by textures.
    SafeVector<ImageExperimental> images;

    /// Textures, which may reference images.
    SafeVector<Texture> textures;

    /// Materials, which may reference textures.
    SafeVector<MaterialExperimental> materials;

    /// Lights referenced by nodes.
    SafeVector<Light> lights;

    /// Cameras referenced by nodes. The first camera, if any, is the default view.
    SafeVector<Camera> cameras;

    /// Skeleton metadata.
    SafeVector<Skeleton> skeletons;

    /// Animation metadata; currently unused.
    SafeVector<Animation> animations;

    /// Application- or format-specific scene data.
    Extensions extensions;

public:
    ///
    /// Appends an element by copying an lvalue or moving an rvalue. Adding a node does not
    /// attach it to the hierarchy or update root_nodes; adding a mesh does not instantiate it.
    ///
    /// @param value The element to add. It can be node, mesh, image, texture, material,
    ///              light, camera, skeleton or animation.
    ///
    /// @return Index of the new element in its corresponding scene vector.
    ///
    template <typename T>
    ElementId add(T&& value);

    ///
    /// Appends child_id to the parent's children and sets the child's parent. Both nodes must
    /// already exist. Does not remove a previous parent link, deduplicate children, update
    /// root_nodes, or detect cycles; callers must preserve a valid hierarchy.
    ///
    /// @param parent_id  Valid index into nodes of the parent.
    /// @param child_id   Valid index into nodes of the child.
    ///
    void add_child(ElementId parent_id, ElementId child_id);
};

/// @}

template <typename Scalar, typename Index>
template <typename T>
ElementId Scene<Scalar, Index>::add(T&& value)
{
    using ElementType = std::decay_t<T>;
    if constexpr (std::is_same_v<ElementType, Node>) {
        nodes.emplace_back(std::forward<T>(value));
        return nodes.size() - 1;
    } else if constexpr (std::is_same_v<ElementType, MeshType>) {
        meshes.emplace_back(std::forward<T>(value));
        return meshes.size() - 1;
    } else if constexpr (std::is_same_v<ElementType, ImageExperimental>) {
        images.emplace_back(std::forward<T>(value));
        return images.size() - 1;
    } else if constexpr (std::is_same_v<ElementType, Texture>) {
        textures.emplace_back(std::forward<T>(value));
        return textures.size() - 1;
    } else if constexpr (std::is_same_v<ElementType, MaterialExperimental>) {
        materials.emplace_back(std::forward<T>(value));
        return materials.size() - 1;
    } else if constexpr (std::is_same_v<ElementType, Light>) {
        lights.emplace_back(std::forward<T>(value));
        return lights.size() - 1;
    } else if constexpr (std::is_same_v<ElementType, Camera>) {
        cameras.emplace_back(std::forward<T>(value));
        return cameras.size() - 1;
    } else if constexpr (std::is_same_v<ElementType, Skeleton>) {
        skeletons.emplace_back(std::forward<T>(value));
        return skeletons.size() - 1;
    } else if constexpr (std::is_same_v<ElementType, Animation>) {
        animations.emplace_back(std::forward<T>(value));
        return animations.size() - 1;
    } else {
        static_assert(StaticAssertableBool<T>::False, "Unsupported type");
    }
}

///
/// @addtogroup module-scene
/// @{
///

using Scene32f = Scene<float, uint32_t>;
using Scene32d = Scene<double, uint32_t>;
using Scene64f = Scene<float, uint64_t>;
using Scene64d = Scene<double, uint64_t>;

/// @}

} // namespace lagrange::scene
