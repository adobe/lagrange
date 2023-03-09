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

#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/foreach_attribute.h>
#include <lagrange/internal/find_attribute_utils.h>
#include <lagrange/io/save_mesh_ply.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/range.h>
#include <lagrange/views.h>

#include "internal/convert_attribute_utils.h"

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <happly.h>
#include <lagrange/utils/warnon.h>
// clang-format on

namespace lagrange::io {

namespace {

template <typename T, typename Derived>
std::vector<T> to_vector(const Eigen::DenseBase<Derived>& src)
{
    std::vector<T> dst(src.size());
    Eigen::VectorX<T>::Map(dst.data(), dst.size()) = src.template cast<T>();
    return dst;
}

template <typename T, typename Derived>
std::vector<std::vector<T>> to_vector_2d(const Eigen::DenseBase<Derived>& src)
{
    std::vector<std::vector<T>> dst(src.rows());
    for (auto i : range(src.rows())) {
        dst[i].resize(src.cols());
        for (auto j : range(src.cols())) {
            dst[i][j] = static_cast<T>(src(i, j));
        }
    }
    return dst;
}

template <typename T>
constexpr bool is_valid_ply_type()
{
    if constexpr (std::is_same_v<T, int8_t>) return true;
    if constexpr (std::is_same_v<T, uint8_t>) return true;
    if constexpr (std::is_same_v<T, int16_t>) return true;
    if constexpr (std::is_same_v<T, uint16_t>) return true;
    if constexpr (std::is_same_v<T, int32_t>) return true;
    if constexpr (std::is_same_v<T, uint32_t>) return true;
    if constexpr (std::is_same_v<T, float>) return true;
    if constexpr (std::is_same_v<T, double>) return true;
    return false;
}

template <typename T>
void register_normal(happly::Element& element, std::string_view name, T&& attr, size_t& count)
{
    using AttributeType = std::decay_t<decltype(attr)>;
    using ValueType = typename AttributeType::ValueType;
    logger().debug("Writing normal attribute '{}'", name);
    auto nrm = matrix_view(attr);
    std::string suffix = count == 0 ? "" : fmt::format("_{}", count);
    if constexpr (is_valid_ply_type<ValueType>()) {
        auto nx = to_vector<ValueType>(nrm.col(0));
        auto ny = to_vector<ValueType>(nrm.col(1));
        auto nz = to_vector<ValueType>(nrm.col(2));
        element.addProperty<ValueType>(fmt::format("nx{}", suffix), nx);
        element.addProperty<ValueType>(fmt::format("ny{}", suffix), ny);
        element.addProperty<ValueType>(fmt::format("nz{}", suffix), nz);
    } else {
        auto nx = to_vector<double>(nrm.col(0));
        auto ny = to_vector<double>(nrm.col(1));
        auto nz = to_vector<double>(nrm.col(2));
        element.addProperty<double>(fmt::format("nx{}", suffix), nx);
        element.addProperty<double>(fmt::format("ny{}", suffix), ny);
        element.addProperty<double>(fmt::format("nz{}", suffix), nz);
    }
    ++count;
}

template <typename T>
void register_uv(happly::Element& element, std::string_view name, T&& attr, size_t& count)
{
    using AttributeType = std::decay_t<decltype(attr)>;
    using ValueType = typename AttributeType::ValueType;
    logger().debug("Writing uv attribute '{}'", name);
    auto uv = matrix_view(attr);
    std::string suffix = count == 0 ? "" : fmt::format("_{}", count);
    if constexpr (is_valid_ply_type<ValueType>()) {
        auto s = to_vector<ValueType>(uv.col(0));
        auto t = to_vector<ValueType>(uv.col(1));
        element.addProperty<ValueType>(fmt::format("s{}", suffix), s);
        element.addProperty<ValueType>(fmt::format("t{}", suffix), t);
    } else {
        auto s = to_vector<double>(uv.col(0));
        auto t = to_vector<double>(uv.col(1));
        element.addProperty<double>(fmt::format("s{}", suffix), s);
        element.addProperty<double>(fmt::format("t{}", suffix), t);
    }
    ++count;
}

template <typename T>
void register_color(happly::Element& element, std::string_view name, T&& attr, size_t& count)
{
    using AttributeType = std::decay_t<decltype(attr)>;
    using ValueType = typename AttributeType::ValueType;
    auto num_channels = attr.get_num_channels();
    if (num_channels != 3 && num_channels != 4) return;
    auto num_elements = attr.get_num_elements();
    std::string suffix = count == 0 ? "" : fmt::format("_{}", count);

    auto to_unsigned_char = [](ValueType val) -> unsigned char {
        if constexpr (std::is_floating_point_v<ValueType>) {
            return static_cast<unsigned char>(std::round(val * 255));
        } else {
            return static_cast<unsigned char>(val);
        }
    };

    logger().debug("Writing color attribute '{}'", name);
    std::vector<unsigned char> buf;
    buf.resize(num_elements);
    auto add_channel = [&](std::string channel_name, size_t channel_index) {
        for (auto i : range(num_elements)) {
            buf[i] = to_unsigned_char(attr.get(i, channel_index));
        }
        element.addProperty<unsigned char>(channel_name, buf);
    };

    add_channel(fmt::format("red{}", suffix), 0);
    add_channel(fmt::format("green{}", suffix), 1);
    add_channel(fmt::format("blue{}", suffix), 2);
    if (num_channels == 4) {
        add_channel(fmt::format("alpha{}", suffix), 3);
    }

    ++count;
}

template <typename T>
void register_attribute(happly::Element& element, std::string_view name, T&& attr)
{
    using AttributeType = std::decay_t<decltype(attr)>;
    using ValueType = typename AttributeType::ValueType;
    logger().debug("Writing attribute '{}'", name);
    auto data = matrix_view(attr);
    if constexpr (is_valid_ply_type<ValueType>()) {
        if (attr.get_num_channels() == 1) {
            auto data2 = to_vector<ValueType>(data);
            element.addProperty<ValueType>(std::string(name), data2);
        } else {
            auto data2 = to_vector_2d<ValueType>(data);
            element.addListProperty<ValueType>(std::string(name), data2);
        }
    } else {
        if (attr.get_num_channels() == 1) {
            auto data2 = to_vector<double>(data);
            element.addProperty<double>(std::string(name), data2);
        } else {
            auto data2 = to_vector_2d<double>(data);
            element.addListProperty<double>(std::string(name), data2);
        }
    }
}


} // namespace

template <typename Scalar, typename Index>
void save_mesh_ply(
    std::ostream& output_stream,
    const SurfaceMesh<Scalar, Index>& mesh,
    const SaveOptions& options)
{
    la_runtime_assert(mesh.get_dimension() == 3);

    // Handle index attribute conversion if necessary.
    if (span<const AttributeId> attr_ids{
            options.selected_attributes.data(),
            options.selected_attributes.size()};
        options.attribute_conversion_policy ==
            SaveOptions::AttributeConversionPolicy::ConvertAsNeeded &&
        involve_indexed_attribute(mesh, attr_ids)) {
        auto [mesh2, attr_ids_2] = remap_indexed_attributes(mesh, attr_ids);

        SaveOptions options2 = options;
        options2.attribute_conversion_policy =
            SaveOptions::AttributeConversionPolicy::ExactMatchOnly;
        options2.selected_attributes.swap(attr_ids_2);
        return save_mesh_ply(output_stream, mesh2, options2);
    }

    // Create an empty object
    happly::PLYData ply;

    // Add mesh elements
    ply.addElement("vertex", mesh.get_num_vertices());
    ply.addElement("face", mesh.get_num_facets());
    if (mesh.has_edges()) {
        ply.addElement("edge", mesh.get_num_edges());
    }

    size_t vertex_normal_count = 0;
    size_t vertex_uv_count = 0;
    size_t vertex_color_count = 0;
    size_t facet_normal_count = 0;
    size_t facet_color_count = 0;

    auto register_vertex_attribute = [&](std::string_view name, auto&& attr) {
        if (mesh.attr_name_is_reserved(name)) return;
        using AttributeTypeRef = decltype(attr);
        switch (attr.get_usage()) {
        case AttributeUsage::UV:
            register_uv(
                ply.getElement("vertex"),
                name,
                std::forward<AttributeTypeRef>(attr),
                vertex_uv_count);
            break;
        case AttributeUsage::Normal:
            register_normal(
                ply.getElement("vertex"),
                name,
                std::forward<AttributeTypeRef>(attr),
                vertex_normal_count);
            break;
        case AttributeUsage::Color:
            register_color(
                ply.getElement("vertex"),
                name,
                std::forward<AttributeTypeRef>(attr),
                vertex_color_count);
            break;
        default:
            register_attribute(
                ply.getElement("vertex"),
                name,
                std::forward<AttributeTypeRef>(attr));
        }
    };

    auto register_facet_attribute = [&](std::string_view name, auto&& attr) {
        if (mesh.attr_name_is_reserved(name)) return;
        using AttributeTypeRef = decltype(attr);
        switch (attr.get_usage()) {
        case AttributeUsage::Normal:
            register_normal(
                ply.getElement("face"),
                name,
                std::forward<AttributeTypeRef>(attr),
                facet_normal_count);
            break;
        case AttributeUsage::Color:
            register_color(
                ply.getElement("face"),
                name,
                std::forward<AttributeTypeRef>(attr),
                facet_color_count);
            break;
        default:
            register_attribute(ply.getElement("face"), name, std::forward<AttributeTypeRef>(attr));
        }
    };

    // Vertex positions
    {
        auto pos = vertex_view(mesh);
        auto vx = to_vector<Scalar>(pos.col(0));
        auto vy = to_vector<Scalar>(pos.col(1));
        auto vz = to_vector<Scalar>(pos.col(2));
        ply.getElement("vertex").addProperty<Scalar>("x", vx);
        ply.getElement("vertex").addProperty<Scalar>("y", vy);
        ply.getElement("vertex").addProperty<Scalar>("z", vz);
    }

    // Facet indices
    {
        std::vector<std::vector<uint32_t>> facet_indices(mesh.get_num_facets());
        for (Index f = 0; f < mesh.get_num_facets(); ++f) {
            facet_indices[f].reserve(mesh.get_facet_size(f));
            for (Index v : mesh.get_facet_vertices(f)) {
                facet_indices[f].push_back(static_cast<uint32_t>(v));
            }
        }
        ply.getElement("face").addListProperty<uint32_t>("vertex_indices", facet_indices);
    }

    // Edge indices
    if (mesh.has_edges()) {
        std::vector<uint32_t> v1(mesh.get_num_edges());
        std::vector<uint32_t> v2(mesh.get_num_edges());
        for (Index e = 0; e < mesh.get_num_edges(); ++e) {
            auto verts = mesh.get_edge_vertices(e);
            v1[e] = uint32_t(verts[0]);
            v2[e] = uint32_t(verts[1]);
        }
        ply.getElement("edge").addProperty<uint32_t>("vertex1", v1);
        ply.getElement("edge").addProperty<uint32_t>("vertex2", v2);
    }

    details::internal_foreach_named_attribute<
        AttributeElement::Vertex,
        details::Ordering::Sequential,
        details::Access::Read>(mesh, register_vertex_attribute, options.selected_attributes);

    details::internal_foreach_named_attribute<
        AttributeElement::Facet,
        details::Ordering::Sequential,
        details::Access::Read>(mesh, register_facet_attribute, options.selected_attributes);

    // Write the object to file
    happly::DataFormat format;
    switch (options.encoding) {
    case FileEncoding::Binary: format = happly::DataFormat::Binary; break;
    case FileEncoding::Ascii: format = happly::DataFormat::ASCII; break;
    default: format = happly::DataFormat::Binary;
    }

    ply.validate();
    ply.write(output_stream, format);
}

template <typename Scalar, typename Index>
void save_mesh_ply(
    const fs::path& filename,
    const SurfaceMesh<Scalar, Index>& mesh,
    const SaveOptions& options)
{
    fs::ofstream fout(
        filename,
        (options.encoding == io::FileEncoding::Binary ? std::ios::binary : std::ios::out));
    save_mesh_ply(fout, mesh, options);
}

#define LA_X_save_mesh_ply(_, Scalar, Index)    \
    template void save_mesh_ply(                \
        const fs::path& filename,               \
        const SurfaceMesh<Scalar, Index>& mesh, \
        const SaveOptions& options);
LA_SURFACE_MESH_X(save_mesh_ply, 0)

} // namespace lagrange::io
