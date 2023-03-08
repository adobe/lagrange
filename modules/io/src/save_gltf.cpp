/*
 * Copyright 2022 Adobe. All rights reserved.
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
#include <lagrange/io/save_mesh_gltf.h>
#include <lagrange/io/save_simple_scene_gltf.h>
// ====

#include <lagrange/Attribute.h>
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/foreach_attribute.h>
#include <lagrange/internal/string_from_scalar.h>
#include <lagrange/scene/SimpleSceneTypes.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/strings.h>
#include <lagrange/views.h>

#include "internal/convert_attribute_utils.h"

#include <tiny_gltf.h>

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
void save_gltf(const fs::path& filename, const tinygltf::Model& model, const SaveOptions& options)
{
    bool binary = to_lower(filename.extension().string()) == ".glb";
    if (binary && options.encoding != FileEncoding::Binary) {
        logger().warn("Saving mesh in binary due to `.glb` extension.");
    } else if (!binary && options.encoding != FileEncoding::Ascii) {
        logger().warn("Saving mesh in ascii due to `.gltf` extension.");
    }

    tinygltf::TinyGLTF loader;
    constexpr bool embed_images = false;
    constexpr bool embed_buffers = true;
    constexpr bool pretty_print = true;
    loader.WriteGltfSceneToFile(
        &model,
        filename.string(),
        embed_images,
        embed_buffers,
        pretty_print,
        binary);
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
        reinterpret_cast<std::byte *>(buffer.data.data()) + byte_offset);

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
                // This should be the case if you call `unify_index_buffers`
                logger().warn("Skipping attribute {}: unsupported non-indexed", name);
                return;
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


        tinygltf::Accessor accessor;
        if constexpr (std::is_same_v<ValueType, char>) {
            accessor.componentType = TINYGLTF_COMPONENT_TYPE_BYTE;
        } else if constexpr (std::is_same_v<ValueType, unsigned char>) {
            accessor.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;
        } else if constexpr (std::is_same_v<ValueType, uint16_t>) {
            accessor.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT;
        } else if constexpr (
            std::is_same_v<ValueType, int> || std::is_same_v<ValueType, int32_t> ||
            std::is_same_v<ValueType, int64_t>) {
            accessor.componentType = TINYGLTF_COMPONENT_TYPE_INT;
        } else if constexpr (
            std::is_same_v<ValueType, unsigned int> || std::is_same_v<ValueType, uint32_t> ||
            std::is_same_v<ValueType, uint64_t> || std::is_same_v<ValueType, size_t>) {
            accessor.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT;
        } else if constexpr (std::is_same_v<ValueType, float>) {
            accessor.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
        } else if constexpr (std::is_same_v<ValueType, double>) {
            // special case: convert double to float
            accessor.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
        } else {
            logger().warn(
                "Skipping attribute {}: unsupported type {}",
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
                "Skipping attribute {}: unsupported number of channels {}",
                name,
                attr.get_num_channels());
            return;
        }

        int buffer_index;
        size_t byte_offset, byte_length;
        if constexpr (std::is_same_v<ValueType, double>) {
            std::vector<float> tmp;
            span<const float> data = get_attribute_as<ValueType, float>(values.get_all(), tmp);
            std::tie(buffer_index, byte_offset, byte_length) = write_to_buffer<float>(model, data);
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

        std::string name_uppercase;
        std::transform(name.begin(), name.end(), name_uppercase.begin(), [](unsigned char c) {
            return std::toupper(c);
        });
        if (attr.get_usage() == AttributeUsage::Normal) {
            if (found_normal) {
                name_uppercase = "_" + name_uppercase;
                logger().warn(
                    "Found multiple attributes for normal, saving {} as {}",
                    name,
                    name_uppercase);
            } else {
                found_normal = true;
                name_uppercase = "NORMAL";
            }
        } else if (attr.get_usage() == AttributeUsage::Tangent) {
            if (found_tangent) {
                name_uppercase = "_" + name_uppercase;
                logger().warn(
                    "Found multiple attributes for tangent, saving {} as {}",
                    name,
                    name_uppercase);
            } else {
                found_tangent = true;
                name_uppercase = "TANGENT";
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
            // POSITION, NORMAL, TANGENT< TEXCOORD_n, COLOR_n, JOINTS_n, and WEIGHTS_n.
            // Custom attributes are allowed ONLY if they start with a leading understore,
            // e.g. _TEMPERATURE. Custom attribute MUST NOT use unsigned int type.
            // https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#meshes
        }

        primitive.attributes[name_uppercase] = accessor_index;
    });
}

template <typename Scalar, typename Index>
tinygltf::Mesh create_gltf_mesh(
    tinygltf::Model& model,
    const SurfaceMesh<Scalar, Index>& lmesh,
    const SaveOptions& options)
{
    // Hanlde index attribute conversion if necessary.
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
        return create_gltf_mesh(model, mesh2, options2);
    }

    tinygltf::Mesh mesh;
    mesh.primitives.push_back(tinygltf::Primitive());
    tinygltf::Primitive& primitive = mesh.primitives.front();
    primitive.mode = TINYGLTF_MODE_TRIANGLES;
    primitive.material = 0;

    populate_vertices(model, primitive, lmesh);
    populate_facets(model, primitive, lmesh);
    populate_attributes(model, primitive, lmesh, options);

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

#define LA_X_save_mesh_gltf(_, S, I)   \
    template void save_mesh_gltf(      \
        const fs::path& filename,      \
        const SurfaceMesh<S, I>& mesh, \
        const SaveOptions& options);
LA_SURFACE_MESH_X(save_mesh_gltf, 0)
#undef LA_X_save_mesh_gltf

// =====================================
// save_simple_scene_gltf.h
// =====================================
template <typename Scalar, typename Index, size_t Dimension>
void save_simple_scene_gltf(
    const fs::path& filename,
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
        la_runtime_assert(lmesh.get_vertex_per_facet() == 3); // only support triangles
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

    save_gltf(filename, model, options);
}

#define LA_X_save_simple_scene_gltf(_, S, I, D)   \
    template void save_simple_scene_gltf(         \
        const fs::path& filename,                 \
        const scene::SimpleScene<S, I, D>& scene, \
        const SaveOptions& options);
LA_SIMPLE_SCENE_X(save_simple_scene_gltf, 0);
#undef LA_X_save_simple_scene_gltf

} // namespace lagrange::io
