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
#include <lagrange/serialization/serialize_simple_scene.h>

#include <lagrange/Logger.h>
#include <lagrange/cast.h>
#include <lagrange/scene/SimpleSceneTypes.h>
#include <lagrange/scene/scene_convert.h>
#include <lagrange/scene/simple_scene_convert.h>
#include <lagrange/serialization/serialize_mesh.h>
#include <lagrange/serialization/serialize_scene.h>
#include <lagrange/utils/assert.h>

#include "CistaSimpleScene.h"
#include "compress.h"
#include "detect_type.h"
#include "mesh_convert.h"

#include <cstring>

namespace lagrange::serialization {

using internal::compress_buffer;
using internal::decompress_buffer;
using internal::is_compressed;
using internal::k_cista_mode;

namespace internal {

template <typename Scalar, typename Index, size_t Dimension>
CistaSimpleScene to_cista_simple_scene(const scene::SimpleScene<Scalar, Index, Dimension>& scene)
{
    CistaSimpleScene cscene;
    cscene.scalar_type_size = sizeof(Scalar);
    cscene.index_type_size = sizeof(Index);
    cscene.dimension = static_cast<uint8_t>(Dimension);

    const Index num_meshes = scene.get_num_meshes();

    // Serialize meshes
    cscene.meshes.resize(num_meshes);
    for (Index i = 0; i < num_meshes; ++i) {
        cscene.meshes[i] = to_cista_mesh(scene.get_mesh(i));
    }

    // Serialize instances (flattened with per-mesh counts)
    cscene.instances_per_mesh.resize(num_meshes);
    for (Index i = 0; i < num_meshes; ++i) {
        const Index num_instances = scene.get_num_instances(i);
        cscene.instances_per_mesh[i] = num_instances;

        for (Index j = 0; j < num_instances; ++j) {
            const auto& inst = scene.get_instance(i, j);

            CistaInstance cinst;
            cinst.mesh_index = inst.mesh_index;

            // Store transform as raw bytes
            constexpr size_t matrix_size = (Dimension + 1) * (Dimension + 1);
            constexpr size_t byte_size = matrix_size * sizeof(Scalar);
            cinst.transform_bytes.resize(byte_size);
            std::memcpy(cinst.transform_bytes.data(), inst.transform.matrix().data(), byte_size);

            cscene.instances.emplace_back(std::move(cinst));
        }
    }

    return cscene;
}

template <typename Scalar, typename Index, size_t Dimension>
scene::SimpleScene<Scalar, Index, Dimension> from_cista_simple_scene(const CistaSimpleScene& cscene)
{
    la_runtime_assert(
        cscene.version == simple_scene_format_version(),
        fmt::format(
            "Unsupported encoding format version: expected {}, got {}",
            simple_scene_format_version(),
            cscene.version));
    la_runtime_assert(
        cscene.scalar_type_size == sizeof(Scalar),
        fmt::format(
            "Scalar type size mismatch: expected {}, got {}",
            sizeof(Scalar),
            cscene.scalar_type_size));
    la_runtime_assert(
        cscene.index_type_size == sizeof(Index),
        fmt::format(
            "Index type size mismatch: expected {}, got {}",
            sizeof(Index),
            cscene.index_type_size));
    la_runtime_assert(
        cscene.dimension == Dimension,
        fmt::format("Dimension mismatch: expected {}, got {}", Dimension, cscene.dimension));

    scene::SimpleScene<Scalar, Index, Dimension> scene;

    const size_t num_meshes = cscene.meshes.size();
    la_runtime_assert(
        cscene.instances_per_mesh.size() == num_meshes,
        fmt::format(
            "instances_per_mesh size mismatch: expected {}, got {}",
            num_meshes,
            cscene.instances_per_mesh.size()));
    scene.reserve_meshes(static_cast<Index>(num_meshes));

    for (size_t i = 0; i < num_meshes; ++i) {
        scene.add_mesh(from_cista_mesh<Scalar, Index>(cscene.meshes[i]));
    }

    // Reconstruct instances from flattened data
    size_t instance_offset = 0;
    for (size_t i = 0; i < num_meshes; ++i) {
        const size_t num_instances = static_cast<size_t>(cscene.instances_per_mesh[i]);
        la_runtime_assert(
            instance_offset + num_instances <= cscene.instances.size(),
            fmt::format(
                "Instance offset out of bounds: offset={} + count={} > total={}",
                instance_offset,
                num_instances,
                cscene.instances.size()));
        scene.reserve_instances(static_cast<Index>(i), static_cast<Index>(num_instances));

        for (size_t j = 0; j < num_instances; ++j) {
            const auto& cinst = cscene.instances[instance_offset + j];

            using InstanceType = scene::MeshInstance<Scalar, Index, Dimension>;
            InstanceType inst;
            inst.mesh_index = static_cast<Index>(cinst.mesh_index);

            // Restore transform from raw bytes
            constexpr size_t matrix_size = (Dimension + 1) * (Dimension + 1);
            constexpr size_t byte_size = matrix_size * sizeof(Scalar);
            la_runtime_assert(
                cinst.transform_bytes.size() == byte_size,
                fmt::format(
                    "Transform data size mismatch: expected {}, got {}",
                    byte_size,
                    cinst.transform_bytes.size()));
            std::memcpy(inst.transform.matrix().data(), cinst.transform_bytes.data(), byte_size);

            scene.add_instance(std::move(inst));
        }
        instance_offset += num_instances;
    }
    la_runtime_assert(
        instance_offset == cscene.instances.size(),
        fmt::format(
            "Total instance count mismatch: expected {}, got {}",
            cscene.instances.size(),
            instance_offset));

    return scene;
}

/// Deserialize a CistaSimpleScene buffer with runtime dispatch on stored Scalar/Index types, then
/// cast meshes and transforms to the requested <ToScalar, ToIndex> types.
template <typename ToScalar, typename ToIndex, size_t Dimension>
scene::SimpleScene<ToScalar, ToIndex, Dimension> deserialize_simple_scene_with_cast(
    span<const uint8_t> buffer)
{
    const auto* cscene = cista::deserialize<CistaSimpleScene, k_cista_mode>(
        buffer.data(),
        buffer.data() + buffer.size());

    la_runtime_assert(
        cscene->version == simple_scene_format_version(),
        fmt::format(
            "Unsupported encoding format version: expected {}, got {}",
            simple_scene_format_version(),
            cscene->version));
    la_runtime_assert(
        cscene->dimension == Dimension,
        fmt::format("Dimension mismatch: expected {}, got {}", Dimension, cscene->dimension));

    const uint8_t ss = cscene->scalar_type_size;
    const uint8_t is = cscene->index_type_size;

    // Helper to deserialize and cast a single mesh
    auto cast_one_mesh = [&](const CistaMesh& cm) -> SurfaceMesh<ToScalar, ToIndex> {
        if (ss == sizeof(float) && is == sizeof(uint32_t)) {
            auto m = from_cista_mesh<float, uint32_t>(cm);
            return lagrange::cast<ToScalar, ToIndex>(m);
        } else if (ss == sizeof(double) && is == sizeof(uint32_t)) {
            auto m = from_cista_mesh<double, uint32_t>(cm);
            return lagrange::cast<ToScalar, ToIndex>(m);
        } else if (ss == sizeof(float) && is == sizeof(uint64_t)) {
            auto m = from_cista_mesh<float, uint64_t>(cm);
            return lagrange::cast<ToScalar, ToIndex>(m);
        } else if (ss == sizeof(double) && is == sizeof(uint64_t)) {
            auto m = from_cista_mesh<double, uint64_t>(cm);
            return lagrange::cast<ToScalar, ToIndex>(m);
        } else {
            throw std::runtime_error(
                fmt::format("Unsupported scalar/index type sizes: scalar={} index={}", ss, is));
        }
    };

    scene::SimpleScene<ToScalar, ToIndex, Dimension> result;

    const size_t num_meshes = cscene->meshes.size();
    la_runtime_assert(
        cscene->instances_per_mesh.size() == num_meshes,
        fmt::format(
            "instances_per_mesh size mismatch: expected {}, got {}",
            num_meshes,
            cscene->instances_per_mesh.size()));
    result.reserve_meshes(static_cast<ToIndex>(num_meshes));

    for (size_t i = 0; i < num_meshes; ++i) {
        result.add_mesh(cast_one_mesh(cscene->meshes[i]));
    }

    // Reconstruct instances with transform casting
    constexpr size_t matrix_size = (Dimension + 1) * (Dimension + 1);
    size_t instance_offset = 0;
    for (size_t i = 0; i < num_meshes; ++i) {
        const size_t num_instances = static_cast<size_t>(cscene->instances_per_mesh[i]);
        la_runtime_assert(
            instance_offset + num_instances <= cscene->instances.size(),
            fmt::format(
                "Instance offset out of bounds: offset={} + count={} > total={}",
                instance_offset,
                num_instances,
                cscene->instances.size()));
        result.reserve_instances(static_cast<ToIndex>(i), static_cast<ToIndex>(num_instances));

        for (size_t j = 0; j < num_instances; ++j) {
            const auto& cinst = cscene->instances[instance_offset + j];

            using InstanceType = scene::MeshInstance<ToScalar, ToIndex, Dimension>;
            InstanceType inst;
            inst.mesh_index = static_cast<ToIndex>(cinst.mesh_index);

            // Cast transform from native scalar type
            const size_t expected_byte_size = matrix_size * ss;
            la_runtime_assert(
                cinst.transform_bytes.size() == expected_byte_size,
                fmt::format(
                    "Transform data size mismatch: expected {}, got {}",
                    expected_byte_size,
                    cinst.transform_bytes.size()));
            if (ss == sizeof(float)) {
                float native[matrix_size];
                std::memcpy(native, cinst.transform_bytes.data(), matrix_size * sizeof(float));
                for (size_t k = 0; k < matrix_size; ++k) {
                    inst.transform.matrix().data()[k] = static_cast<ToScalar>(native[k]);
                }
            } else if (ss == sizeof(double)) {
                double native[matrix_size];
                std::memcpy(native, cinst.transform_bytes.data(), matrix_size * sizeof(double));
                for (size_t k = 0; k < matrix_size; ++k) {
                    inst.transform.matrix().data()[k] = static_cast<ToScalar>(native[k]);
                }
            } else {
                throw std::runtime_error(fmt::format("Unsupported scalar type size: {}", ss));
            }

            result.add_instance(std::move(inst));
        }
        instance_offset += num_instances;
    }
    la_runtime_assert(
        instance_offset == cscene->instances.size(),
        fmt::format(
            "Total instance count mismatch: expected {}, got {}",
            cscene->instances.size(),
            instance_offset));

    return result;
}

} // namespace internal

template <typename Scalar, typename Index, size_t Dimension>
std::vector<uint8_t> serialize_simple_scene(
    const scene::SimpleScene<Scalar, Index, Dimension>& scene,
    const SerializeOptions& options)
{
    auto cscene = internal::to_cista_simple_scene(scene);

    cista::buf<std::vector<uint8_t>> buf;
    cista::serialize<k_cista_mode>(buf, cscene);

    if (options.compress) {
        return compress_buffer(buf.buf_, options.compression_level, options.num_threads);
    }

    return std::move(buf.buf_);
}

template <typename SceneType>
SceneType deserialize_simple_scene(span<const uint8_t> buffer, const DeserializeOptions& options)
{
    using Scalar = typename SceneType::MeshType::Scalar;
    using Index = typename SceneType::MeshType::Index;
    constexpr size_t Dimension = SceneType::Dim;

    // Decompress if needed
    std::vector<uint8_t> decompressed_storage;
    span<const uint8_t> data = buffer;
    if (is_compressed(buffer)) {
        decompressed_storage = decompress_buffer(buffer);
        data = span<const uint8_t>(decompressed_storage);
    }

    auto load_native_simple_scene = [&]() -> SceneType {
        const auto* cscene = cista::deserialize<internal::CistaSimpleScene, k_cista_mode>(
            data.data(),
            data.data() + data.size());
        if (cscene->scalar_type_size != sizeof(Scalar) ||
            cscene->index_type_size != sizeof(Index)) {
            if (!options.allow_type_cast) {
                throw std::runtime_error(
                    fmt::format(
                        "Scalar/Index type mismatch: buffer has scalar_size={} index_size={}, "
                        "expected scalar_size={} index_size={}",
                        cscene->scalar_type_size,
                        cscene->index_type_size,
                        sizeof(Scalar),
                        sizeof(Index)));
            }
            if (!options.quiet) {
                logger().warn(
                    "Casting SimpleScene types: buffer has scalar_size={} index_size={}, "
                    "requested scalar_size={} index_size={}",
                    cscene->scalar_type_size,
                    cscene->index_type_size,
                    sizeof(Scalar),
                    sizeof(Index));
            }
            return internal::deserialize_simple_scene_with_cast<Scalar, Index, SceneType::Dim>(
                data);
        }
        return internal::from_cista_simple_scene<Scalar, Index, SceneType::Dim>(*cscene);
    };

    if (!options.allow_scene_conversion) {
        return load_native_simple_scene();
    }

    auto type = internal::detect_encoded_type(data);
    switch (type) {
    case internal::EncodedType::SimpleScene: return load_native_simple_scene();
    case internal::EncodedType::Mesh: {
        if (!options.quiet) {
            logger().warn("Buffer contains a Mesh, converting to SimpleScene");
        }
        DeserializeOptions native_opts;
        native_opts.allow_scene_conversion = false;
        auto mesh = deserialize_mesh<SurfaceMesh<Scalar, Index>>(data, native_opts);
        return scene::mesh_to_simple_scene<Dimension>(std::move(mesh));
    }
    case internal::EncodedType::Scene: {
        if constexpr (Dimension == 3) {
            if (!options.quiet) {
                logger().warn("Buffer contains a Scene, converting to SimpleScene");
            }
            DeserializeOptions native_opts;
            native_opts.allow_scene_conversion = false;
            auto scene = deserialize_scene<scene::Scene<Scalar, Index>>(data, native_opts);
            return scene::scene_to_simple_scene(scene);
        } else {
            throw std::runtime_error(
                "Cannot convert an encoded Scene to a SimpleScene with Dimension != 3");
        }
    }
    default: throw std::runtime_error("Unknown encoded data type in buffer");
    }
}

template <typename Scalar, typename Index, size_t Dimension>
void save_simple_scene(
    const fs::path& filename,
    const scene::SimpleScene<Scalar, Index, Dimension>& scene,
    const SerializeOptions& options)
{
    auto buf = serialize_simple_scene(scene, options);
    fs::ofstream ofs(filename, std::ios::binary);
    la_runtime_assert(ofs.good(), "Failed to open file for writing: " + filename.string());
    ofs.write(reinterpret_cast<const char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
    la_runtime_assert(ofs.good(), "Failed to write to file: " + filename.string());
}

template <typename SceneType>
SceneType load_simple_scene(const fs::path& filename, const DeserializeOptions& options)
{
    auto buf = internal::read_file_to_buffer(filename);
    if (is_compressed(buf)) {
        buf = decompress_buffer(buf);
    }
    return deserialize_simple_scene<SceneType>(lagrange::span<const uint8_t>(buf), options);
}

// Explicit template instantiations
#define LA_X_serialization2_ss(_, Scalar, Index, Dim)                          \
    template LA_SERIALIZATION2_API std::vector<uint8_t>                        \
    serialize_simple_scene<Scalar, Index, Dim>(                                \
        const scene::SimpleScene<Scalar, Index, Dim>&,                         \
        const SerializeOptions&);                                              \
    template LA_SERIALIZATION2_API scene::SimpleScene<Scalar, Index, Dim>      \
    deserialize_simple_scene<scene::SimpleScene<Scalar, Index, Dim>>(          \
        span<const uint8_t>,                                                   \
        const DeserializeOptions&);                                            \
    template LA_SERIALIZATION2_API void save_simple_scene<Scalar, Index, Dim>( \
        const fs::path&,                                                       \
        const scene::SimpleScene<Scalar, Index, Dim>&,                         \
        const SerializeOptions&);                                              \
    template LA_SERIALIZATION2_API scene::SimpleScene<Scalar, Index, Dim>      \
    load_simple_scene<scene::SimpleScene<Scalar, Index, Dim>>(                 \
        const fs::path&,                                                       \
        const DeserializeOptions&);
LA_SIMPLE_SCENE_X(serialization2_ss, 0)
#undef LA_X_serialization2_ss

} // namespace lagrange::serialization
