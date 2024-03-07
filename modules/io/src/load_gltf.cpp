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

// this .cpp provides implementations for functions defined in those headers:
#include <lagrange/io/load_mesh_gltf.h>
#include <lagrange/io/load_scene_gltf.h>
#include <lagrange/io/load_simple_scene_gltf.h>
// ====

#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/combine_meshes.h>
#include <lagrange/internal/skinning.h>
#include <lagrange/scene/SceneTypes.h>
#include <lagrange/scene/SimpleSceneTypes.h>
#include <lagrange/scene/scene_utils.h>
#include <lagrange/scene/simple_scene_convert.h>
#include <lagrange/utils/Error.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/safe_cast.h>
#include <lagrange/utils/strings.h>

#include <tiny_gltf.h>
#include <Eigen/Geometry>

#include <istream>
#include <optional>

namespace lagrange::io {
namespace {

scene::Value convert_value(const tinygltf::Value& value)
{
    switch (value.Type()) {
    case tinygltf::Type::BOOL_TYPE: return scene::Value(value.Get<bool>());
    case tinygltf::Type::INT_TYPE: return scene::Value(value.Get<int>());
    case tinygltf::Type::REAL_TYPE: return scene::Value(value.Get<double>());
    case tinygltf::Type::STRING_TYPE: return scene::Value(value.Get<std::string>());
    case tinygltf::Type::BINARY_TYPE: return scene::Value(value.Get<std::vector<unsigned char>>());
    case tinygltf::Type::ARRAY_TYPE: {
        scene::Value::Array arr;
        for (int i = 0; i < static_cast<int>(value.Size()); ++i) {
            arr.push_back(convert_value(value.Get(i)));
        }
        return scene::Value(std::move(arr));
    }
    case tinygltf::Type::OBJECT_TYPE: {
        scene::Value::Object obj;
        for (const auto& s : value.Keys()) {
            obj.insert({s, convert_value(value.Get(s))});
        }
        return scene::Value(std::move(obj));
    }
    case tinygltf::Type::NULL_TYPE: 
        [[fallthrough]];
    default: return scene::Value();
    }
}

scene::Extensions convert_extension_map(
    const tinygltf::ExtensionMap& extension_map,
    const LoadOptions& options)
{
    scene::Extensions extensions;
    for (const auto& [key, value] : extension_map) {
        scene::Value lvalue = convert_value(value);

        bool found_extension = false;
        for (const auto* extension : options.extension_converters) {
            if (extension->can_read(key)) {
                extensions.user_data.insert({key, extension->read(lvalue)});
                found_extension = true;
                break;
            }
        }

        if (!found_extension) {
            extensions.data.insert({key, lvalue});
        }
    }
    return extensions;
}

Eigen::Vector3f to_vec3(std::vector<double> v)
{
    return Eigen::Vector3f(v[0], v[1], v[2]);
}
Eigen::Vector4f to_vec4(std::vector<double> v)
{
    return Eigen::Vector4f(v[0], v[1], v[2], v[3]);
}

size_t get_num_channels(int type)
{
    switch (type) {
    case TINYGLTF_TYPE_SCALAR: return 1;
    case TINYGLTF_TYPE_VEC2: return 2;
    case TINYGLTF_TYPE_VEC3: return 3;
    case TINYGLTF_TYPE_VEC4: return 4;
    case TINYGLTF_TYPE_MAT2: return 4;
    case TINYGLTF_TYPE_MAT3: return 9;
    case TINYGLTF_TYPE_MAT4: return 16;
    }
    return invalid<size_t>();
}

template <typename Orig_t, typename Target_t>
std::vector<Target_t> load_buffer_data_internal(
    const tinygltf::Model& model,
    const tinygltf::Accessor& accessor)
{
    const tinygltf::BufferView& buffer_view = model.bufferViews[accessor.bufferView];
    const tinygltf::Buffer& buffer = model.buffers[buffer_view.buffer];

    size_t size = get_num_channels(accessor.type);

    std::vector<Orig_t> data(accessor.count * size);

    const size_t start = accessor.byteOffset + buffer_view.byteOffset;
    if (buffer_view.byteStride) {
        for (size_t i = 0; i < accessor.count; ++i) {
            size_t buf_idx = start + buffer_view.byteStride * i;
            std::memcpy(&data.at(i * size), &buffer.data.at(buf_idx), size * sizeof(Orig_t));
        }
    } else {
        std::memcpy(&data.at(0), &buffer.data.at(start), accessor.count * size * sizeof(Orig_t));
    }

    std::vector<Target_t> ret;
    std::transform(data.begin(), data.end(), std::back_inserter(ret), [&](Orig_t x) {
        if (accessor.normalized) {
            // If needed, convert normalized values into float or double. Details here:
            // https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#animations
            if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_BYTE) {
                return Target_t(std::max(x / 127.0, -1.0));
            } else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                return Target_t(x / 255.0);
            } else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_SHORT) {
                return Target_t(std::max(x / 32767.0, -1.0));
            } else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                return Target_t(x / 65535.0);
            } else {
                la_runtime_assert("Invalid normalized/componentType pair!");
                return Target_t(0); // happy compiler
            }
        } else {
            return Target_t(x);
        }
    });
    return ret;
}

///
/// Load accessor buffer data as a span of ValueType without any copy.
///
/// @tparam ValueType      The value type of the span.
/// @param model           The glTF model object.
/// @param accessor        The accessor object.
/// @param working_buffer  A working buffer to store the data if necessary.
///
/// @return A span of ValueType that points to the buffer data stored in the accessor.
///
template <typename ValueType>
span<const ValueType> load_buffer(
    const tinygltf::Model& model,
    const tinygltf::Accessor& accessor,
    std::vector<ValueType>& working_buffer)
{
    const tinygltf::BufferView& buffer_view = model.bufferViews[accessor.bufferView];
    const tinygltf::Buffer& buffer = model.buffers[buffer_view.buffer];

    size_t num_channels = get_num_channels(accessor.type);
    if (num_channels == invalid<size_t>())
        throw Error(fmt::format("Unsupported accessor type {}", accessor.type));

    const size_t start = accessor.byteOffset + buffer_view.byteOffset;

    if (buffer_view.byteStride != 0) {
        working_buffer.resize(accessor.count * num_channels);
        for (size_t i = 0; i < accessor.count; ++i) {
            size_t buf_idx = start + buffer_view.byteStride * i;
            std::copy_n(
                reinterpret_cast<const ValueType*>(buffer.data.data() + buf_idx),
                num_channels,
                working_buffer.data() + i * num_channels);
        }
        return {working_buffer.data(), working_buffer.size()};
    } else {
        return {
            reinterpret_cast<const ValueType*>(buffer.data.data() + start),
            accessor.count * num_channels};
    }
}

template <typename T>
std::vector<T> load_buffer_data_as(const tinygltf::Model& model, const tinygltf::Accessor& accessor)
{
    // clang-format off
    switch (accessor.componentType) {
    case TINYGLTF_COMPONENT_TYPE_BYTE:
        return load_buffer_data_internal<char, T>(model, accessor);
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
        return load_buffer_data_internal<unsigned char, T>(model, accessor);
    case TINYGLTF_COMPONENT_TYPE_SHORT:
        return load_buffer_data_internal<short, T>(model, accessor);
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
        return load_buffer_data_internal<unsigned short, T>(model, accessor);
    case TINYGLTF_COMPONENT_TYPE_INT:
        return load_buffer_data_internal<int, T>(model, accessor);
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
        return load_buffer_data_internal<unsigned int, T>(model, accessor);
    case TINYGLTF_COMPONENT_TYPE_FLOAT:
        return load_buffer_data_internal<float, T>(model, accessor);
    case TINYGLTF_COMPONENT_TYPE_DOUBLE:
        return load_buffer_data_internal<double, T>(model, accessor);
    default:
        throw std::runtime_error("Unexpected component type");
    }
    // clang-format on
    return std::vector<T>(); // make compiler happy
}

// =====================================

tinygltf::Model load_tinygltf(std::istream& input_stream)
{
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    loader.SetStoreOriginalJSONForExtrasAndExtensions(true);
    std::string err;
    std::string warn;
    bool ret;

    std::istreambuf_iterator<char> data_itr(input_stream), end_of_stream;
    std::string data(data_itr, end_of_stream);

    if (data.substr(0, 4) == "glTF") {
        // Binary
        ret = loader.LoadBinaryFromMemory(
            &model,
            &err,
            &warn,
            reinterpret_cast<unsigned char*>(data.data()),
            static_cast<unsigned int>(data.size()));
    } else {
        // ASCII
        std::string base_dir;
        ret = loader.LoadASCIIFromString(
            &model,
            &err,
            &warn,
            data.data(),
            static_cast<unsigned int>(data.size()),
            base_dir);
    }

    if (!warn.empty()) {
        for (const auto& line : string_split(warn, '\n')) {
            logger().warn("{}", line);
        }
    }
    if (!ret || !err.empty()) {
        throw std::runtime_error(err);
    }

    return model;
}

tinygltf::Model load_tinygltf(const fs::path& filename)
{
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    loader.SetStoreOriginalJSONForExtrasAndExtensions(true);
    std::string err;
    std::string warn;
    bool ret;
    if (to_lower(filename.extension().string()) == ".gltf") {
        ret = loader.LoadASCIIFromFile(&model, &err, &warn, filename.string());
    } else {
        la_runtime_assert(to_lower(filename.extension().string()) == ".glb");
        ret = loader.LoadBinaryFromFile(&model, &err, &warn, filename.string());
    }

    if (!warn.empty()) {
        for (const auto& line : string_split(warn, '\n')) {
            logger().warn("{}", line);
        }
    }
    if (!ret || !err.empty()) {
        throw std::runtime_error(err);
    }

    return model;
}

///
/// Convert a tinygltf accessor to a lagrange mesh attribute.
///
/// @tparam ValueType   The desired attribute value type.
/// @tparam Scalar      The mesh scalar type.
/// @tparam Index       The mesh index type.
///
/// @param model        The gltf model object.
/// @param accessor     The gltf accessor object.
/// @param name         The attribute name.
/// @param target_usage The target attribute usage if any.
/// @param mesh         The lagrange mesh.
///
template <typename ValueType, typename Scalar, typename Index>
void accessor_to_attribute_internal(
    const tinygltf::Model& model,
    const tinygltf::Accessor& accessor,
    std::string_view name,
    std::optional<AttributeUsage> target_usage,
    SurfaceMesh<Scalar, Index>& mesh)
{
    std::vector<ValueType> working_buffer;
    span<const ValueType> data = load_buffer<ValueType>(model, accessor, working_buffer);
    Index num_channels = static_cast<Index>(data.size() / accessor.count);
    AttributeElement element = AttributeElement::Vertex;
    AttributeUsage usage = AttributeUsage::Scalar;

    if (accessor.count == mesh.get_num_vertices()) {
        element = AttributeElement::Vertex;
    } else if (accessor.count == mesh.get_num_facets()) {
        element = AttributeElement::Facet;
    } else {
        logger().error("Unknown mesh property {}!", name);
        return;
    }

    if (target_usage.has_value()) {
        usage = target_usage.value();
    } else {
        switch (accessor.type) {
        case TINYGLTF_TYPE_SCALAR: usage = AttributeUsage::Scalar; break;
        case TINYGLTF_TYPE_VEC2: [[fallthrough]];
        case TINYGLTF_TYPE_VEC3: [[fallthrough]];
        case TINYGLTF_TYPE_VEC4: [[fallthrough]];
        case TINYGLTF_TYPE_VECTOR: usage = AttributeUsage::Vector; break;
        case TINYGLTF_TYPE_MAT2: [[fallthrough]];
        case TINYGLTF_TYPE_MAT3: [[fallthrough]];
        case TINYGLTF_TYPE_MAT4: [[fallthrough]];
        case TINYGLTF_TYPE_MATRIX:
            // Matrices are flattened as vectors for now.
            usage = AttributeUsage::Vector;
            break;
        default: logger().error("Unknown mesh property {}!", name); return;
        }
    }

    mesh.template create_attribute<ValueType>(name, element, usage, num_channels, data);
}

///
/// Convert a tinygltf accessor to a lagrange mesh attribute.
///
/// The attribute value type will be deduced from the accessor component type.
/// If target usage is not specified, it will be deduced from the accessor type.
///
/// @tparam Scalar      The mesh scalar type.
/// @tparam Index       The mesh index type.
///
/// @param model        The gltf model object.
/// @param accessor     The gltf accessor object.
/// @param name         The attribute name.
/// @param target_usage The target attribute usage if any.
/// @param mesh         The lagrange mesh.
///
template <typename Scalar, typename Index>
void accessor_to_attribute(
    const tinygltf::Model& model,
    const tinygltf::Accessor& accessor,
    std::string_view name,
    std::optional<AttributeUsage> target_usage,
    SurfaceMesh<Scalar, Index>& mesh)
{
    switch (accessor.componentType) {
    case TINYGLTF_COMPONENT_TYPE_BYTE:
        accessor_to_attribute_internal<int8_t>(model, accessor, name, target_usage, mesh);
        break;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
        accessor_to_attribute_internal<uint8_t>(model, accessor, name, target_usage, mesh);
        break;
    case TINYGLTF_COMPONENT_TYPE_SHORT:
        accessor_to_attribute_internal<int16_t>(model, accessor, name, target_usage, mesh);
        break;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
        accessor_to_attribute_internal<uint16_t>(model, accessor, name, target_usage, mesh);
        break;
    case TINYGLTF_COMPONENT_TYPE_INT:
        accessor_to_attribute_internal<int32_t>(model, accessor, name, target_usage, mesh);
        break;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
        accessor_to_attribute_internal<uint32_t>(model, accessor, name, target_usage, mesh);
        break;
    case TINYGLTF_COMPONENT_TYPE_FLOAT:
        accessor_to_attribute_internal<float>(model, accessor, name, target_usage, mesh);
        break;
    case TINYGLTF_COMPONENT_TYPE_DOUBLE:
        accessor_to_attribute_internal<double>(model, accessor, name, target_usage, mesh);
        break;
    default:
        logger().warn(
            "Unsupported component type {} for accessor {}",
            accessor.componentType,
            name);
    }
}

template <typename MeshType>
MeshType convert_tinygltf_primitive_to_lagrange_mesh(
    const tinygltf::Model& model,
    const tinygltf::Primitive& primitive,
    const LoadOptions& options)
{
    using Index = typename MeshType::Index;
    using Scalar = typename MeshType::Scalar;

    // each gltf mesh is made of one or more primitives.
    // Different primitives can reference different materials and data buffers.

    MeshType lmesh;
    la_runtime_assert(primitive.mode == TINYGLTF_MODE_TRIANGLES);

    // read vertices
    auto it = primitive.attributes.find("POSITION");
    la_runtime_assert(it != primitive.attributes.end(), "missing positions");
    {
        const tinygltf::Accessor& accessor = model.accessors[it->second];
        const size_t num_vertices = accessor.count;
        la_debug_assert(accessor.type == TINYGLTF_TYPE_VEC3);
        std::vector<Scalar> coords = load_buffer_data_as<Scalar>(model, accessor);
        lmesh.add_vertices(Index(num_vertices), coords);
    }

    // read faces
    {
        const tinygltf::Accessor& accessor = model.accessors[primitive.indices];
        const size_t num_indices = accessor.count;
        const size_t num_facets = num_indices / 3; // because triangle
        la_debug_assert(accessor.type == TINYGLTF_TYPE_SCALAR);

        std::vector<Index> indices = load_buffer_data_as<Index>(model, accessor);
        lmesh.add_triangles(Index(num_facets), indices);
    }

    // read other attributes
    for (auto& attribute : primitive.attributes) {
        if (starts_with(attribute.first, "POSITION")) continue; // already done

        const tinygltf::Accessor& accessor = model.accessors[attribute.second];

        const std::string& name = attribute.first;
        std::string name_lowercase = attribute.first;
        std::transform(
            name_lowercase.begin(),
            name_lowercase.end(),
            name_lowercase.begin(),
            [](unsigned char c) { return std::tolower(c); });

        // https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#meshes
        if (starts_with(name, "NORMAL") && options.load_normals) {
            accessor_to_attribute(model, accessor, name_lowercase, AttributeUsage::Normal, lmesh);
        } else if (starts_with(name, "TANGENT") && options.load_tangents) {
            accessor_to_attribute(model, accessor, name_lowercase, AttributeUsage::Tangent, lmesh);
        } else if (starts_with(name, "COLOR") && options.load_vertex_colors) {
            accessor_to_attribute(model, accessor, name_lowercase, AttributeUsage::Color, lmesh);
        } else if (starts_with(name, "JOINTS") && options.load_weights) {
            la_runtime_assert(accessor.type == TINYGLTF_TYPE_VEC4);
            la_runtime_assert(
                accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE ||
                accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT);
            accessor_to_attribute(model, accessor, name_lowercase, AttributeUsage::Vector, lmesh);
        } else if (starts_with(name, "WEIGHTS") && options.load_weights) {
            la_runtime_assert(accessor.type == TINYGLTF_TYPE_VEC4);
            la_runtime_assert(
                accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT ||
                accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE ||
                accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT);
            accessor_to_attribute(model, accessor, name_lowercase, AttributeUsage::Vector, lmesh);
        } else if (starts_with(name, "TEXCOORD") && options.load_uvs) {
            accessor_to_attribute(model, accessor, name_lowercase, AttributeUsage::UV, lmesh);
        } else {
            accessor_to_attribute(model, accessor, name_lowercase, {}, lmesh);
        }
    }
    // for future reference, material is here. No need in this function.
    // const tinygltf::Material& mat = model.materials[primitive.material];

    // convert texture coordinates from st to uv
    scene::utils::convert_texcoord_uv_st(lmesh);

    return lmesh;
}

template <typename Scalar>
Eigen::Transform<Scalar, 3, 2> get_node_transform(const tinygltf::Node& node)
{
    using AffineTransform = Eigen::Transform<Scalar, 3, 2>;
    AffineTransform t;
    if (!node.matrix.empty()) {
        for (size_t i = 0; i < 16; ++i) {
            t.data()[i] = Scalar(node.matrix[i]);
        }
    } else {
        AffineTransform translation = AffineTransform::Identity();
        AffineTransform rotation = AffineTransform::Identity();
        AffineTransform scale = AffineTransform::Identity();
        if (!node.translation.empty()) {
            translation = Eigen::Translation<Scalar, 3>(
                Scalar(node.translation[0]),
                Scalar(node.translation[1]),
                Scalar(node.translation[2]));
        }
        if (!node.rotation.empty()) {
            rotation = Eigen::Quaternion<Scalar>(
                Scalar(node.rotation[3]),
                Scalar(node.rotation[0]),
                Scalar(node.rotation[1]),
                Scalar(node.rotation[2]));
        }
        if (!node.scale.empty()) {
            scale = Eigen::Scaling<Scalar>(
                Scalar(node.scale[0]),
                Scalar(node.scale[1]),
                Scalar(node.scale[2]));
        }
        t = translation * rotation * scale;
    }
    return t;
}

template <typename SceneType>
SceneType load_simple_scene_gltf(const tinygltf::Model& model, const LoadOptions& options)
{
    using MeshType = typename SceneType::MeshType;
    using Scalar = typename MeshType::Scalar;
    using Index = typename MeshType::Index;
    using AffineTransform = typename SceneType::AffineTransform;

    // TODO: handle 2d SimpleScene

    SceneType lscene;

    for (const tinygltf::Mesh& mesh : model.meshes) {
        // By adding in the same order, we can assume that tinygltf's indexing is the same
        // as in the lagrange scene. We use this later.
        std::vector<MeshType> lmeshes;
        for (const tinygltf::Primitive& prim : mesh.primitives) {
            lmeshes.push_back(
                convert_tinygltf_primitive_to_lagrange_mesh<MeshType>(model, prim, options));
        }
        if (!lmeshes.empty()) {
            if (lmeshes.size() == 1) {
                lscene.add_mesh(lmeshes.front());
            } else {
                constexpr bool preserve_attributes = true;
                lscene.add_mesh(
                    lagrange::combine_meshes<typename MeshType::Scalar, typename MeshType::Index>(
                        lmeshes,
                        preserve_attributes));
            }
        }
    }

    std::function<void(const tinygltf::Node&, const AffineTransform&)> visit_node;
    visit_node = [&](const tinygltf::Node& node, const AffineTransform& parent_transform) -> void {
        AffineTransform node_transform = AffineTransform::Identity();
        if constexpr (SceneType::Dim == 3) {
            node_transform = get_node_transform<Scalar>(node);
        } else {
            // TODO: convert 3d transforms into 2d
            logger().warn("Ignoring 3d node transform while loading 2d scene");
        }

        AffineTransform global_transform = parent_transform * node_transform;
        if (node.mesh != -1) {
            lscene.add_instance({Index(node.mesh), global_transform});
        }

        for (int child_idx : node.children) {
            visit_node(model.nodes[child_idx], global_transform);
        }
    };

    if (model.scenes.empty()) {
        logger().warn("glTF does not contain any scene.");
    } else {
        int scene_id = model.defaultScene;
        if (scene_id < 0) {
            logger().warn("No default scene selected. Using the first available scene.");
            scene_id = 0;
        }
        for (int node_idx : model.scenes[scene_id].nodes) {
            AffineTransform root_transform = AffineTransform::Identity();
            visit_node(model.nodes[node_idx], root_transform);
        }
    }

    return lscene;
}

template <typename SceneType>
SceneType load_scene_gltf(const tinygltf::Model& model, const LoadOptions& options)
{
    SceneType lscene;
    using MeshType = typename SceneType::MeshType;

    // saves the number of primitives for each mesh. Sums up to the current one.
    // E.g. if the model has 4 meshes with 2 prims each, this vector will be [0, 2, 4, 6].
    // This is used later to find mesh index in the nodes.
    std::vector<size_t> primitive_count;
    size_t primitive_count_tmp = 0;

    for (const tinygltf::Mesh& mesh : model.meshes) {
        for (const tinygltf::Primitive& primitive : mesh.primitives) {
            scene::utils::add_mesh(
                lscene,
                convert_tinygltf_primitive_to_lagrange_mesh<MeshType>(model, primitive, options));
        }
        primitive_count.push_back(primitive_count_tmp);
        primitive_count_tmp += mesh.primitives.size();
    }

    auto convert_map_mode = [](int mode) -> scene::Texture::WrapMode {
        switch (mode) {
        case TINYGLTF_TEXTURE_WRAP_REPEAT: return scene::Texture::WrapMode::Wrap;
        case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE: return scene::Texture::WrapMode::Clamp;
        case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT: return scene::Texture::WrapMode::Mirror;
        default: return scene::Texture::WrapMode::Wrap;
        }
    };
    for (const tinygltf::Texture& texture : model.textures) {
        scene::Texture ltex;
        ltex.image = texture.source;
        if (texture.sampler >= 0) {
            const tinygltf::Sampler& sampler = model.samplers[texture.sampler];
            ltex.mag_filter = scene::Texture::TextureFilter{sampler.magFilter};
            ltex.min_filter = scene::Texture::TextureFilter{sampler.minFilter};
            ltex.wrap_u = convert_map_mode(sampler.wrapS);
            ltex.wrap_v = convert_map_mode(sampler.wrapT);
        }
        lscene.textures.emplace_back(std::move(ltex));
    }

    for (const tinygltf::Material& material : model.materials) {
        scene::MaterialExperimental lmat;
        lmat.name = material.name;
        lmat.double_sided = material.doubleSided;

        lmat.base_color_value = to_vec4(material.pbrMetallicRoughness.baseColorFactor);
        lmat.base_color_texture.index = material.pbrMetallicRoughness.baseColorTexture.index;
        lmat.base_color_texture.texcoord = material.pbrMetallicRoughness.baseColorTexture.texCoord;

        lmat.emissive_value = to_vec3(material.emissiveFactor);
        lmat.emissive_texture.index = material.emissiveTexture.index;
        lmat.emissive_texture.texcoord = material.emissiveTexture.texCoord;

        lmat.metallic_value = material.pbrMetallicRoughness.metallicFactor;
        lmat.roughness_value = material.pbrMetallicRoughness.roughnessFactor;
        lmat.metallic_roughness_texture.index =
            material.pbrMetallicRoughness.metallicRoughnessTexture.index;
        lmat.metallic_roughness_texture.texcoord =
            material.pbrMetallicRoughness.metallicRoughnessTexture.texCoord;

        lmat.normal_texture.index = material.normalTexture.index;
        lmat.normal_texture.texcoord = material.normalTexture.texCoord;
        lmat.normal_scale = material.normalTexture.scale;

        lmat.occlusion_texture.index = material.occlusionTexture.index;
        lmat.occlusion_texture.texcoord = material.occlusionTexture.texCoord;
        lmat.occlusion_strength = material.occlusionTexture.strength;

        lmat.alpha_cutoff = material.alphaCutoff;
        if (material.alphaMode == "OPAQUE") {
            lmat.alpha_mode = scene::MaterialExperimental::AlphaMode::Opaque;
        } else if (material.alphaMode == "MASK") {
            lmat.alpha_mode = scene::MaterialExperimental::AlphaMode::Mask;
        } else if (material.alphaMode == "BLEND") {
            lmat.alpha_mode = scene::MaterialExperimental::AlphaMode::Blend;
        }

        if (!material.extensions.empty()) {
            lmat.extensions = convert_extension_map(material.extensions, options);
        }

        lscene.materials.push_back(lmat);
    }
    for (const tinygltf::Animation& animation : model.animations) {
        scene::Animation lanim;
        lanim.name = animation.name;
        // TODO

        if (!animation.extensions.empty()) {
            lanim.extensions = convert_extension_map(animation.extensions, options);
        }

        lscene.animations.push_back(lanim);
    }
    for (const tinygltf::Image& image : model.images) {
        // Note:
        // We assume the image data was loaded by tinygltf.
        // If this assumptions does not always hold, then we will have
        // to instead read the gltf buffer or file from disk.
        la_runtime_assert(image.width > 0);
        la_runtime_assert(image.height > 0);
        la_runtime_assert(image.component > 0);
        la_runtime_assert(
            static_cast<int>(image.image.size()) == image.width * image.height * image.component);

        scene::ImageLegacy limage;
        limage.name = image.name;
        limage.uri = image.uri;
        fs::path path;
        if (!image.uri.empty()) path = image.uri;
        if (image.mimeType == "image/jpeg" || path.extension() == ".jpg" ||
            path.extension() == ".jpeg") {
            limage.type = scene::ImageLegacy::Type::Jpeg;
        } else if (image.mimeType == "image/png" || path.extension() == ".png") {
            limage.type = scene::ImageLegacy::Type::Png;
        } else if (image.mimeType == "image/bmp" || path.extension() == ".bmp") {
            limage.type = scene::ImageLegacy::Type::Bmp;
        } else if (image.mimeType == "image/gif" || path.extension() == ".gif") {
            limage.type = scene::ImageLegacy::Type::Gif;
        } else {
            limage.type = scene::ImageLegacy::Type::Unknown;
        }
        limage.width = image.width;
        limage.height = image.height;
        limage.channel = [](int component) {
            switch (component) {
            case 1: return image::ImageChannel::one;
            case 3: return image::ImageChannel::three;
            case 4: return image::ImageChannel::four;
            default:
                logger().warn("Loading image with unsupported number of channels!");
                return image::ImageChannel::unknown;
            }
        }(image.component);
        limage.precision = [](int precision) {
            switch (precision) {
            case TINYGLTF_COMPONENT_TYPE_BYTE: return image::ImagePrecision::int8;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: return image::ImagePrecision::uint8;
            case TINYGLTF_COMPONENT_TYPE_INT: return image::ImagePrecision::int32;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: return image::ImagePrecision::uint32;
            case TINYGLTF_COMPONENT_TYPE_FLOAT: return image::ImagePrecision::float32;
            case TINYGLTF_COMPONENT_TYPE_DOUBLE: return image::ImagePrecision::float64;
            case TINYGLTF_COMPONENT_TYPE_SHORT: // unsupported, fallthrough
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: // unsupported, fallthrough
            default:
                logger().warn("Loading image with unsupported pixel precision!");
                return image::ImagePrecision::unknown;
            }
        }(image.pixel_type);
        limage.data = std::make_unique<image::ImageStorage>(
            limage.get_element_size() * image.width * image.component,
            image.height,
            1);
        std::copy(image.image.begin(), image.image.end(), limage.data->data());

        if (!image.extensions.empty()) {
            limage.extensions = convert_extension_map(image.extensions, options);
        }

        lscene.images.emplace_back(std::move(limage));
    }

    for (const tinygltf::Light& light : model.lights) {
        // note that this is not part of the gltf official spec, it is an extension.
        // https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_lights_punctual/README.md
        scene::Light llight;
        llight.name = light.name;
        Eigen::Vector3f color;
        color << light.color[0], light.color[1], light.color[2];
        llight.color_diffuse = color;
        llight.color_ambient = color;
        llight.color_specular = color;
        llight.intensity = light.intensity;
        llight.range = light.range;
        if (light.type == "directional") {
            llight.type = scene::Light::Type::Directional;
        } else if (light.type == "point") {
            llight.type = scene::Light::Type::Point;
        } else if (light.type == "spot") {
            llight.type = scene::Light::Type::Spot;
            llight.angle_inner_cone = light.spot.innerConeAngle;
            llight.angle_outer_cone = light.spot.outerConeAngle;
        } else {
            llight.type = scene::Light::Type::Undefined;
        }
        // this extension does not define ambient or area lights.

        if (!light.extensions.empty()) {
            llight.extensions = convert_extension_map(light.extensions, options);
        }

        lscene.lights.push_back(llight);
    }

    for (const tinygltf::Camera& camera : model.cameras) {
        scene::Camera lcam;
        lcam.name = camera.name;
        if (camera.type == "perspective") {
            lcam.type = scene::Camera::Type::Perspective;
            lcam.aspect_ratio = camera.perspective.aspectRatio;
            lcam.set_horizontal_fov_from_vertical_fov(camera.perspective.yfov);
            lcam.near_plane = camera.perspective.znear;
            lcam.far_plane = camera.perspective.zfar;
        } else {
            lcam.type = scene::Camera::Type::Orthographic;
            lcam.near_plane = camera.orthographic.znear;
            lcam.far_plane = camera.orthographic.zfar;
            lcam.aspect_ratio = camera.orthographic.xmag / camera.orthographic.ymag;
            lcam.orthographic_width = camera.orthographic.xmag;
            lcam.horizontal_fov = 0.f;
        }
        // gltf camera does not specify a position, so we leave them to default.

        if (!camera.extensions.empty()) {
            lcam.extensions = convert_extension_map(camera.extensions, options);
        }

        lscene.cameras.push_back(lcam);
    }
    // TODO model.skins ?

    std::function<size_t(const tinygltf::Node&, size_t parent_idx)> create_node;
    create_node = [&](const tinygltf::Node& node, size_t parent_idx) -> size_t {
        size_t lnode_idx = lscene.nodes.size();
        lscene.nodes.push_back(scene::Node());
        auto& lnode = lscene.nodes.back();

        lnode.name = node.name;
        lnode.transform = get_node_transform<float>(node);
        lnode.parent = parent_idx;

        if (node.camera >= 0) lnode.cameras.push_back(node.camera);
        if (node.mesh >= 0) {
            const tinygltf::Mesh& mesh = model.meshes[node.mesh];
            for (size_t i = 0; i < mesh.primitives.size(); ++i) {
                size_t mesh_idx = primitive_count[node.mesh] + i;
                size_t material_idx = mesh.primitives[i].material;
                lnode.meshes.push_back({mesh_idx, {material_idx}});
            }
        }
        if (!node.extensions.empty()) {
            lnode.extensions = convert_extension_map(node.extensions, options);
        }

        // TODO handle skin property

        size_t children_count = node.children.size();
        lnode.children.resize(children_count);
        for (size_t i = 0; i < children_count; ++i) {
            int child_idx = node.children[i];
            lnode.children[i] = create_node(model.nodes[child_idx], lnode_idx);
        }
        return lnode_idx;
    };

    if (model.scenes.empty()) {
        logger().warn("glTF does not contain any scene.");
    } else {
        int scene_id = model.defaultScene;
        if (scene_id < 0) {
            logger().warn("No default scene selected. Using the first available scene.");
            scene_id = 0;
        }

        const tinygltf::Scene& scene = model.scenes[scene_id];

        // we still traverse the hierarchy rather than copying the list of node for 2 reasons:
        // 1. we need to set the node's parent index
        // 2. we only traverse the hierarchy of the selected scene
        lscene.nodes.reserve(model.nodes.size());
        for (int lnode_idx : scene.nodes) {
            size_t root_index = create_node(model.nodes[lnode_idx], lagrange::invalid<size_t>());
            lscene.root_nodes.push_back(root_index);
        }

        if (!scene.extensions.empty()) {
            lscene.extensions = convert_extension_map(scene.extensions, options);
        }
    }

    // also add the model extensions to the scene
    if (!model.extensions.empty()) {
        auto extensions = convert_extension_map(model.extensions, options);
        lscene.extensions.data.insert(extensions.data.begin(), extensions.data.end());
        lscene.extensions.user_data.insert(
            extensions.user_data.begin(),
            extensions.user_data.end());
    }

    return lscene;
}

} // namespace

// =====================================
// load_mesh_gltf.h
// =====================================
template <typename MeshType>
MeshType load_mesh_gltf(std::istream& input_stream, const LoadOptions& options)
{
    using Scalar = typename MeshType::Scalar;
    using Index = typename MeshType::Index;
    using SceneType = scene::SimpleScene<Scalar, Index, 3u>; // glTF only supports 3d meshes
    return scene::simple_scene_to_mesh(load_simple_scene_gltf<SceneType>(input_stream, options));
}

template <typename MeshType>
MeshType load_mesh_gltf(const fs::path& filename, const LoadOptions& options)
{
    using Scalar = typename MeshType::Scalar;
    using Index = typename MeshType::Index;
    using SceneType = scene::SimpleScene<Scalar, Index, 3u>; // glTF only supports 3d meshes
    return scene::simple_scene_to_mesh(load_simple_scene_gltf<SceneType>(filename, options));
}

// =====================================
// load_simple_scene_gltf.h
// =====================================
template <typename SceneType>
SceneType load_simple_scene_gltf(const fs::path& filename, const LoadOptions& options)
{
    tinygltf::Model model = load_tinygltf(filename);
    return load_simple_scene_gltf<SceneType>(model, options);
}
template <typename SceneType>
SceneType load_simple_scene_gltf(std::istream& input_stream, const LoadOptions& options)
{
    tinygltf::Model model = load_tinygltf(input_stream);
    return load_simple_scene_gltf<SceneType>(model, options);
}

// =====================================
// load_scene_gltf.h
// =====================================
template <typename SceneType>
SceneType load_scene_gltf(const fs::path& filename, const LoadOptions& options)
{
    tinygltf::Model model = load_tinygltf(filename);
    return load_scene_gltf<SceneType>(model, options);
}
template <typename SceneType>
SceneType load_scene_gltf(std::istream& input_stream, const LoadOptions& options)
{
    tinygltf::Model model = load_tinygltf(input_stream);
    return load_scene_gltf<SceneType>(model, options);
}

// =====================================
// explicit template instantiations
// =====================================

#define LA_X_load_mesh_gltf(_, S, I)                                                \
    template SurfaceMesh<S, I> load_mesh_gltf(const fs::path&, const LoadOptions&); \
    template SurfaceMesh<S, I> load_mesh_gltf(std::istream&, const LoadOptions&);
LA_SURFACE_MESH_X(load_mesh_gltf, 0);
#undef LA_X_load_mesh_gltf

#define LA_X_load_simple_scene_gltf(_, S, I, D)                  \
    template scene::SimpleScene<S, I, D> load_simple_scene_gltf( \
        const fs::path& filename,                                \
        const LoadOptions& options);                             \
    template scene::SimpleScene<S, I, D> load_simple_scene_gltf( \
        std::istream& input_stream,                              \
        const LoadOptions& options);
LA_SIMPLE_SCENE_X(load_simple_scene_gltf, 0);
#undef LA_X_load_simple_scene_gltf

#define LA_X_load_scene_gltf(_, S, I)            \
    template scene::Scene<S, I> load_scene_gltf( \
        const fs::path& filename,                \
        const LoadOptions& options);             \
    template scene::Scene<S, I> load_scene_gltf( \
        std::istream& input_stream,              \
        const LoadOptions& options);
LA_SCENE_X(load_scene_gltf, 0);
#undef LA_X_load_scene_gltf

} // namespace lagrange::io
