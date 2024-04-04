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
#include <lagrange/io/api.h>
#include <lagrange/io/save_mesh_gltf.h>
#include <lagrange/io/save_scene_gltf.h>
#include <lagrange/io/save_simple_scene_gltf.h>

// ====

#include <lagrange/Attribute.h>
#include <lagrange/AttributeValueType.h>
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/foreach_attribute.h>
#include <lagrange/internal/string_from_scalar.h>
#include <lagrange/scene/SceneTypes.h>
#include <lagrange/scene/SimpleSceneTypes.h>
#include <lagrange/scene/scene_utils.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/safe_cast.h>
#include <lagrange/utils/strings.h>
#include <lagrange/views.h>

#include "internal/convert_attribute_utils.h"

#include <tiny_gltf.h>

#include <ostream>

namespace lagrange::io {

//
// Important:
// gltf standard requires all accessors of the same primitive to have the same count and indexing.
// This means all attributes must be indexed in the same way. This includes including positions,
// UVs, normals, etc.
// Lagrange supports indexing attributes in different ways. For example, on a cube, we can have 8
// vertices, but 14 UV coordinates, but this would be invalid in the gltf export.
//
// We call `unify_index_buffer` if options.attribute_conversion_policy == ConvertAsNeeded
//

namespace {
tinygltf::Value convert_value(const scene::Value& value)
{
    switch (value.get_type_index()) {
    case scene::Value::bool_index(): return tinygltf::Value(value.get_bool());
    case scene::Value::int_index(): return tinygltf::Value(value.get_int());
    case scene::Value::real_index(): return tinygltf::Value(value.get_real());
    case scene::Value::string_index(): return tinygltf::Value(value.get_string());
    case scene::Value::buffer_index(): {
        scene::Value::Buffer copy = value.get_buffer();
        return tinygltf::Value(std::move(copy));
    }
    case scene::Value::array_index(): {
        tinygltf::Value::Array arr; // std::vector
        for (size_t i = 0; i < value.size(); ++i) {
            arr.push_back(convert_value(value[i]));
        }
        return tinygltf::Value(std::move(arr));
    }
    case scene::Value::object_index(): {
        tinygltf::Value::Object obj; // std::map
        for (const auto& [key, val] : value.get_object()) {
            obj.insert({key, convert_value(val)});
        }
        return tinygltf::Value(std::move(obj));
    }
    default: return tinygltf::Value();
    }
}

tinygltf::ExtensionMap convert_extension_map(
    const scene::Extensions& extensions,
    const SaveOptions& options)
{
    // temporary map with both the default extensions and the converted user ones.
    std::unordered_map<std::string, scene::Value> map = extensions.data;

    // convert supported user extensions to lagrange::scene::Value
    for (const auto& [key, value] : extensions.user_data) {
        for (const auto* extension : options.extension_converters) {
            if (extension->can_write(key)) {
                map.insert({key, extension->write(value)});
            }
        }
    }

    tinygltf::ExtensionMap out;
    for (const auto& [key, value] : map) {
        out.insert({key, convert_value(value)});
    }
    return out;
}

std::vector<double> to_vec3(const Eigen::Vector3f& v)
{
    return {v(0), v(1), v(2)};
}
std::vector<double> to_vec4(const Eigen::Vector4f& v)
{
    return {v(0), v(1), v(2), v(3)};
}

void save_gltf(const fs::path& filename, const tinygltf::Model& model, const SaveOptions& options)
{
    bool binary = to_lower(filename.extension().string()) == ".glb";
    if (binary && options.encoding != FileEncoding::Binary) {
        logger().warn("Saving mesh in binary due to `.glb` extension.");
    } else if (!binary && options.encoding != FileEncoding::Ascii) {
        // this is extremely common, since the default value of `encoding` is binary.
        // But trying to save as `.gltf` is explicit enough, so skip this message.

        // logger().warn("Saving mesh in ascii due to `.gltf` extension.");
    }

    // https://github.com/syoyo/tinygltf/issues/323
    tinygltf::WriteImageDataFunction write_image_data_function = &tinygltf::WriteImageData;
    tinygltf::FsCallbacks fs_callbacks;
    fs_callbacks.FileExists = &tinygltf::FileExists;
    fs_callbacks.ExpandFilePath = &tinygltf::ExpandFilePath;
    fs_callbacks.ReadWholeFile = &tinygltf::ReadWholeFile;
    fs_callbacks.WriteWholeFile = &tinygltf::WriteWholeFile;
    fs_callbacks.user_data = nullptr;

    tinygltf::TinyGLTF loader;
    loader.SetImageWriter(write_image_data_function, &fs_callbacks);

    constexpr bool embed_buffers = true;
    constexpr bool pretty_print = true;
    bool success = loader.WriteGltfSceneToFile(
        &model,
        filename.string(),
        options.embed_images,
        embed_buffers,
        pretty_print,
        binary);
    if (!success) logger().error("Error saving {}", filename.string());
}

void save_gltf(
    std::ostream& output_stream,
    const tinygltf::Model& model,
    const SaveOptions& options)
{
    bool binary = options.encoding == FileEncoding::Binary;
    tinygltf::TinyGLTF loader;
    constexpr bool pretty_print = true;
    bool success = loader.WriteGltfSceneToStream(&model, output_stream, pretty_print, binary);
    if (!success) logger().error("Error writing gltf file to stream");
}

// returns data, tmp
template <typename from_t, typename to_t>
span<const to_t> get_attribute_as(span<const from_t> data, std::vector<to_t>& tmp)
{
    if constexpr (std::is_same_v<from_t, to_t>) {
        return data;
    } else {
        tmp.resize(data.size());
        for (size_t i = 0; i < data.size(); ++i) {
            tmp[i] = to_t(data[i]);
        }
        return tmp;
    }
}

// returns buffer_index, byte_offset, byte_length
template <typename T>
std::tuple<int, size_t, size_t> write_to_buffer(tinygltf::Model& model, span<const T> data)
{
    constexpr uint64_t buffer_max_size = ((uint64_t)2 << 32) - 1; // 2^32 - 1

    if (model.buffers.empty()) {
        model.buffers.push_back(tinygltf::Buffer());
    }

    tinygltf::Buffer& buffer = model.buffers.back();
    size_t byte_length = data.size_bytes();
    size_t byte_offset = buffer.data.size();
    if ((uint64_t)byte_offset + (uint64_t)byte_length > buffer_max_size) {
        // a single data block cannot exceed the maximum buffer size
        la_runtime_assert((uint64_t)byte_length < buffer_max_size);
        model.buffers.push_back(tinygltf::Buffer());
        buffer = model.buffers.back();
        byte_offset = 0;
    }
    buffer.data.resize(byte_offset + byte_length);

    static_assert(sizeof(std::byte) == sizeof(decltype(*buffer.data.data())));
    const auto data_bytes = as_bytes(data);
    std::copy(
        data_bytes.begin(),
        data_bytes.end(),
        reinterpret_cast<std::byte*>(buffer.data.data()) + byte_offset);

    return {int(model.buffers.size()) - 1, byte_offset, byte_length};
}

template <typename Scalar, typename Index>
void populate_vertices(
    tinygltf::Model& model,
    tinygltf::Primitive& primitive,
    const SurfaceMesh<Scalar, Index>& lmesh)
{
    std::vector<float> tmp;
    span<const float> data =
        get_attribute_as<Scalar, float>(lmesh.get_vertex_to_position().get_all(), tmp);
    auto [buffer_index, byte_offset, byte_length] = write_to_buffer<float>(model, data);

    tinygltf::BufferView buffer_view;
    buffer_view.buffer = buffer_index;
    buffer_view.byteLength = byte_length;
    buffer_view.byteOffset = byte_offset;
    buffer_view.target = TINYGLTF_TARGET_ARRAY_BUFFER;
    int buffer_view_index = int(model.bufferViews.size());
    model.bufferViews.push_back(buffer_view);

    tinygltf::Accessor accessor;
    accessor.bufferView = buffer_view_index;
    accessor.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
    accessor.type = TINYGLTF_TYPE_VEC3;
    accessor.count = lmesh.get_num_vertices();
    auto V = vertex_view(lmesh);
    auto bb_max = V.colwise().maxCoeff();
    auto bb_min = V.colwise().minCoeff();
    accessor.maxValues = {double(bb_max(0)), double(bb_max(1)), double(bb_max(2))};
    accessor.minValues = {double(bb_min(0)), double(bb_min(1)), double(bb_min(2))};
    int accessor_index = int(model.accessors.size());
    model.accessors.push_back(accessor);

    primitive.attributes["POSITION"] = accessor_index;
}

template <typename Scalar, typename Index>
void populate_facets(
    tinygltf::Model& model,
    tinygltf::Primitive& primitive,
    const SurfaceMesh<Scalar, Index>& lmesh)
{
    // TODO: Does gltf require uint32_t as index type?
    std::vector<uint32_t> tmp;
    span<const uint32_t> data =
        get_attribute_as<Index, uint32_t>(lmesh.get_corner_to_vertex().get_all(), tmp);

    auto [buffer_index, byte_offset, byte_length] = write_to_buffer<uint32_t>(model, data);

    tinygltf::BufferView buffer_view;
    buffer_view.buffer = buffer_index;
    buffer_view.byteLength = byte_length;
    buffer_view.byteOffset = byte_offset;
    buffer_view.target = TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER;
    int buffer_view_index = int(model.bufferViews.size());
    model.bufferViews.push_back(buffer_view);

    tinygltf::Accessor accessor;
    accessor.bufferView = buffer_view_index;
    accessor.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT;
    accessor.type = TINYGLTF_TYPE_SCALAR;
    accessor.count = lmesh.get_num_facets() * lmesh.get_vertex_per_facet();
    int accessor_index = int(model.accessors.size());
    model.accessors.push_back(accessor);

    primitive.indices = accessor_index;
}

template <typename Scalar, typename Index>
void populate_attributes(
    tinygltf::Model& model,
    tinygltf::Primitive& primitive,
    const SurfaceMesh<Scalar, Index>& lmesh,
    const SaveOptions& options)
{
    bool found_normal = false;
    bool found_tangent = false;
    int texcoord_count = 0;
    int color_count = 0;
    // TODO: The following is an O(N^2) approach, rewrite it to be O(N).
    seq_foreach_named_attribute_read(lmesh, [&](std::string_view name, auto&& attr) {
        // TODO: change this for the attribute visitor that takes id and simplify this.

        if (lmesh.attr_name_is_reserved(name)) return;
        AttributeId id = lmesh.get_attribute_id(name);
        if (options.output_attributes == SaveOptions::OutputAttributes::SelectedOnly) {
            if (std::find(
                    options.selected_attributes.begin(),
                    options.selected_attributes.end(),
                    id) == options.selected_attributes.end()) {
                return;
            }
        }
        using AttributeType = std::decay_t<decltype(attr)>;
        using ValueType = typename AttributeType::ValueType;

        if constexpr (AttributeType::IsIndexed) {
            const auto& indices = attr.indices();
            if (vector_view(indices) != vector_view(lmesh.get_corner_to_vertex())) {
                // Indexed attributes are supported IF their indexing matches the vertices.
                // This should be the case if you call `unify_index_buffer`
                logger().warn(
                    "Skipping attribute `{}`: unsupported non-indexed. Consider calling "
                    "`unify_index_buffer`",
                    name);
                return;
            }
        }

        tinygltf::Accessor accessor;
        if constexpr (std::is_same_v<ValueType, char>) {
            accessor.componentType = TINYGLTF_COMPONENT_TYPE_BYTE;
        } else if constexpr (std::is_same_v<ValueType, unsigned char>) {
            accessor.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;
        } else if constexpr (std::is_same_v<ValueType, uint16_t>) {
            accessor.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT;
        } else if constexpr (
            std::is_same_v<ValueType, unsigned int> || std::is_same_v<ValueType, uint32_t>) {
            accessor.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT;
        } else if constexpr (
            std::is_same_v<ValueType, int> || std::is_same_v<ValueType, int32_t> ||
            std::is_same_v<ValueType, int64_t> || std::is_same_v<ValueType, uint64_t> ||
            std::is_same_v<ValueType, size_t>) {
            // special case: convert signed to unsigned, and 64 bits to 32 bits
            accessor.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT;
        } else if constexpr (std::is_same_v<ValueType, float>) {
            accessor.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
        } else if constexpr (std::is_same_v<ValueType, double>) {
            // special case: convert double to float
            accessor.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
        } else {
            logger().warn(
                "Skipping attribute `{}`: unsupported type {}",
                name,
                internal::string_from_scalar<ValueType>());
            return;
        }

        switch (attr.get_num_channels()) {
        case 1: accessor.type = TINYGLTF_TYPE_SCALAR; break;
        case 2: accessor.type = TINYGLTF_TYPE_VEC2; break;
        case 3: accessor.type = TINYGLTF_TYPE_VEC3; break;
        case 4: accessor.type = TINYGLTF_TYPE_VEC4; break;
        // note that we have no way to know if the type should be MAT2 instead
        case 9: accessor.type = TINYGLTF_TYPE_MAT3; break;
        case 16: accessor.type = TINYGLTF_TYPE_MAT4; break;
        default:
            logger().warn(
                "Skipping attribute `{}`: unsupported number of channels {}",
                name,
                attr.get_num_channels());
            return;
        }

        std::string name_uppercase = to_upper(std::string(name));
        if (attr.get_usage() == AttributeUsage::Normal) {
            if (found_normal) {
                name_uppercase = "_" + name_uppercase;
                logger().warn(
                    "Found multiple attributes for normal, saving `{}` as `{}`.",
                    name,
                    name_uppercase);
            } else {
                found_normal = true;
                name_uppercase = "NORMAL";
            }
        } else if (attr.get_usage() == AttributeUsage::Tangent) {
            if (!found_tangent && accessor.type == TINYGLTF_TYPE_VEC4) {
                // this is good!
                found_tangent = true;
                name_uppercase = "TANGENT";
            } else if (accessor.type != TINYGLTF_TYPE_VEC4) {
                name_uppercase = "_" + name_uppercase;
                logger().warn(
                    "gltf TANGENT attribute must be in vec4, saving `{}` as `{}`.",
                    name,
                    name_uppercase);
            } else {
                name_uppercase = "_" + name_uppercase;
                logger().warn(
                    "Found multiple attributes for tangent, saving `{}` as `{}`.",
                    name,
                    name_uppercase);
            }
        } else if (attr.get_usage() == AttributeUsage::Color) {
            name_uppercase = "COLOR_" + std::to_string(color_count);
            ++color_count;
        } else if (attr.get_usage() == AttributeUsage::UV) {
            name_uppercase = "TEXCOORD_" + std::to_string(texcoord_count);
            ++texcoord_count;
        } else {
            // If no previous match, save with the current attribute name.
            // Note that the gltf format is quite strict on the allowed names:
            // POSITION, NORMAL, TANGENT, TEXCOORD_n, COLOR_n, JOINTS_n, and WEIGHTS_n.
            // Custom attributes are allowed ONLY if they start with a leading understore,
            // e.g. _TEMPERATURE. Custom attribute MUST NOT use unsigned int type.
            // https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#meshes

            if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                logger().warn(
                    "gltf mesh attributes cannot use UNSIGNED_INT component type. Skipping "
                    "attribute `{}`",
                    name);
                return;
            }

            if (name_uppercase[0] != '_') {
                name_uppercase = "_" + name_uppercase;
                logger().warn("Saving attribute `{}` as `{}`.", name, name_uppercase);
            }
        }

        const auto& values = [&]() -> const auto&
        {
            if constexpr (AttributeType::IsIndexed) {
                return attr.values();
            } else {
                return attr;
            }
        }
        ();

        // we are committed to writing the buffer here. Do not return early after this line.

        int buffer_index;
        size_t byte_offset, byte_length;
        if constexpr (std::is_same_v<ValueType, double>) {
            std::vector<float> tmp;
            span<const float> data = get_attribute_as<ValueType, float>(values.get_all(), tmp);
            std::tie(buffer_index, byte_offset, byte_length) = write_to_buffer<float>(model, data);
        } else if constexpr (
            std::is_same_v<ValueType, int> || std::is_same_v<ValueType, int32_t> ||
            std::is_same_v<ValueType, int64_t> || std::is_same_v<ValueType, uint64_t> ||
            std::is_same_v<ValueType, size_t>) {
            std::vector<uint32_t> tmp;
            span<const uint32_t> data =
                get_attribute_as<ValueType, uint32_t>(values.get_all(), tmp);
            std::tie(buffer_index, byte_offset, byte_length) =
                write_to_buffer<uint32_t>(model, data);
        } else {
            std::vector<ValueType> tmp;
            span<const ValueType> data =
                get_attribute_as<ValueType, ValueType>(values.get_all(), tmp);
            std::tie(buffer_index, byte_offset, byte_length) =
                write_to_buffer<ValueType>(model, data);
        }

        tinygltf::BufferView buffer_view;
        buffer_view.buffer = buffer_index;
        buffer_view.byteLength = byte_length;
        buffer_view.byteOffset = byte_offset;
        buffer_view.target = TINYGLTF_TARGET_ARRAY_BUFFER;
        int buffer_view_index = int(model.bufferViews.size());
        model.bufferViews.push_back(buffer_view);

        accessor.bufferView = buffer_view_index;
        accessor.count = values.get_num_elements();
        int accessor_index = int(model.accessors.size());
        model.accessors.push_back(accessor);

        primitive.attributes[name_uppercase] = accessor_index;
    });
}

template <typename Scalar, typename Index>
tinygltf::Primitive create_gltf_primitive(
    tinygltf::Model& model,
    const SurfaceMesh<Scalar, Index>& lmesh,
    const SaveOptions& options)
{
    // Handle index attribute conversion if necessary.
    if (span<const AttributeId> attr_ids{
            options.selected_attributes.data(),
            options.selected_attributes.size()};
        options.attribute_conversion_policy ==
            SaveOptions::AttributeConversionPolicy::ConvertAsNeeded &&
        involve_indexed_attribute(lmesh, attr_ids)) {
        auto [mesh2, attr_ids_2] = remap_indexed_attributes(lmesh, attr_ids);

        SaveOptions options2 = options;
        options2.attribute_conversion_policy =
            SaveOptions::AttributeConversionPolicy::ExactMatchOnly;
        options2.selected_attributes.swap(attr_ids_2);
        return create_gltf_primitive(model, mesh2, options2);
    }

    SurfaceMesh<Scalar, Index> lmesh_copy = lmesh;
    scene::utils::convert_texcoord_uv_st(lmesh_copy);

    tinygltf::Primitive primitive;
    primitive.mode = TINYGLTF_MODE_TRIANGLES;
    primitive.material = 0;

    populate_vertices(model, primitive, lmesh_copy);
    populate_facets(model, primitive, lmesh_copy);
    populate_attributes(model, primitive, lmesh_copy, options);

    return primitive;
}

template <typename Scalar, typename Index>
tinygltf::Mesh create_gltf_mesh(
    tinygltf::Model& model,
    const SurfaceMesh<Scalar, Index>& lmesh,
    const SaveOptions& options)
{
    tinygltf::Mesh mesh;
    mesh.primitives.push_back(create_gltf_primitive(model, lmesh, options));
    return mesh;
}

} // namespace

// =====================================
// save_mesh_gltf.h
// =====================================
template <typename Scalar, typename Index>
void save_mesh_gltf(
    const fs::path& filename,
    const SurfaceMesh<Scalar, Index>& mesh,
    const SaveOptions& options)
{
    scene::SimpleScene<Scalar, Index> scene;
    using AffineTransform = typename decltype(scene)::AffineTransform;

    AffineTransform t = AffineTransform::Identity();
    scene.add_instance({scene.add_mesh(mesh), t});

    save_simple_scene_gltf<Scalar, Index>(filename, scene, options);
}

template <typename Scalar, typename Index>
void save_mesh_gltf(
    std::ostream& output_stream,
    const SurfaceMesh<Scalar, Index>& mesh,
    const SaveOptions& options)
{
    scene::SimpleScene<Scalar, Index> scene;
    using AffineTransform = typename decltype(scene)::AffineTransform;

    AffineTransform t = AffineTransform::Identity();
    scene.add_instance({scene.add_mesh(mesh), t});

    save_simple_scene_gltf<Scalar, Index>(output_stream, scene, options);
}

#define LA_X_save_mesh_gltf(_, S, I)        \
    template LA_IO_API void save_mesh_gltf( \
        const fs::path& filename,           \
        const SurfaceMesh<S, I>& mesh,      \
        const SaveOptions& options);        \
    template LA_IO_API void save_mesh_gltf( \
        std::ostream&,                      \
        const SurfaceMesh<S, I>& mesh,      \
        const SaveOptions& options);
LA_SURFACE_MESH_X(save_mesh_gltf, 0)
#undef LA_X_save_mesh_gltf

// =====================================
// save_simple_scene_gltf.h
// =====================================

template <typename Scalar, typename Index, size_t Dimension>
tinygltf::Model scene2model(
    const scene::SimpleScene<Scalar, Index, Dimension>& lscene,
    const SaveOptions& options)
{
    // https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html

    tinygltf::Model model;
    model.asset.generator = "Lagrange";
    model.asset.version = "2.0";

    model.scenes.push_back(tinygltf::Scene());
    model.defaultScene = 0;
    tinygltf::Scene& scene = model.scenes.front();

    // gltf requires a material, so we create a dummy one
    model.materials.push_back(tinygltf::Material());

    for (Index i = 0; i < lscene.get_num_meshes(); ++i) {
        const auto& lmesh = lscene.get_mesh(i);

        // Skip empty meshes and meshes with no instances.
        if (lmesh.get_num_vertices() == 0) continue;
        if (lscene.get_num_instances(i) == 0) continue;

        la_runtime_assert(lmesh.is_triangle_mesh()); // only support triangles
        model.meshes.push_back(create_gltf_mesh(model, lmesh, options));

        for (Index j = 0; j < lscene.get_num_instances(i); ++j) {
            const auto& instance = lscene.get_instance(i, j);

            tinygltf::Node node;
            node.mesh = int(i);
            if constexpr (Dimension == 3) {
                if (!instance.transform.matrix().isIdentity()) {
                    node.matrix = std::vector<double>(
                        instance.transform.data(),
                        instance.transform.data() + 16);
                }
            } else {
                // TODO: convert 2d transforms to 3d
                logger().warn("Ignoring 2d instance transforms while saving gltf scene");
            }

            model.nodes.push_back(node);
            scene.nodes.push_back(int(model.nodes.size()) - 1);
        }
    }

    return model;
}

template <typename Scalar, typename Index, size_t Dimension>
void save_simple_scene_gltf(
    const fs::path& filename,
    const scene::SimpleScene<Scalar, Index, Dimension>& lscene,
    const SaveOptions& options)
{
    auto model = scene2model(lscene, options);
    save_gltf(filename, model, options);
}

template <typename Scalar, typename Index, size_t Dimension>
void save_simple_scene_gltf(
    std::ostream& output_stream,
    const scene::SimpleScene<Scalar, Index, Dimension>& lscene,
    const SaveOptions& options)
{
    auto model = scene2model(lscene, options);
    save_gltf(output_stream, model, options);
}

#define LA_X_save_simple_scene_gltf(_, S, I, D)     \
    template LA_IO_API void save_simple_scene_gltf( \
        const fs::path& filename,                   \
        const scene::SimpleScene<S, I, D>& scene,   \
        const SaveOptions& options);                \
    template LA_IO_API void save_simple_scene_gltf( \
        std::ostream&,                              \
        const scene::SimpleScene<S, I, D>& scene,   \
        const SaveOptions& options);
LA_SIMPLE_SCENE_X(save_simple_scene_gltf, 0);
#undef LA_X_save_simple_scene_gltf


// =====================================
// save_scene_gltf.h
// =====================================

template <typename Scalar, typename Index>
tinygltf::Model lagrange_scene_to_gltf_model(
    const scene::Scene<Scalar, Index>& lscene,
    const SaveOptions& options)
{
    // https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html

    tinygltf::Model model;
    model.asset.generator = "Lagrange";
    model.asset.version = "2.0";

    model.scenes.push_back(tinygltf::Scene());
    model.defaultScene = 0;
    tinygltf::Scene& scene = model.scenes.front();
    scene.name = lscene.name;

    if (!lscene.extensions.empty()) {
        scene.extensions = convert_extension_map(lscene.extensions, options);
    }

    for (const auto& llight : lscene.lights) {
        // note that the gltf support for lights is limited compared to our representation.
        // Information can be lost.
        tinygltf::Light light;
        light.name = llight.name;
        light.color = {
            llight.color_diffuse.x(),
            llight.color_diffuse.y(),
            llight.color_diffuse.z()};
        light.intensity = 1 / llight.attenuation_constant;
        switch (llight.type) {
        case scene::Light::Type::Directional: light.type = "directional"; break;
        case scene::Light::Type::Point: light.type = "point"; break;
        case scene::Light::Type::Spot:
            light.type = "spot";
            light.spot.innerConeAngle = llight.angle_inner_cone;
            light.spot.outerConeAngle = llight.angle_outer_cone;
            break;
        default:
            logger().warn("unsupported light type in GLTF format");
            light.type = "point";
            break;
        }

        if (!llight.extensions.empty()) {
            light.extensions = convert_extension_map(llight.extensions, options);
        }

        model.lights.push_back(light);
    }

    for (const auto& lcam : lscene.cameras) {
        tinygltf::Camera camera;
        camera.name = lcam.name;
        switch (lcam.type) {
        case scene::Camera::Type::Perspective:
            camera.type = "perspective";
            camera.perspective.aspectRatio = lcam.aspect_ratio;
            camera.perspective.yfov = lcam.get_vertical_fov();
            camera.perspective.znear = lcam.near_plane;
            camera.perspective.zfar = lcam.far_plane;
            break;
        case scene::Camera::Type::Orthographic:
            camera.type = "orthographic";
            camera.orthographic.xmag = lcam.orthographic_width;
            camera.orthographic.ymag = lcam.orthographic_width / lcam.aspect_ratio;
            camera.orthographic.znear = lcam.near_plane;
            camera.orthographic.zfar = lcam.far_plane;
            break;
        }
        // TODO: camera may have a position/up/lookAt. This is not allowed in gltf and will be lost.

        if (!lcam.extensions.empty()) {
            camera.extensions = convert_extension_map(lcam.extensions, options);
        }

        model.cameras.push_back(camera);
    }

    for (const auto& limage : lscene.images) {
        tinygltf::Image image;
        image.name = limage.name;

        const scene::ImageBufferExperimental& lbuffer = limage.image;
        image.width = static_cast<int>(lbuffer.width);
        image.height = static_cast<int>(lbuffer.height);
        image.component = static_cast<int>(lbuffer.num_channels);
        switch (lbuffer.element_type) {
        case AttributeValueType::e_uint8_t:
            image.pixel_type = TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;
            break;
        case AttributeValueType::e_int8_t: image.pixel_type = TINYGLTF_COMPONENT_TYPE_BYTE; break;
        case AttributeValueType::e_uint16_t:
            image.pixel_type = TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT;
            break;
        case AttributeValueType::e_int16_t: image.pixel_type = TINYGLTF_COMPONENT_TYPE_SHORT; break;
        case AttributeValueType::e_uint32_t:
            image.pixel_type = TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT;
            break;
        case AttributeValueType::e_int32_t: image.pixel_type = TINYGLTF_COMPONENT_TYPE_INT; break;
        case AttributeValueType::e_float: image.pixel_type = TINYGLTF_COMPONENT_TYPE_FLOAT; break;
        case AttributeValueType::e_double: image.pixel_type = TINYGLTF_COMPONENT_TYPE_DOUBLE; break;
        default:
            logger().error("Saving image with unsupported pixel precision!");
            // TODO: should we simply fail?
            image.pixel_type = TINYGLTF_COMPONENT_TYPE_BYTE;
        }
        image.bits = static_cast<int>(lbuffer.get_bits_per_element());
        std::copy(lbuffer.data.begin(), lbuffer.data.end(), std::back_inserter(image.image));

        if (!limage.uri.empty()) {
            image.uri = limage.uri.string();
        } else {
            image.mimeType = "image/png";
        }

        if (!limage.extensions.empty()) {
            image.extensions = convert_extension_map(limage.extensions, options);
        }

        model.images.push_back(image);
    }

    auto element_id_to_int = [](scene::ElementId id) -> int {
        if (id == lagrange::invalid<scene::ElementId>()) {
            return -1;
        } else {
            return static_cast<int>(id);
        }
    };
    for (const auto& lmat : lscene.materials) {
        tinygltf::Material material;
        material.name = lmat.name;
        material.doubleSided = lmat.double_sided;

        material.pbrMetallicRoughness.baseColorFactor = to_vec4(lmat.base_color_value);
        material.pbrMetallicRoughness.baseColorTexture.index =
            element_id_to_int(lmat.base_color_texture.index);
        material.pbrMetallicRoughness.baseColorTexture.texCoord = lmat.base_color_texture.texcoord;

        material.emissiveFactor = to_vec3(lmat.emissive_value);
        material.emissiveTexture.index = element_id_to_int(lmat.emissive_texture.index);
        material.emissiveTexture.texCoord = lmat.emissive_texture.texcoord;

        material.pbrMetallicRoughness.metallicFactor = lmat.metallic_value;
        material.pbrMetallicRoughness.roughnessFactor = lmat.roughness_value;
        material.pbrMetallicRoughness.metallicRoughnessTexture.index =
            element_id_to_int(lmat.metallic_roughness_texture.index);
        material.pbrMetallicRoughness.metallicRoughnessTexture.texCoord =
            lmat.metallic_roughness_texture.texcoord;

        material.normalTexture.index = element_id_to_int(lmat.normal_texture.index);
        material.normalTexture.texCoord = lmat.normal_texture.texcoord;
        material.normalTexture.scale = lmat.normal_scale;

        material.occlusionTexture.index = element_id_to_int(lmat.occlusion_texture.index);
        material.occlusionTexture.texCoord = lmat.occlusion_texture.texcoord;
        material.occlusionTexture.strength = lmat.occlusion_strength;

        material.alphaCutoff = lmat.alpha_cutoff;
        material.alphaMode = [](scene::MaterialExperimental::AlphaMode mode) {
            switch (mode) {
            case scene::MaterialExperimental::AlphaMode::Opaque: return "OPAQUE";
            case scene::MaterialExperimental::AlphaMode::Mask: return "MASK";
            case scene::MaterialExperimental::AlphaMode::Blend: return "BLEND";
            default: logger().warn("Invalid alpha mode"); return "";
            }
        }(lmat.alpha_mode);

        if (!lmat.extensions.empty()) {
            material.extensions = convert_extension_map(lmat.extensions, options);
        }

        model.materials.push_back(material);
    }

    for (const auto& ltex : lscene.textures) {
        la_debug_assert(ltex.image != invalid<scene::ElementId>());
        tinygltf::Texture texture;
        texture.name = ltex.name;
        texture.source = static_cast<int>(ltex.image);

        if (!ltex.extensions.empty()) {
            texture.extensions = convert_extension_map(ltex.extensions, options);
        }

        model.textures.push_back(texture);
    }

    // TODO skeletons

    // TODO animations

    std::function<int(const scene::Node&)> visit_node;
    visit_node = [&](const scene::Node& lnode) -> int {
        tinygltf::Node node;
        node.name = lnode.name;

        if (!lnode.cameras.empty()) {
            node.camera = lagrange::safe_cast<int>(lnode.cameras.front());
            if (lnode.cameras.size() > 1) {
                logger().warn("GLTF format only supports one camera per node");
            }
        }

        if (!lnode.meshes.empty()) {
            // we treat multiple meshes in one lagrange node as one gltf mesh with multiple
            // primitives. they must reference exactly one material.
            tinygltf::Mesh mesh;
            for (const auto& mesh_instance : lnode.meshes) {
                const auto& lmesh = lscene.meshes[mesh_instance.mesh];
                tinygltf::Primitive prim = create_gltf_primitive(model, lmesh, options);
                la_runtime_assert(mesh_instance.materials.size() == 1);
                prim.material = lagrange::safe_cast<int>(mesh_instance.materials.front());
                mesh.primitives.push_back(prim);
            }
            int mesh_idx = lagrange::safe_cast<int>(model.meshes.size());
            model.meshes.push_back(mesh);
            node.mesh = mesh_idx;
        }

        if (!lnode.extensions.empty()) {
            node.extensions = convert_extension_map(lnode.extensions, options);
        }

        int node_idx = lagrange::safe_cast<int>(model.nodes.size());
        model.nodes.push_back(node);

        for (const size_t lagrange_child_idx : lnode.children) {
            int child_idx = visit_node(lscene.nodes[lagrange_child_idx]);
            model.nodes[node_idx].children.push_back(child_idx);
        }

        return node_idx;
    };
    for (const scene::Node& lnode : lscene.nodes) {
        int lnode_idx = visit_node(lnode);
        scene.nodes.push_back(lnode_idx);
    }

    return model;
}

template <typename Scalar, typename Index>
void save_scene_gltf(
    const fs::path& filename,
    const scene::Scene<Scalar, Index>& lscene,
    const SaveOptions& options)
{
    auto model = lagrange_scene_to_gltf_model<Scalar, Index>(lscene, options);
    save_gltf(filename, model, options);
}
template <typename Scalar, typename Index>
void save_scene_gltf(
    std::ostream& output_stream,
    const scene::Scene<Scalar, Index>& lscene,
    const SaveOptions& options)
{
    auto model = lagrange_scene_to_gltf_model<Scalar, Index>(lscene, options);
    save_gltf(output_stream, model, options);
}

#define LA_X_save_scene_gltf(_, S, I)        \
    template LA_IO_API void save_scene_gltf( \
        const fs::path& filename,            \
        const scene::Scene<S, I>& scene,     \
        const SaveOptions& options);         \
    template LA_IO_API void save_scene_gltf( \
        std::ostream&,                       \
        const scene::Scene<S, I>& scene,     \
        const SaveOptions& options);
LA_SCENE_X(save_scene_gltf, 0);
#undef LA_X_save_scene_gltf

} // namespace lagrange::io
