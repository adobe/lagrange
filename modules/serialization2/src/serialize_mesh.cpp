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
#include <lagrange/serialization/serialize_mesh.h>

#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/cast.h>
#include <lagrange/internal/surface_mesh_info_convert.h>
#include <lagrange/scene/scene_convert.h>
#include <lagrange/scene/simple_scene_convert.h>
#include <lagrange/serialization/serialize_scene.h>
#include <lagrange/serialization/serialize_simple_scene.h>
#include <lagrange/utils/assert.h>

#include "CistaMesh.h"
#include "compress.h"
#include "detect_type.h"


namespace lagrange::serialization {

using internal::compress_buffer;
using internal::decompress_buffer;
using internal::is_compressed;
using internal::k_cista_mode;

namespace internal {

namespace {

/// Set up a data::vector as a non-owning view over a span's data.
/// The span's data must outlive the vector. The vector will not free the memory on destruction.
void set_non_owning(data::vector<uint8_t>& dest, lagrange::span<const uint8_t> src)
{
    if (!src.empty()) {
        dest.el_ = const_cast<uint8_t*>(src.data());
        dest.used_size_ = src.size();
        dest.allocated_size_ = src.size();
        dest.self_allocated_ = false;
    }
}

} // namespace

template <typename Scalar, typename Index>
CistaMesh to_cista_mesh(const SurfaceMesh<Scalar, Index>& mesh)
{
    auto info = lagrange::internal::from_surface_mesh(mesh);

    CistaMesh cmesh;
    cmesh.scalar_type_size = info.scalar_type_size;
    cmesh.index_type_size = info.index_type_size;
    cmesh.num_vertices = info.num_vertices;
    cmesh.num_facets = info.num_facets;
    cmesh.num_corners = info.num_corners;
    cmesh.num_edges = info.num_edges;
    cmesh.dimension = info.dimension;
    cmesh.vertex_per_facet = info.vertex_per_facet;

    for (const auto& ai : info.attributes) {
        CistaAttributeInfo cai;
        cai.name = data::string(ai.name.data(), ai.name.size());
        cai.attribute_id = ai.attribute_id;
        cai.value_type = ai.value_type;
        cai.element_type = ai.element_type;
        cai.usage = ai.usage;
        cai.num_channels = ai.num_channels;
        cai.num_elements = ai.num_elements;
        cai.is_indexed = ai.is_indexed;

        if (ai.is_indexed) {
            set_non_owning(cai.values_bytes, ai.values_bytes);
            cai.values_num_elements = ai.values_num_elements;
            cai.values_num_channels = ai.values_num_channels;
            set_non_owning(cai.indices_bytes, ai.indices_bytes);
            cai.indices_num_elements = ai.indices_num_elements;
            cai.index_type_size = ai.index_type_size;
        } else {
            set_non_owning(cai.data_bytes, ai.data_bytes);
        }

        cmesh.attributes.emplace_back(std::move(cai));
    }

    return cmesh;
}

template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> from_cista_mesh(const CistaMesh& cmesh)
{
    la_runtime_assert(
        cmesh.version == mesh_format_version(),
        "Unsupported encoding format version: expected " + std::to_string(mesh_format_version()) +
            ", got " + std::to_string(cmesh.version));

    lagrange::internal::SurfaceMeshInfo info;
    info.scalar_type_size = cmesh.scalar_type_size;
    info.index_type_size = cmesh.index_type_size;
    info.num_vertices = cmesh.num_vertices;
    info.num_facets = cmesh.num_facets;
    info.num_corners = cmesh.num_corners;
    info.num_edges = cmesh.num_edges;
    info.dimension = cmesh.dimension;
    info.vertex_per_facet = cmesh.vertex_per_facet;

    for (const auto& cai : cmesh.attributes) {
        lagrange::internal::AttributeInfo ai;
        ai.name = std::string_view(cai.name.data(), cai.name.size());
        ai.attribute_id = cai.attribute_id;
        ai.value_type = cai.value_type;
        ai.element_type = cai.element_type;
        ai.usage = cai.usage;
        ai.num_channels = cai.num_channels;
        ai.num_elements = cai.num_elements;
        ai.is_indexed = cai.is_indexed;

        if (cai.is_indexed) {
            ai.values_bytes =
                lagrange::span<const uint8_t>(cai.values_bytes.data(), cai.values_bytes.size());
            ai.values_num_elements = cai.values_num_elements;
            ai.values_num_channels = cai.values_num_channels;
            ai.indices_bytes =
                lagrange::span<const uint8_t>(cai.indices_bytes.data(), cai.indices_bytes.size());
            ai.indices_num_elements = cai.indices_num_elements;
            ai.index_type_size = cai.index_type_size;
        } else {
            ai.data_bytes =
                lagrange::span<const uint8_t>(cai.data_bytes.data(), cai.data_bytes.size());
        }

        info.attributes.push_back(std::move(ai));
    }

    return lagrange::internal::to_surface_mesh<Scalar, Index>(info);
}

/// Deserialize a CistaMesh buffer with runtime dispatch on the stored Scalar/Index types, then
/// cast to the requested <ToScalar, ToIndex> types.
template <typename ToScalar, typename ToIndex>
SurfaceMesh<ToScalar, ToIndex> deserialize_mesh_with_cast(span<const uint8_t> buffer)
{
    const auto* cmesh =
        cista::deserialize<CistaMesh, k_cista_mode>(buffer.data(), buffer.data() + buffer.size());
    const uint8_t ss = cmesh->scalar_type_size;
    const uint8_t is = cmesh->index_type_size;

    // Runtime dispatch on stored (scalar_size, index_size) -> load with native types -> cast
    if (ss == sizeof(float) && is == sizeof(uint32_t)) {
        auto mesh = from_cista_mesh<float, uint32_t>(*cmesh);
        return lagrange::cast<ToScalar, ToIndex>(mesh);
    } else if (ss == sizeof(double) && is == sizeof(uint32_t)) {
        auto mesh = from_cista_mesh<double, uint32_t>(*cmesh);
        return lagrange::cast<ToScalar, ToIndex>(mesh);
    } else if (ss == sizeof(float) && is == sizeof(uint64_t)) {
        auto mesh = from_cista_mesh<float, uint64_t>(*cmesh);
        return lagrange::cast<ToScalar, ToIndex>(mesh);
    } else if (ss == sizeof(double) && is == sizeof(uint64_t)) {
        auto mesh = from_cista_mesh<double, uint64_t>(*cmesh);
        return lagrange::cast<ToScalar, ToIndex>(mesh);
    } else {
        throw std::runtime_error(
            "Unsupported scalar/index type sizes: scalar=" + std::to_string(ss) +
            " index=" + std::to_string(is));
    }
}

} // namespace internal

template <typename Scalar, typename Index>
std::vector<uint8_t> serialize_mesh(
    const SurfaceMesh<Scalar, Index>& mesh,
    const SerializeOptions& options)
{
    auto cmesh = internal::to_cista_mesh(mesh);

    cista::buf<std::vector<uint8_t>> buf;
    cista::serialize<k_cista_mode>(buf, cmesh);

    if (options.compress) {
        return compress_buffer(buf.buf_, options.compression_level, options.num_threads);
    }

    return std::move(buf.buf_);
}

template <typename MeshType>
MeshType deserialize_mesh(span<const uint8_t> buffer, const DeserializeOptions& options)
{
    using Scalar = typename MeshType::Scalar;
    using Index = typename MeshType::Index;

    // Decompress if needed
    std::vector<uint8_t> decompressed_storage;
    span<const uint8_t> data = buffer;
    if (is_compressed(buffer)) {
        decompressed_storage = decompress_buffer(buffer);
        data = span<const uint8_t>(decompressed_storage);
    }

    auto load_native_mesh = [&]() -> MeshType {
        const auto* cmesh = cista::deserialize<internal::CistaMesh, k_cista_mode>(
            data.data(),
            data.data() + data.size());
        if (cmesh->scalar_type_size != sizeof(Scalar) || cmesh->index_type_size != sizeof(Index)) {
            if (!options.allow_type_cast) {
                throw std::runtime_error(
                    "Scalar/Index type mismatch: buffer has scalar_size=" +
                    std::to_string(cmesh->scalar_type_size) +
                    " index_size=" + std::to_string(cmesh->index_type_size) +
                    ", expected scalar_size=" + std::to_string(sizeof(Scalar)) +
                    " index_size=" + std::to_string(sizeof(Index)));
            }
            if (!options.quiet) {
                logger().warn(
                    "Casting mesh types: buffer has scalar_size={} index_size={}, "
                    "requested scalar_size={} index_size={}",
                    cmesh->scalar_type_size,
                    cmesh->index_type_size,
                    sizeof(Scalar),
                    sizeof(Index));
            }
            return internal::deserialize_mesh_with_cast<Scalar, Index>(data);
        }
        return internal::from_cista_mesh<Scalar, Index>(*cmesh);
    };

    if (!options.allow_scene_conversion) {
        return load_native_mesh();
    }

    auto type = internal::detect_encoded_type(data);
    switch (type) {
    case internal::EncodedType::Mesh: return load_native_mesh();
    case internal::EncodedType::SimpleScene: {
        if (!options.quiet) {
            logger().warn("Buffer contains a SimpleScene, converting to Mesh");
        }
        DeserializeOptions native_opts;
        native_opts.allow_scene_conversion = false;
        auto scene =
            deserialize_simple_scene<scene::SimpleScene<Scalar, Index, 3>>(data, native_opts);
        return scene::simple_scene_to_mesh(scene);
    }
    case internal::EncodedType::Scene: {
        if (!options.quiet) {
            logger().warn("Buffer contains a Scene, converting to Mesh");
        }
        DeserializeOptions native_opts;
        native_opts.allow_scene_conversion = false;
        auto scene = deserialize_scene<scene::Scene<Scalar, Index>>(data, native_opts);
        return scene::scene_to_mesh(scene);
    }
    default: throw std::runtime_error("Unknown encoded data type in buffer");
    }
}

template <typename Scalar, typename Index>
void save_mesh(
    const fs::path& filename,
    const SurfaceMesh<Scalar, Index>& mesh,
    const SerializeOptions& options)
{
    // TODO: serialize/deserialize directly to a mmap file
    auto buf = serialize_mesh(mesh, options);
    fs::ofstream ofs(filename, std::ios::binary);
    la_runtime_assert(ofs.good(), "Failed to open file for writing: " + filename.string());
    ofs.write(reinterpret_cast<const char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
    la_runtime_assert(ofs.good(), "Failed to write to file: " + filename.string());
}

template <typename MeshType>
MeshType load_mesh(const fs::path& filename, const DeserializeOptions& options)
{
    auto buf = internal::read_file_to_buffer(filename);
    if (is_compressed(buf)) {
        buf = decompress_buffer(buf);
    }
    return deserialize_mesh<MeshType>(lagrange::span<const uint8_t>(buf), options);
}

// Explicit template instantiations
#define LA_X_serialization2(_, Scalar, Index)                                                     \
    template LA_SERIALIZATION2_API std::vector<uint8_t> serialize_mesh<Scalar, Index>(            \
        const SurfaceMesh<Scalar, Index>&,                                                        \
        const SerializeOptions&);                                                                 \
    template LA_SERIALIZATION2_API SurfaceMesh<Scalar, Index>                                     \
    deserialize_mesh<SurfaceMesh<Scalar, Index>>(span<const uint8_t>, const DeserializeOptions&); \
    template LA_SERIALIZATION2_API void save_mesh<Scalar, Index>(                                 \
        const fs::path&,                                                                          \
        const SurfaceMesh<Scalar, Index>&,                                                        \
        const SerializeOptions&);                                                                 \
    template LA_SERIALIZATION2_API SurfaceMesh<Scalar, Index>                                     \
    load_mesh<SurfaceMesh<Scalar, Index>>(const fs::path&, const DeserializeOptions&);
LA_SURFACE_MESH_X(serialization2, 0)
#undef LA_X_serialization2

} // namespace lagrange::serialization
