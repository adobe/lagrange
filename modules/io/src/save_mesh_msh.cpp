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

#include <lagrange/Attribute.h>
#include <lagrange/AttributeTypes.h>
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMesh.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/foreach_attribute.h>
#include <lagrange/internal/attribute_string_utils.h>
#include <lagrange/internal/find_attribute_utils.h>
#include <lagrange/io/api.h>
#include <lagrange/io/save_mesh_msh.h>
#include <lagrange/map_attribute.h>
#include <lagrange/utils/Error.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/fmt/format.h>
#include <lagrange/utils/range.h>
#include <lagrange/utils/safe_cast.h>
#include <lagrange/views.h>

#include "internal/convert_attribute_utils.h"

#include <mshio/mshio.h>

#include <cstdint>
#include <vector>

namespace lagrange {
namespace io {

namespace {

struct AttributeCounts
{
    size_t normal_count = 0;
    size_t uv_count = 0;
    size_t color_count = 0;
};

template <typename Scalar, typename Index>
std::string
get_attribute_name(const SurfaceMesh<Scalar, Index>& mesh, AttributeId id, AttributeCounts& counts)
{
    std::string name;
    auto usage = mesh.get_attribute_base(id).get_usage();
    auto element = mesh.get_attribute_base(id).get_element_type();

    switch (usage) {
    case AttributeUsage::UV:
        name = format(
            "{}_{}_{}",
            internal::to_string(element),
            internal::to_string(usage),
            counts.uv_count++);
        break;
    case AttributeUsage::Normal:
        name = format(
            "{}_{}_{}",
            internal::to_string(element),
            internal::to_string(usage),
            counts.normal_count++);
        break;
    case AttributeUsage::Color:
        name = format(
            "{}_{}_{}",
            internal::to_string(element),
            internal::to_string(usage),
            counts.color_count++);
        break;
    default:
        name = mesh.get_attribute_name(id);
        if (!name.empty() && name[0] == '@') {
            name = name.substr(1); // Remove the '@' prefix.
        }
    }
    return name;
}

template <typename Index>
int element_type_from_facet_size(Index facet_size, Index facet_index)
{
    switch (facet_size) {
    case 2: return 1; // 2-node line.
    case 3: return 2; // 3-node triangle.
    case 4: return 3; // 4-node quad.
    default:
        throw Error(format(
            "MSH export only supports line segments, triangles, and quads; facet {} has {} "
            "vertices",
            facet_index,
            facet_size));
    }
}

template <typename Scalar, typename Index>
void append_node(mshio::NodeBlock& block, const SurfaceMesh<Scalar, Index>& mesh, Index vertex)
{
    auto position = mesh.get_position(vertex);
    block.tags.push_back(static_cast<size_t>(vertex + 1));
    block.data.push_back(static_cast<double>(position[0]));
    block.data.push_back(static_cast<double>(position[1]));
    block.data.push_back(mesh.get_dimension() == 3 ? static_cast<double>(position[2]) : 0);
}

template <typename Scalar, typename Index>
void populate_nodes(mshio::MshSpec& spec, const SurfaceMesh<Scalar, Index>& mesh)
{
    const Index dim = mesh.get_dimension();
    if (dim != 2 && dim != 3) {
        throw Error("Only 2D and 3D mesh are supported!");
    }

    const Index num_vertices = mesh.get_num_vertices();
    std::vector<bool> on_line(static_cast<size_t>(num_vertices), false);
    for (Index f = 0; f < mesh.get_num_facets(); ++f) {
        if (mesh.get_facet_size(f) != 2) continue;
        for (Index lv = 0; lv < 2; ++lv) {
            const Index vertex = mesh.get_facet_vertex(f, lv);
            la_runtime_assert(vertex < num_vertices, "Line facet vertex is out of range");
            on_line[static_cast<size_t>(vertex)] = true;
        }
    }

    spec.nodes.num_nodes = static_cast<size_t>(num_vertices);
    spec.nodes.min_node_tag = 1;
    spec.nodes.max_node_tag = static_cast<size_t>(num_vertices);
    spec.nodes.entity_blocks.clear();

    auto add_block = [&](int entity_dim, bool line_vertices) {
        mshio::NodeBlock block;
        block.entity_dim = entity_dim;
        block.entity_tag = 1;
        block.parametric = 0;
        for (Index vertex = 0; vertex < num_vertices; ++vertex) {
            if (on_line[static_cast<size_t>(vertex)] != line_vertices) continue;
            append_node(block, mesh, vertex);
        }
        block.num_nodes_in_block = block.tags.size();
        if (block.num_nodes_in_block == 0) return;
        spec.nodes.entity_blocks.push_back(std::move(block));
    };

    if (num_vertices == 0) {
        // Historical empty surface block for a mesh with no vertices.
        mshio::NodeBlock block;
        block.entity_dim = 2;
        block.entity_tag = 1;
        block.parametric = 0;
        spec.nodes.entity_blocks.push_back(std::move(block));
    } else {
        // Vertices incident to a line are classified on the curve. Tags stay vertex index + 1.
        add_block(1, true);
        add_block(2, false);
    }
    spec.nodes.num_entity_blocks = spec.nodes.entity_blocks.size();
}

template <typename Scalar, typename Index>
void populate_elements(mshio::MshSpec& spec, const SurfaceMesh<Scalar, Index>& mesh)
{
    const Index num_facets = mesh.get_num_facets();
    spec.elements.num_elements = static_cast<size_t>(num_facets);
    spec.elements.min_element_tag = 1;
    spec.elements.max_element_tag = static_cast<size_t>(num_facets);
    spec.elements.entity_blocks.clear();

    // Point clouds have no facets. Keep the historical empty triangle block.
    if (num_facets == 0) {
        spec.elements.num_entity_blocks = 1;
        auto& element_block = spec.elements.entity_blocks.emplace_back();
        element_block.entity_dim = 2;
        element_block.entity_tag = 1;
        element_block.element_type = 2;
        element_block.num_elements_in_block = 0;
        return;
    }

    // MSH element blocks are uniform. Split the mesh into contiguous runs so facet order, and
    // therefore element tags, stay aligned with facet and corner attributes.
    Index run_begin = 0;
    while (run_begin < num_facets) {
        const Index facet_size = mesh.get_facet_size(run_begin);
        const int element_type = element_type_from_facet_size(facet_size, run_begin);
        Index run_end = run_begin + 1;
        while (run_end < num_facets && mesh.get_facet_size(run_end) == facet_size) {
            ++run_end;
        }

        auto& element_block = spec.elements.entity_blocks.emplace_back();
        element_block.entity_dim = element_type == 1 ? 1 : 2;
        element_block.entity_tag = 1;
        element_block.element_type = element_type;
        element_block.num_elements_in_block = static_cast<size_t>(run_end - run_begin);
        element_block.data.reserve(
            static_cast<size_t>(run_end - run_begin) * static_cast<size_t>(facet_size + 1));
        for (Index f = run_begin; f < run_end; ++f) {
            element_block.data.push_back(static_cast<size_t>(f + 1)); // element tag.
            for (Index lv = 0; lv < facet_size; ++lv) {
                element_block.data.push_back(static_cast<size_t>(mesh.get_facet_vertex(f, lv) + 1));
            }
        }
        run_begin = run_end;
    }
    spec.elements.num_entity_blocks = spec.elements.entity_blocks.size();
}

template <typename Scalar, typename Index, typename Value>
void populate_non_indexed_vertex_attribute(
    mshio::MshSpec& spec,
    const SurfaceMesh<Scalar, Index>& mesh,
    AttributeId id,
    AttributeCounts& counts)
{
    la_debug_assert(mesh.template is_attribute_type<Value>(id));
    const auto& attr = mesh.template get_attribute<Value>(id);
    [[maybe_unused]] const AttributeElement element = attr.get_element_type();
    la_debug_assert(element == AttributeElement::Vertex);
    std::string name = get_attribute_name(mesh, id, counts);

    const auto num_vertices = mesh.get_num_vertices();
    const auto num_channels = attr.get_num_channels();

    spec.node_data.emplace_back();
    auto& node_data = spec.node_data.back();
    node_data.header.string_tags.emplace_back(std::move(name));
    node_data.header.real_tags.push_back(0); // Time value.
    node_data.header.int_tags = {
        0, // Time step.
        static_cast<int>(num_channels),
        static_cast<int>(num_vertices),
        0 // Partition index.
    };

    node_data.entries.reserve(num_vertices);
    for (auto i : range(num_vertices)) {
        node_data.entries.emplace_back();
        auto& entry = node_data.entries.back();
        entry.tag = safe_cast<std::int32_t>(i + 1);
        entry.data.reserve(num_channels);

        for (auto j : range(num_channels)) {
            entry.data.push_back(static_cast<double>(attr.get(i, j)));
        }
    }
}

template <typename Scalar, typename Index, typename Value>
void populate_non_indexed_facet_attribute(
    mshio::MshSpec& spec,
    const SurfaceMesh<Scalar, Index>& mesh,
    AttributeId id,
    AttributeCounts& counts)
{
    la_debug_assert(mesh.template is_attribute_type<Value>(id));
    const auto& attr = mesh.template get_attribute<Value>(id);
    [[maybe_unused]] const AttributeElement element = attr.get_element_type();
    la_debug_assert(element == AttributeElement::Facet);
    std::string name = get_attribute_name(mesh, id, counts);

    const auto num_facets = mesh.get_num_facets();
    const auto num_channels = attr.get_num_channels();

    spec.element_data.emplace_back();
    auto& element_data = spec.element_data.back();
    element_data.header.string_tags.emplace_back(name);
    element_data.header.real_tags.push_back(0); // Time value.
    element_data.header.int_tags = {
        0, // Time step.
        static_cast<int>(num_channels),
        static_cast<int>(num_facets),
        0 // Partition index.
    };

    element_data.entries.reserve(num_facets);
    for (auto i : range(num_facets)) {
        element_data.entries.emplace_back();
        auto& entry = element_data.entries.back();
        entry.tag = safe_cast<std::int32_t>(i + 1);
        entry.data.reserve(num_channels);

        for (auto j : range(num_channels)) {
            entry.data.push_back(static_cast<double>(attr.get(i, j)));
        }
    }
}

template <typename Scalar, typename Index, typename Value>
void populate_non_indexed_edge_attribute(
    mshio::MshSpec& /*spec*/,
    const SurfaceMesh<Scalar, Index>& mesh,
    AttributeId id,
    AttributeCounts& /*counts*/,
    bool quiet)
{
    if (!quiet) {
        auto attr_name = mesh.get_attribute_name(id);
        logger().warn("Skipping saving edge attribute {} in MSH format", attr_name);
    }
}

template <typename Scalar, typename Index, typename Value>
void populate_non_indexed_corner_attribute(
    mshio::MshSpec& spec,
    const SurfaceMesh<Scalar, Index>& mesh,
    AttributeId id,
    AttributeCounts& counts)
{
    la_debug_assert(mesh.template is_attribute_type<Value>(id));
    const auto& attr = mesh.template get_attribute<Value>(id);
    [[maybe_unused]] const AttributeElement element = attr.get_element_type();
    la_debug_assert(element == AttributeElement::Corner);
    std::string name = get_attribute_name(mesh, id, counts);

    const auto num_facets = mesh.get_num_facets();
    const auto num_channels = attr.get_num_channels();

    spec.element_node_data.emplace_back();
    auto& element_node_data = spec.element_node_data.back();
    element_node_data.header.string_tags.emplace_back(name);
    element_node_data.header.real_tags.push_back(0); // Time value.
    element_node_data.header.int_tags = {
        0, // Time step.
        static_cast<int>(num_channels),
        static_cast<int>(num_facets),
        0 // Partition index.
    };

    element_node_data.entries.reserve(num_facets);
    for (auto i : range(num_facets)) {
        const auto facet_size = mesh.get_facet_size(i);
        const auto corner_begin = mesh.get_facet_corner_begin(i);
        element_node_data.entries.emplace_back();
        auto& entry = element_node_data.entries.back();
        entry.tag = safe_cast<std::int32_t>(i + 1);
        entry.num_nodes_per_element = int(facet_size);
        entry.data.reserve(num_channels * facet_size);
        for (auto j : range(facet_size)) {
            for (auto k : range(num_channels)) {
                entry.data.push_back(static_cast<double>(attr.get(corner_begin + j, k)));
            }
        }
    }
}

template <typename Scalar, typename Index>
void populate_non_indexed_attribute(
    mshio::MshSpec& spec,
    const SurfaceMesh<Scalar, Index>& mesh,
    AttributeId id,
    AttributeCounts& counts,
    bool quiet)
{
    la_runtime_assert(!mesh.is_attribute_indexed(id));
    const auto& attr_base = mesh.get_attribute_base(id);

    switch (attr_base.get_element_type()) {
    case AttributeElement::Vertex: {
#define LA_X_try_attribute(_, T)                \
    if (mesh.template is_attribute_type<T>(id)) \
        populate_non_indexed_vertex_attribute<Scalar, Index, T>(spec, mesh, id, counts);
        LA_ATTRIBUTE_X(try_attribute, 0)
#undef LA_X_try_attribute
        break;
    }
    case AttributeElement::Facet: {
#define LA_X_try_attribute(_, T)                \
    if (mesh.template is_attribute_type<T>(id)) \
        populate_non_indexed_facet_attribute<Scalar, Index, T>(spec, mesh, id, counts);
        LA_ATTRIBUTE_X(try_attribute, 0)
#undef LA_X_try_attribute
        break;
    }
    case AttributeElement::Edge: {
#define LA_X_try_attribute(_, T)                \
    if (mesh.template is_attribute_type<T>(id)) \
        populate_non_indexed_edge_attribute<Scalar, Index, T>(spec, mesh, id, counts, quiet);
        LA_ATTRIBUTE_X(try_attribute, 0)
#undef LA_X_try_attribute
        break;
    }
    case AttributeElement::Corner: {
#define LA_X_try_attribute(_, T)                \
    if (mesh.template is_attribute_type<T>(id)) \
        populate_non_indexed_corner_attribute<Scalar, Index, T>(spec, mesh, id, counts);
        LA_ATTRIBUTE_X(try_attribute, 0)
#undef LA_X_try_attribute
        break;
    }
    case AttributeElement::Value: {
        if (!quiet) {
            logger().warn(
                "Ignoring value attribute {} when saving to MSH format.",
                mesh.get_attribute_name(id));
        }
        break;
    }
    default: throw Error("Unsupported attribute element type!"); break;
    }
}

template <typename Scalar, typename Index>
void populate_indexed_attribute(
    mshio::MshSpec& spec,
    const SurfaceMesh<Scalar, Index>& mesh,
    AttributeId id,
    AttributeCounts& counts)
{
    std::string_view name = mesh.get_attribute_name(id);
    logger().info("Saving indexed attribute {} as corner attribute", name);

    auto tmp_mesh = SurfaceMesh<Scalar, Index>::stripped_copy(mesh);
    auto new_id = tmp_mesh.create_attribute_from(name, mesh, name);
    new_id = map_attribute_in_place(tmp_mesh, new_id, AttributeElement::Corner);

#define LA_X_try_attribute(_, T)                        \
    if (tmp_mesh.template is_attribute_type<T>(new_id)) \
        populate_non_indexed_corner_attribute<Scalar, Index, T>(spec, tmp_mesh, new_id, counts);
    LA_ATTRIBUTE_X(try_attribute, 0)
#undef LA_X_try_attribute
}

template <typename Scalar, typename Index>
void populate_attribute(
    mshio::MshSpec& spec,
    const SurfaceMesh<Scalar, Index>& mesh,
    AttributeId id,
    AttributeCounts& counts,
    bool quiet)
{
    if (mesh.is_attribute_indexed(id)) {
        populate_indexed_attribute(spec, mesh, id, counts);
    } else {
        populate_non_indexed_attribute(spec, mesh, id, counts, quiet);
    }
}

} // namespace

template <typename Scalar, typename Index>
void save_mesh_msh(
    std::ostream& output_stream,
    const SurfaceMesh<Scalar, Index>& mesh,
    const SaveOptions& options)
{
    // Hanlde index attribute conversion if necessary.
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
        return save_mesh_msh(output_stream, mesh2, options2);
    }

    la_runtime_assert(output_stream, "Invalid output stream");
    mshio::MshSpec spec;
    spec.mesh_format.file_type = options.encoding == FileEncoding::Binary ? 1 : 0;
    populate_nodes(spec, mesh);
    populate_elements(spec, mesh);

    AttributeCounts counts;
    if (options.output_attributes == SaveOptions::OutputAttributes::All) {
        mesh.seq_foreach_attribute_id([&](AttributeId id) {
            auto name = mesh.get_attribute_name(id);
            if (!mesh.attr_name_is_reserved(name)) {
                populate_attribute(spec, mesh, id, counts, options.quiet);
            }
        });
    } else {
        for (auto id : options.selected_attributes) {
            populate_attribute(spec, mesh, id, counts, options.quiet);
        }
    }

    mshio::save_msh(output_stream, spec);
}

template <typename Scalar, typename Index>
void save_mesh_msh(
    const fs::path& filename,
    const SurfaceMesh<Scalar, Index>& mesh,
    const SaveOptions& options)
{
    fs::path parent_dir = filename.parent_path();
    if (!parent_dir.empty() && !fs::exists(parent_dir)) fs::create_directories(parent_dir);

    fs::ofstream fout(
        filename,
        (options.encoding == io::FileEncoding::Binary ? std::ios::binary : std::ios::out));

    if (!fout) {
        throw std::runtime_error(
            format("Failed to open MSH file for writing: {}", filename.string()));
    }

    save_mesh_msh(fout, mesh, options);
}

#define LA_X_save_mesh_msh(_, S, I)        \
    template LA_IO_API void save_mesh_msh( \
        std::ostream&,                     \
        const SurfaceMesh<S, I>& mesh,     \
        const SaveOptions& options);       \
    template LA_IO_API void save_mesh_msh( \
        const fs::path& filename,          \
        const SurfaceMesh<S, I>& mesh,     \
        const SaveOptions& options);
LA_SURFACE_MESH_X(save_mesh_msh, 0)
#undef LA_X_save_mesh_msh

} // namespace io
} // namespace lagrange
