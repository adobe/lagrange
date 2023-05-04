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
#include <lagrange/io/internal/load_gltf.h>
#include <lagrange/io/load_mesh_gltf.h>
#include <lagrange/io/load_simple_scene_gltf.h>
// ====

#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/combine_meshes.h>
#include <lagrange/internal/skinning.h>
#include <lagrange/scene/SimpleSceneTypes.h>
#include <lagrange/utils/Error.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/strings.h>

#include <tiny_gltf.h>
#include <Eigen/Geometry>

#include <istream>
#include <optional>

namespace lagrange::io {
namespace internal {

namespace {

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
} // namespace

// =====================================
// internal/load_gltf.h
// =====================================
tinygltf::Model load_tinygltf(std::istream& input_stream)
{
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
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

    if (!warn.empty()) logger().warn("%s", warn.c_str());
    if (!ret || !err.empty()) {
        throw std::runtime_error(err);
    }

    return model;
}

tinygltf::Model load_tinygltf(const fs::path& filename)
{
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;
    bool ret;
    if (to_lower(filename.extension().string()) == ".gltf") {
        ret = loader.LoadASCIIFromFile(&model, &err, &warn, filename.string());
    } else {
        la_runtime_assert(to_lower(filename.extension().string()) == ".glb");
        ret = loader.LoadBinaryFromFile(&model, &err, &warn, filename.string());
    }

    if (!warn.empty()) logger().warn("%s", warn.c_str());
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
            // Matrices are flattend as vectors for now.
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
MeshType convert_mesh_tinygltf_to_lagrange(
    const tinygltf::Model& model,
    const tinygltf::Mesh& mesh,
    const LoadOptions& options)
{
    using Index = typename MeshType::Index;
    using Scalar = typename MeshType::Scalar;

    // each gltf mesh is made of one or more primitives.
    // Different primitives can reference different materials and data buffers.
    std::vector<MeshType> lmeshes;

    for (size_t i = 0; i < mesh.primitives.size(); ++i) {
        MeshType lmesh;
        const tinygltf::Primitive& primitive = mesh.primitives[i];
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
                accessor_to_attribute(
                    model,
                    accessor,
                    name_lowercase,
                    AttributeUsage::Normal,
                    lmesh);
            } else if (starts_with(name, "TANGENT") && options.load_tangents) {
                accessor_to_attribute(
                    model,
                    accessor,
                    name_lowercase,
                    AttributeUsage::Tangent,
                    lmesh);
            } else if (starts_with(name, "COLOR") && options.load_vertex_colors) {
                accessor_to_attribute(
                    model,
                    accessor,
                    name_lowercase,
                    AttributeUsage::Color,
                    lmesh);
            } else if (starts_with(name, "JOINTS") && options.load_weights) {
                la_runtime_assert(accessor.type == TINYGLTF_TYPE_VEC4);
                la_runtime_assert(
                    accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE ||
                    accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT);
                accessor_to_attribute(
                    model,
                    accessor,
                    AttributeName::indexed_joint,
                    AttributeUsage::Vector,
                    lmesh);
            } else if (starts_with(name, "WEIGHTS") && options.load_weights) {
                la_runtime_assert(accessor.type == TINYGLTF_TYPE_VEC4);
                la_runtime_assert(
                    accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT ||
                    accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE ||
                    accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT);
                accessor_to_attribute(
                    model,
                    accessor,
                    AttributeName::indexed_weight,
                    AttributeUsage::Vector,
                    lmesh);
            } else if (starts_with(name, "TEXCOORD") && options.load_uvs) {
                accessor_to_attribute(
                    model,
                    accessor,
                    AttributeName::texcoord,
                    AttributeUsage::UV,
                    lmesh);
            } else {
                accessor_to_attribute(model, accessor, name_lowercase, {}, lmesh);
            }
        }


        // for future reference, material is here. No need in this function.
        // const tinygltf::Material& mat = model.materials[primitive.material];

        lmeshes.push_back(lmesh);
    }

    if (lmeshes.size() == 1) {
        return lmeshes.front();
    } else {
        constexpr bool preserve_attributes = true;
        return combine_meshes<Scalar, Index>(lmeshes, preserve_attributes);
    }
}
#define LA_X_convert_mesh_tinygltf_to_lagrange(_, S, I)           \
    template SurfaceMesh<S, I> convert_mesh_tinygltf_to_lagrange( \
        const tinygltf::Model& model,                             \
        const tinygltf::Mesh& mesh,                               \
        const LoadOptions& options);
LA_SURFACE_MESH_X(convert_mesh_tinygltf_to_lagrange, 0);
#undef LA_X_convert_mesh_tinygltf_to_lagrange

template <typename MeshType>
MeshType load_mesh_gltf(const tinygltf::Model& model, const LoadOptions& options)
{
    la_runtime_assert(!model.meshes.empty());
    if (model.meshes.size() == 1) {
        return convert_mesh_tinygltf_to_lagrange<MeshType>(model, model.meshes[0], options);
    } else {
        std::vector<MeshType> meshes(model.meshes.size());
        for (size_t i = 0; i < model.meshes.size(); ++i) {
            meshes[i] =
                convert_mesh_tinygltf_to_lagrange<MeshType>(model, model.meshes[i], options);
        }
        constexpr bool preserve_attributes = true;
        return lagrange::combine_meshes<typename MeshType::Scalar, typename MeshType::Index>(
            meshes,
            preserve_attributes);
    }
}
#define LA_X_load_mesh_gltf(_, S, I)           \
    template SurfaceMesh<S, I> load_mesh_gltf( \
        const tinygltf::Model& model,          \
        const LoadOptions& options);
LA_SURFACE_MESH_X(load_mesh_gltf, 0);
#undef LA_X_load_mesh_gltf

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
        lscene.add_mesh(convert_mesh_tinygltf_to_lagrange<MeshType>(model, mesh, options));
    }

    std::function<void(const tinygltf::Node&, const AffineTransform&)> visit_node;
    visit_node = [&](const tinygltf::Node& node, const AffineTransform& parent_transform) -> void {
        AffineTransform node_transform = AffineTransform::Identity();
        if constexpr (SceneType::Dim == 3) {
            if (!node.matrix.empty()) {
                // gltf stores in column-major order, so we should be good
                for (size_t i = 0; i < 16; ++i) {
                    node_transform.data()[i] = Scalar(node.matrix[i]);
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
                node_transform = translation * rotation * scale;
            }
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

#define LA_X_load_simple_scene_gltf(_, S, I, D)                  \
    template scene::SimpleScene<S, I, D> load_simple_scene_gltf( \
        const tinygltf::Model& model,                            \
        const LoadOptions& options);
LA_SIMPLE_SCENE_X(load_simple_scene_gltf, 0);
#undef LA_X_load_simple_scene_gltf


} // namespace internal


// =====================================
// load_mesh_gltf.h
// =====================================
template <typename MeshType>
MeshType load_mesh_gltf(std::istream& input_stream, const LoadOptions& options)
{
    tinygltf::Model model = internal::load_tinygltf(input_stream);
    return internal::load_mesh_gltf<MeshType>(model, options);
}

template <typename MeshType>
MeshType load_mesh_gltf(const fs::path& filename, const LoadOptions& options)
{
    tinygltf::Model model = internal::load_tinygltf(filename);
    return internal::load_mesh_gltf<MeshType>(model, options);
}

#define LA_X_load_mesh_gltf(_, S, I)                                                \
    template SurfaceMesh<S, I> load_mesh_gltf(const fs::path&, const LoadOptions&); \
    template SurfaceMesh<S, I> load_mesh_gltf(std::istream&, const LoadOptions&);
LA_SURFACE_MESH_X(load_mesh_gltf, 0);
#undef LA_X_load_mesh_gltf


// =====================================
// load_simple_scene_gltf.h
// =====================================
template <typename SceneType>
SceneType load_simple_scene_gltf(const fs::path& filename, const LoadOptions& options)
{
    tinygltf::Model model = internal::load_tinygltf(filename);
    return internal::load_simple_scene_gltf<SceneType>(model, options);
}
template <typename SceneType>
SceneType load_simple_scene_gltf(std::istream& input_stream, const LoadOptions& options)
{
    tinygltf::Model model = internal::load_tinygltf(input_stream);
    return internal::load_simple_scene_gltf<SceneType>(model, options);
}
#define LA_X_load_simple_scene_gltf(_, S, I, D)                  \
    template scene::SimpleScene<S, I, D> load_simple_scene_gltf( \
        const fs::path& filename,                                \
        const LoadOptions& options);                             \
    template scene::SimpleScene<S, I, D> load_simple_scene_gltf( \
        std::istream&,                                           \
        const LoadOptions& options);
LA_SIMPLE_SCENE_X(load_simple_scene_gltf, 0);
#undef LA_X_load_simple_scene_gltf

} // namespace lagrange::io
