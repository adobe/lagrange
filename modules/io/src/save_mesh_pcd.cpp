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

#include <lagrange/io/save_mesh_pcd.h>

#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/io/api.h>

#include <stdexcept>

#include <lagrange/Attribute.h>
#include <lagrange/Logger.h>
#include <lagrange/foreach_attribute.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/fmt/format.h>
#include <lagrange/views.h>

#include "internal/pcd_utils.h"

#include <pcdio/pcdio.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace lagrange::io {

namespace {

using internal::pcd_type_char;

template <typename ValueType>
uint8_t to_u8(ValueType v)
{
    if constexpr (std::is_floating_point_v<ValueType>) {
        double d = std::round(static_cast<double>(v) * 255.0);
        return static_cast<uint8_t>(std::clamp(d, 0.0, 255.0));
    } else {
        long long x = static_cast<long long>(v);
        return static_cast<uint8_t>(std::clamp<long long>(x, 0, 255));
    }
}

// Append the raw bytes of `value` to `bytes`.
template <typename ValueType>
void append_bytes(std::vector<uint8_t>& bytes, ValueType value)
{
    const auto* p = reinterpret_cast<const uint8_t*>(&value);
    bytes.insert(bytes.end(), p, p + sizeof(ValueType));
}

} // namespace

template <typename Scalar, typename Index>
void save_mesh_pcd(
    std::ostream& output_stream,
    const SurfaceMesh<Scalar, Index>& mesh,
    const SaveOptions& options)
{
    la_runtime_assert(mesh.get_dimension() == 3, "PCD export requires a 3D mesh.");

    const size_t num_points = static_cast<size_t>(mesh.get_num_vertices());

    // Build a pcdio::PcdSpec and let the pcdio library serialize it (header + ascii/binary/
    // binary_compressed LZF encoding). This function only maps Lagrange attributes onto PCD fields.
    pcdio::PcdSpec spec;
    spec.width = num_points;
    spec.height = 1;
    spec.points = num_points;
    // spec.viewpoint keeps its default {0, 0, 0, 1, 0, 0, 0}.

    // Vertex positions: x, y, z.
    {
        auto pos = vertex_view(mesh);
        for (int k = 0; k < 3; ++k) {
            pcdio::PcdField field;
            field.name = (k == 0 ? "x" : (k == 1 ? "y" : "z"));
            field.type = pcd_type_char<Scalar>();
            field.size = static_cast<int>(sizeof(Scalar));
            field.count = 1;
            field.data.reserve(num_points * sizeof(Scalar));
            for (size_t i = 0; i < num_points; ++i) {
                append_bytes(field.data, static_cast<Scalar>(pos(static_cast<Eigen::Index>(i), k)));
            }
            spec.fields.push_back(std::move(field));
        }
    }

    bool wrote_normal = false;
    bool wrote_color = false;

    auto register_attribute = [&](std::string_view name, auto&& attr) {
        if (mesh.attr_name_is_reserved(name)) return;
        using AttributeType = std::decay_t<decltype(attr)>;
        using ValueType = typename AttributeType::ValueType;
        const size_t num_channels = static_cast<size_t>(attr.get_num_channels());
        const AttributeUsage usage = attr.get_usage();

        if (usage == AttributeUsage::Normal && num_channels >= 3 && !wrote_normal) {
            wrote_normal = true;
            const char* suffixes[3] = {"normal_x", "normal_y", "normal_z"};
            for (int k = 0; k < 3; ++k) {
                pcdio::PcdField field;
                field.name = suffixes[k];
                field.type = pcd_type_char<ValueType>();
                field.size = static_cast<int>(sizeof(ValueType));
                field.count = 1;
                field.data.reserve(num_points * sizeof(ValueType));
                for (size_t i = 0; i < num_points; ++i) {
                    append_bytes(field.data, attr.get(i, k));
                }
                spec.fields.push_back(std::move(field));
            }
            return;
        }

        if (usage == AttributeUsage::Color && (num_channels == 3 || num_channels == 4) &&
            !wrote_color) {
            wrote_color = true;
            const bool has_alpha = (num_channels == 4);
            pcdio::PcdField field;
            field.name = has_alpha ? "rgba" : "rgb";
            field.type = has_alpha ? 'U' : 'F'; // PCL packs rgb as a float, rgba as a uint32.
            field.size = 4;
            field.count = 1;
            field.data.reserve(num_points * 4);
            for (size_t i = 0; i < num_points; ++i) {
                uint32_t r = to_u8(attr.get(i, 0));
                uint32_t g = to_u8(attr.get(i, 1));
                uint32_t b = to_u8(attr.get(i, 2));
                uint32_t a = has_alpha ? to_u8(attr.get(i, 3)) : 0u;
                uint32_t packed = (a << 24) | (r << 16) | (g << 8) | b;
                append_bytes(field.data, packed);
            }
            spec.fields.push_back(std::move(field));
            return;
        }

        // Skip attributes whose name collides with a PCD-owned field or cannot be represented as a
        // whitespace-delimited FIELDS token; emitting them would produce an ambiguous or unreadable
        // file.
        static constexpr std::string_view reserved_names[] = {
            "x",
            "y",
            "z",
            "normal_x",
            "normal_y",
            "normal_z",
            "nx",
            "ny",
            "nz",
            "rgb",
            "rgba",
            "_"};
        bool reserved = name.empty() || name.find_first_of(" \t\r\n") != std::string_view::npos;
        for (auto r : reserved_names) reserved = reserved || (name == r);
        if (reserved) {
            if (!options.quiet) {
                logger().warn(
                    "Skipping vertex attribute '{}' when saving PCD: its name is empty, contains "
                    "whitespace, or collides with a reserved PCD field.",
                    name);
            }
            return;
        }

        // Generic attribute: one field carrying all channels.
        pcdio::PcdField field;
        field.name = std::string(name);
        field.type = pcd_type_char<ValueType>();
        field.size = static_cast<int>(sizeof(ValueType));
        field.count = static_cast<int>(num_channels);
        field.data.reserve(num_points * num_channels * sizeof(ValueType));
        for (size_t i = 0; i < num_points; ++i) {
            for (size_t c = 0; c < num_channels; ++c) {
                append_bytes(field.data, attr.get(i, c));
            }
        }
        spec.fields.push_back(std::move(field));
    };

    if (options.output_attributes == SaveOptions::OutputAttributes::All) {
        seq_foreach_named_attribute_read<AttributeElement::Vertex>(mesh, register_attribute);
    } else if (!options.selected_attributes.empty()) {
        details::internal_foreach_named_attribute<
            AttributeElement::Vertex,
            details::Ordering::Sequential,
            details::Access::Read>(mesh, register_attribute, options.selected_attributes);
    }

    // Binary output is written as LZF-compressed `binary_compressed`, matching PCL's default and
    // keeping files compact. Uncompressed `binary` is still supported on read.
    spec.data = (options.encoding == FileEncoding::Ascii) ? "ascii" : "binary_compressed";

    pcdio::save_pcd(output_stream, spec);
}

template <typename Scalar, typename Index>
void save_mesh_pcd(
    const fs::path& filename,
    const SurfaceMesh<Scalar, Index>& mesh,
    const SaveOptions& options)
{
    fs::path parent_dir = filename.parent_path();
    if (!parent_dir.empty() && !fs::exists(parent_dir)) fs::create_directories(parent_dir);

    fs::ofstream fout(
        filename,
        (options.encoding == FileEncoding::Ascii ? std::ios::out : std::ios::binary));
    if (!fout) {
        throw std::runtime_error(
            format("Failed to open PCD file for writing: {}", filename.string()));
    }
    save_mesh_pcd(fout, mesh, options);
}

#define LA_X_save_mesh_pcd(_, Scalar, Index)    \
    template LA_IO_API void save_mesh_pcd(      \
        std::ostream&,                          \
        const SurfaceMesh<Scalar, Index>& mesh, \
        const SaveOptions& options);            \
    template LA_IO_API void save_mesh_pcd(      \
        const fs::path& filename,               \
        const SurfaceMesh<Scalar, Index>& mesh, \
        const SaveOptions& options);
LA_SURFACE_MESH_X(save_mesh_pcd, 0)

} // namespace lagrange::io
