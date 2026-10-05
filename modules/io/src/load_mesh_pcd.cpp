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

#include <lagrange/io/load_mesh_pcd.h>

#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/io/api.h>
#include <lagrange/utils/Error.h>

#include <lagrange/Attribute.h>
#include <lagrange/SurfaceMesh.h>
#include <lagrange/attribute_names.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/fmt/format.h>

#include "internal/pcd_utils.h"

#include <pcdio/pcdio.h>

#include <cstdint>
#include <cstring>
#include <istream>
#include <string>
#include <string_view>

namespace lagrange::io {

namespace {

using internal::dispatch_pcd_type;

// Read the value of `field` at point `i`, channel `c`, cast to `Out`. `PcdField::data` is stored in
// point-major order: point i, channel c occupies bytes [(i * count + c) * size, ...).
template <typename Out>
Out read_field_value(const pcdio::PcdField& field, size_t i, int c)
{
    const uint8_t* ptr =
        field.data.data() + (i * static_cast<size_t>(field.count) + static_cast<size_t>(c)) *
                                static_cast<size_t>(field.size);
    Out out{};
    dispatch_pcd_type(field.type, field.size, [&](auto zero) {
        using ValueType = decltype(zero);
        ValueType value;
        std::memcpy(&value, ptr, sizeof(ValueType));
        out = static_cast<Out>(value);
    });
    return out;
}

} // namespace

template <typename MeshType>
MeshType load_mesh_pcd(std::istream& input_stream, const LoadOptions& options)
{
    using Scalar = typename MeshType::Scalar;
    using Index = typename MeshType::Index;

    MeshType mesh;

    // The PCD container (ascii, binary, and LZF-compressed binary_compressed) is parsed by the
    // pcdio library; this function only maps the resulting fields onto Lagrange mesh attributes.
    const pcdio::PcdSpec spec = pcdio::load_pcd(input_stream);
    const size_t points = spec.points;

    // Positions.
    const pcdio::PcdField* fx = spec.find_field("x");
    const pcdio::PcdField* fy = spec.find_field("y");
    const pcdio::PcdField* fz = spec.find_field("z");
    la_runtime_assert(fx && fy && fz, "PCD file must contain x, y and z fields.");
    mesh.add_vertices(static_cast<Index>(points), [&](Index v, span<Scalar> p) {
        const size_t i = static_cast<size_t>(v);
        p[0] = read_field_value<Scalar>(*fx, i, 0);
        p[1] = read_field_value<Scalar>(*fy, i, 0);
        p[2] = read_field_value<Scalar>(*fz, i, 0);
    });

    // Normals.
    if (options.load_normals) {
        const pcdio::PcdField* nx = spec.find_field("normal_x");
        const pcdio::PcdField* ny = spec.find_field("normal_y");
        const pcdio::PcdField* nz = spec.find_field("normal_z");
        if (!nx) nx = spec.find_field("nx");
        if (!ny) ny = spec.find_field("ny");
        if (!nz) nz = spec.find_field("nz");
        if (nx && ny && nz) {
            dispatch_pcd_type(nx->type, nx->size, [&](auto zero) {
                using ValueType = decltype(zero);
                auto id = mesh.template create_attribute<ValueType>(
                    AttributeName::normal,
                    AttributeElement::Vertex,
                    AttributeUsage::Normal,
                    3);
                auto attr = mesh.template ref_attribute<ValueType>(id).ref_all();
                for (size_t i = 0; i < points; ++i) {
                    attr[i * 3 + 0] = read_field_value<ValueType>(*nx, i, 0);
                    attr[i * 3 + 1] = read_field_value<ValueType>(*ny, i, 0);
                    attr[i * 3 + 2] = read_field_value<ValueType>(*nz, i, 0);
                }
            });
        }
    }

    // Colors: packed rgb (float bits) or rgba (uint32), unpacked into uint8 channels.
    if (options.load_vertex_colors) {
        const pcdio::PcdField* rgba = spec.find_field("rgba");
        const pcdio::PcdField* rgb = spec.find_field("rgb");
        auto unpack = [&](const pcdio::PcdField& field, Index channels) {
            auto id = mesh.template create_attribute<uint8_t>(
                AttributeName::color,
                AttributeElement::Vertex,
                AttributeUsage::Color,
                channels);
            auto attr = mesh.template ref_attribute<uint8_t>(id).ref_all();
            const size_t n = static_cast<size_t>(channels);
            for (size_t i = 0; i < points; ++i) {
                uint32_t packed;
                std::memcpy(&packed, field.data.data() + i * field.stride(), 4);
                attr[i * n + 0] = static_cast<uint8_t>((packed >> 16) & 0xffu);
                attr[i * n + 1] = static_cast<uint8_t>((packed >> 8) & 0xffu);
                attr[i * n + 2] = static_cast<uint8_t>(packed & 0xffu);
                if (channels == 4) attr[i * n + 3] = static_cast<uint8_t>((packed >> 24) & 0xffu);
            }
        };
        if (rgba && rgba->size == 4) {
            unpack(*rgba, 4);
        } else if (rgb && rgb->size == 4) {
            unpack(*rgb, 3);
        }
    }

    // Remaining fields become generic vertex attributes named after the field.
    static const std::string_view reserved_fields[] =
        {"x", "y", "z", "normal_x", "normal_y", "normal_z", "nx", "ny", "nz", "rgb", "rgba", "_"};
    auto is_reserved_field = [&](std::string_view name) {
        for (auto reserved : reserved_fields) {
            if (name == reserved) return true;
        }
        return false;
    };

    for (const auto& field : spec.fields) {
        if (field.name.empty() || is_reserved_field(field.name)) continue;
        dispatch_pcd_type(field.type, field.size, [&](auto zero) {
            using ValueType = decltype(zero);
            const Index num_channels = static_cast<Index>(field.count);
            const AttributeUsage usage =
                num_channels == 1 ? AttributeUsage::Scalar : AttributeUsage::Vector;
            auto id = mesh.template create_attribute<ValueType>(
                field.name,
                AttributeElement::Vertex,
                usage,
                num_channels);
            auto attr = mesh.template ref_attribute<ValueType>(id).ref_all();
            const size_t n = static_cast<size_t>(num_channels);
            for (size_t i = 0; i < points; ++i) {
                for (int c = 0; c < field.count; ++c) {
                    attr[i * n + static_cast<size_t>(c)] = read_field_value<ValueType>(field, i, c);
                }
            }
        });
    }

    return mesh;
}

template <typename MeshType>
MeshType load_mesh_pcd(const fs::path& filename, const LoadOptions& options)
{
    fs::ifstream fin(filename, std::ios::binary);
    la_runtime_assert(fin.good(), format("Unable to open file {}", filename.string()));
    return load_mesh_pcd<MeshType>(fin, options);
}

#define LA_X_load_mesh_pcd(_, S, I)                                                                \
    template LA_IO_API SurfaceMesh<S, I> load_mesh_pcd(std::istream&, const LoadOptions& options); \
    template LA_IO_API SurfaceMesh<S, I> load_mesh_pcd(                                            \
        const fs::path& filename,                                                                  \
        const LoadOptions& options);
LA_SURFACE_MESH_X(load_mesh_pcd, 0)

} // namespace lagrange::io
