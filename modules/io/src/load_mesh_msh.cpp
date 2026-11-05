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
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/attribute_names.h>
#include <lagrange/internal/attribute_string_utils.h>
#include <lagrange/io/api.h>
#include <lagrange/io/load_mesh_msh.h>
#include <lagrange/triangulate_polygonal_facets.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/fmt/format.h>
#include <lagrange/utils/range.h>
#include <lagrange/utils/strings.h>
#include <mshio/mshio.h>

#include "stitch_mesh.h"

#include <algorithm>
#include <numeric>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace lagrange::io {

namespace {

template <typename Index>
struct MshLoadIndex
{
    std::unordered_map<size_t, Index> node_tag_to_vertex;
    std::unordered_map<size_t, Index> element_tag_to_facet;
};

bool is_line_element_block(const mshio::ElementBlock& block)
{
    return block.entity_dim == 1 && block.element_type == 1;
}

bool is_loaded_element_block(const mshio::ElementBlock& block, bool load_lines)
{
    // Surface blocks cover triangles, quads, and higher-order surface elements. 2-node lines are
    // curve elements and follow LoadOptions::load_lines.
    return block.entity_dim == 2 || (load_lines && is_line_element_block(block));
}

size_t node_value_stride(const mshio::NodeBlock& block)
{
    // MshIO stores 3 coordinates, plus one parameter per entity dimension when parametric == 1.
    if (block.parametric == 0) return 3;
    la_runtime_assert(block.entity_dim >= 0, "Negative node entity dimension");
    return static_cast<size_t>(3 + block.entity_dim);
}

/**
 * @private
 *
 * Extract vertex positions from msh spec.
 *
 * Point, curve, and surface nodes are loaded regardless of element references. Vertices are
 * inserted in tag order so a tag is not confused with its position in the file.
 */
template <typename Scalar, typename Index>
void extract_vertices(
    const mshio::MshSpec& spec,
    SurfaceMesh<Scalar, Index>& mesh,
    MshLoadIndex<Index>& index)
{
    struct NodeRecord
    {
        size_t tag = 0;
        Scalar x = 0;
        Scalar y = 0;
        Scalar z = 0;
        Scalar u = 0;
        Scalar v = 0;
        bool has_uv = false;
    };
    std::vector<NodeRecord> records;

    for (const auto& node_block : spec.nodes.entity_blocks) {
        const bool curve_or_surface = node_block.entity_dim == 1 || node_block.entity_dim == 2;
        const bool point = node_block.entity_dim == 0;
        if (!curve_or_surface && !point) {
            logger().warn("Skipping vertex block with entity dimension {}.", node_block.entity_dim);
            continue;
        }

        const size_t stride = node_value_stride(node_block);
        la_runtime_assert(
            node_block.tags.size() >= node_block.num_nodes_in_block,
            "Node block has fewer tags than nodes");
        la_runtime_assert(
            node_block.data.size() >= node_block.num_nodes_in_block * stride,
            "Node block is shorter than its entity dimension requires");

        for (size_t node = 0; node < node_block.num_nodes_in_block; ++node) {
            const size_t tag = node_block.tags[node];
            NodeRecord record;
            record.tag = tag;
            const size_t base = node * stride;
            record.x = static_cast<Scalar>(node_block.data[base]);
            record.y = static_cast<Scalar>(node_block.data[base + 1]);
            record.z = static_cast<Scalar>(node_block.data[base + 2]);
            // Only a parametric surface carries a 2D UV. A curve has one parameter, not a UV.
            if (node_block.parametric != 0 && node_block.entity_dim == 2) {
                record.u = static_cast<Scalar>(node_block.data[base + 3]);
                record.v = static_cast<Scalar>(node_block.data[base + 4]);
                record.has_uv = true;
            }
            records.push_back(record);
        }
    }

    std::stable_sort(records.begin(), records.end(), [](const NodeRecord& a, const NodeRecord& b) {
        return a.tag < b.tag;
    });

    std::vector<Scalar> positions;
    std::vector<Scalar> uvs;
    bool any_uv = false;
    positions.reserve(records.size() * 3);
    for (const NodeRecord& record : records) {
        if (index.node_tag_to_vertex.find(record.tag) != index.node_tag_to_vertex.end()) {
            logger().warn("Duplicate node tag {}; keeping the first.", record.tag);
            continue;
        }
        const Index vertex = static_cast<Index>(index.node_tag_to_vertex.size());
        index.node_tag_to_vertex.emplace(record.tag, vertex);
        positions.push_back(record.x);
        positions.push_back(record.y);
        positions.push_back(record.z);
        any_uv = any_uv || record.has_uv;
    }
    if (positions.empty()) return;

    const Index num_vertices = static_cast<Index>(positions.size() / 3);
    mesh.add_vertices(num_vertices, positions);
    if (!any_uv) return;

    // Nonparametric nodes, including curve nodes mixed into a parametric surface, get (0, 0).
    uvs.assign(static_cast<size_t>(num_vertices) * 2, Scalar(0));
    for (const NodeRecord& record : records) {
        if (!record.has_uv) continue;
        const auto it = index.node_tag_to_vertex.find(record.tag);
        if (it == index.node_tag_to_vertex.end()) continue;
        uvs[static_cast<size_t>(it->second) * 2] = record.u;
        uvs[static_cast<size_t>(it->second) * 2 + 1] = record.v;
    }
    mesh.template create_attribute<Scalar>(
        AttributeName::texcoord,
        AttributeElement::Vertex,
        AttributeUsage::UV,
        2,
        uvs);
}

template <typename Index>
struct PendingLine
{
    size_t element_tag = 0;
    Index v0 = 0;
    Index v1 = 0;
};

/**
 * @private
 *
 * Extract surface facets from msh spec. Line segments are returned so they can be attached after
 * surface-boundary stitching. A line on a surface edge would otherwise make that edge non-boundary.
 */
template <typename Scalar, typename Index>
std::vector<PendingLine<Index>> extract_facets(
    const mshio::MshSpec& spec,
    SurfaceMesh<Scalar, Index>& mesh,
    MshLoadIndex<Index>& index,
    bool load_lines,
    bool defer_lines)
{
    std::vector<Index> facet_sizes;
    std::vector<Index> facet_indices;
    std::vector<PendingLine<Index>> lines;
    std::unordered_set<size_t> seen_tags;
    for (const auto& element_block : spec.elements.entity_blocks) {
        if (is_line_element_block(element_block) && !load_lines) continue;
        if (!is_loaded_element_block(element_block, load_lines)) {
            logger().warn(
                "Skipping element block with entity dimension {} and element type {}.",
                element_block.entity_dim,
                element_block.element_type);
            continue;
        }
        const size_t nodes_per_element = mshio::nodes_per_element(element_block.element_type);
        const size_t stride = nodes_per_element + 1;
        la_runtime_assert(
            element_block.data.size() >= element_block.num_elements_in_block * stride,
            "Element block is shorter than its element type requires");
        const bool is_line = is_line_element_block(element_block);

        for (size_t element = 0; element < element_block.num_elements_in_block; ++element) {
            const size_t element_tag = element_block.data[element * stride];
            if (!seen_tags.insert(element_tag).second) {
                logger().warn("Duplicate element tag {}; keeping the first.", element_tag);
                continue;
            }

            std::vector<Index> vertices;
            vertices.reserve(nodes_per_element);
            bool missing_node = false;
            for (size_t node = 0; node < nodes_per_element; ++node) {
                const size_t node_tag = element_block.data[element * stride + 1 + node];
                const auto it = index.node_tag_to_vertex.find(node_tag);
                if (it == index.node_tag_to_vertex.end()) {
                    logger().warn(
                        "Skipping element {} because node tag {} was not loaded.",
                        element_tag,
                        node_tag);
                    missing_node = true;
                    break;
                }
                vertices.push_back(it->second);
            }
            if (missing_node) continue;

            if (is_line && defer_lines) {
                lines.push_back(PendingLine<Index>{element_tag, vertices[0], vertices[1]});
                continue;
            }

            index.element_tag_to_facet.emplace(element_tag, static_cast<Index>(facet_sizes.size()));
            facet_sizes.push_back(static_cast<Index>(nodes_per_element));
            facet_indices.insert(facet_indices.end(), vertices.begin(), vertices.end());
        }
    }
    if (!facet_sizes.empty()) {
        mesh.add_hybrid(facet_sizes, facet_indices);
    }
    return lines;
}

template <typename Scalar, typename Index>
void append_lines(
    SurfaceMesh<Scalar, Index>& mesh,
    const std::vector<PendingLine<Index>>& lines,
    span<const size_t> old_to_new,
    MshLoadIndex<Index>& index)
{
    if (lines.empty()) return;
    const Index facet_begin = mesh.get_num_facets();
    std::vector<Index> facet_sizes(lines.size(), Index(2));
    std::vector<Index> facet_indices;
    facet_indices.reserve(lines.size() * 2);
    for (size_t i = 0; i < lines.size(); ++i) {
        la_runtime_assert(lines[i].v0 < static_cast<Index>(old_to_new.size()));
        la_runtime_assert(lines[i].v1 < static_cast<Index>(old_to_new.size()));
        facet_indices.push_back(static_cast<Index>(old_to_new[lines[i].v0]));
        facet_indices.push_back(static_cast<Index>(old_to_new[lines[i].v1]));
        index.element_tag_to_facet.emplace(
            lines[i].element_tag,
            facet_begin + static_cast<Index>(i));
    }
    mesh.add_hybrid(facet_sizes, facet_indices);
}

template <typename Scalar>
bool write_attribute_values(
    span<Scalar> buffer,
    size_t destination,
    const std::vector<double>& values)
{
    if (destination + values.size() > buffer.size()) return false;
    for (size_t i = 0; i < values.size(); ++i) {
        buffer[destination + i] = static_cast<Scalar>(values[i]);
    }
    return true;
}

/**
 * @private
 *
 * Extract attribute from a data section in the spec.
 *
 * Entries are placed by node or element tag. A section may cover only some elements; unspecified
 * corners stay at the attribute default.
 */
template <typename Scalar, typename Index>
void extract_attribute(
    const mshio::Data& data,
    SurfaceMesh<Scalar, Index>& mesh,
    AttributeElement element_type,
    const LoadOptions& options,
    const MshLoadIndex<Index>& index,
    bool warn_unknown_tags)
{
    la_runtime_assert(data.header.string_tags.size() > 0);
    la_runtime_assert(data.header.int_tags.size() > 2);
    const auto& attr_name = data.header.string_tags[0];
    const int num_fields = data.header.int_tags[1];
    const int num_entries = data.header.int_tags[2];
    la_runtime_assert(num_entries == static_cast<int>(data.entries.size()));
    la_runtime_assert(num_fields >= 0, "Attribute field count is negative");

    // Determine attribute usage from attribute name prefix.
    AttributeUsage usage = AttributeUsage::Scalar;
    for (auto special_usage : {AttributeUsage::Normal, AttributeUsage::UV, AttributeUsage::Color}) {
        if (starts_with(
                attr_name,
                format(
                    "{}_{}",
                    internal::to_string(element_type),
                    internal::to_string(special_usage)))) {
            usage = special_usage;
        }
    }
    switch (usage) {
    case AttributeUsage::Normal:
        if (!options.load_normals) return;
        break;
    case AttributeUsage::UV:
        if (!options.load_uvs) return;
        break;
    case AttributeUsage::Color:
        if (element_type == AttributeElement::Vertex && !options.load_vertex_colors) return;
        break;
    default: break;
    }
    if (usage == AttributeUsage::Scalar && num_fields > 1) {
        usage = AttributeUsage::Vector;
    }

    AttributeId id;
    if (mesh.has_attribute(attr_name)) {
        id = mesh.get_attribute_id(attr_name);
        la_runtime_assert(mesh.template is_attribute_type<Scalar>(id));
        la_runtime_assert(mesh.get_attribute_base(id).get_element_type() == element_type);
    } else {
        id = mesh.template create_attribute<Scalar>(attr_name, element_type, usage, num_fields);
    }
    auto& attr = mesh.template ref_attribute<Scalar>(id);
    auto buffer = attr.ref_all();
    const size_t fields = static_cast<size_t>(num_fields);

    for (const auto& entry : data.entries) {
        if (element_type == AttributeElement::Vertex) {
            const auto it = index.node_tag_to_vertex.find(entry.tag);
            if (it == index.node_tag_to_vertex.end()) {
                if (warn_unknown_tags) {
                    logger().warn("Skipping node data for unknown tag {}.", entry.tag);
                }
                continue;
            }
            if (entry.data.size() != fields) {
                logger().warn(
                    "Skipping node data for tag {} with an unexpected field count.",
                    entry.tag);
                continue;
            }
            const size_t destination = static_cast<size_t>(it->second) * fields;
            la_runtime_assert(
                write_attribute_values(buffer, destination, entry.data),
                "Node data exceeds the vertex count");
        } else if (element_type == AttributeElement::Facet) {
            const auto it = index.element_tag_to_facet.find(entry.tag);
            if (it == index.element_tag_to_facet.end()) {
                if (warn_unknown_tags) {
                    logger().warn("Skipping element data for unknown tag {}.", entry.tag);
                }
                continue;
            }
            if (entry.data.size() != fields) {
                logger().warn(
                    "Skipping element data for tag {} with an unexpected field count.",
                    entry.tag);
                continue;
            }
            const size_t destination = static_cast<size_t>(it->second) * fields;
            la_runtime_assert(
                write_attribute_values(buffer, destination, entry.data),
                "Element data exceeds the facet count");
        } else {
            const auto it = index.element_tag_to_facet.find(entry.tag);
            if (it == index.element_tag_to_facet.end()) {
                if (warn_unknown_tags) {
                    logger().warn("Skipping element-node data for unknown tag {}.", entry.tag);
                }
                continue;
            }
            const Index facet = it->second;
            const Index facet_size = mesh.get_facet_size(facet);
            if (entry.num_nodes_per_element != static_cast<int>(facet_size)) {
                logger().warn(
                    "Skipping element-node data for tag {} because its node count does not match "
                    "the facet.",
                    entry.tag);
                continue;
            }
            const size_t value_count = static_cast<size_t>(facet_size) * fields;
            if (entry.data.size() != value_count) {
                logger().warn(
                    "Skipping element-node data for tag {} with an unexpected field count.",
                    entry.tag);
                continue;
            }
            const size_t destination =
                static_cast<size_t>(mesh.get_facet_corner_begin(facet)) * fields;
            la_runtime_assert(
                write_attribute_values(buffer, destination, entry.data),
                "Element-node data exceeds the mesh corner count");
        }
    }
}

/**
 * @private
 */
template <typename Scalar, typename Index>
void extract_vertex_attributes(
    const mshio::MshSpec& spec,
    SurfaceMesh<Scalar, Index>& mesh,
    const LoadOptions& options,
    const MshLoadIndex<Index>& index,
    bool warn_unknown_tags)
{
    for (const auto& data : spec.node_data) {
        extract_attribute(data, mesh, AttributeElement::Vertex, options, index, warn_unknown_tags);
    }
}

/**
 * @private
 */
template <typename Scalar, typename Index>
void extract_facet_attributes(
    const mshio::MshSpec& spec,
    SurfaceMesh<Scalar, Index>& mesh,
    const LoadOptions& options,
    const MshLoadIndex<Index>& index,
    bool warn_unknown_tags)
{
    for (const auto& data : spec.element_data) {
        extract_attribute(data, mesh, AttributeElement::Facet, options, index, warn_unknown_tags);
    }
}

/**
 * @private
 */
template <typename Scalar, typename Index>
void extract_corner_attributes(
    const mshio::MshSpec& spec,
    SurfaceMesh<Scalar, Index>& mesh,
    const LoadOptions& options,
    const MshLoadIndex<Index>& index,
    bool warn_unknown_tags)
{
    for (const auto& data : spec.element_node_data) {
        extract_attribute(data, mesh, AttributeElement::Corner, options, index, warn_unknown_tags);
    }
}

} // namespace

template <typename MeshType>
MeshType load_mesh_msh(std::istream& input_stream, const LoadOptions& options)
{
    using Index = typename MeshType::Index;
    mshio::MshSpec spec = mshio::load_msh(input_stream);
    MeshType mesh;
    MshLoadIndex<Index> index;
    extract_vertices(spec, mesh, index);
    auto lines = extract_facets(spec, mesh, index, options.load_lines, options.stitch_vertices);
    extract_vertex_attributes(spec, mesh, options, index, true);
    if (options.stitch_vertices) {
        // Surface attributes must exist before stitching so they follow the remapped facets.
        extract_facet_attributes(spec, mesh, options, index, false);
        extract_corner_attributes(spec, mesh, options, index, false);
        std::vector<size_t> old_to_new(static_cast<size_t>(mesh.get_num_vertices()));
        // Stitch the surface before lines exist, so a boundary line does not hide the seam.
        // Defer welding until line corners point at the original vertex value rows.
        const bool defer_weld = !lines.empty();
        stitch_mesh(mesh, {old_to_new.data(), old_to_new.size()}, !defer_weld);
        if (defer_weld) {
            const Index corner_begin = mesh.get_num_corners();
            append_lines(mesh, lines, {old_to_new.data(), old_to_new.size()}, index);
            std::vector<Index> value_rows;
            value_rows.reserve(lines.size() * 2);
            for (const auto& line : lines) {
                value_rows.push_back(line.v0);
                value_rows.push_back(line.v1);
            }
            assign_indexed_corner_indices(
                mesh,
                corner_begin,
                {value_rows.data(), value_rows.size()});
            weld_indexed_attributes(mesh);
        }
    }
    extract_facet_attributes(spec, mesh, options, index, true);
    extract_corner_attributes(spec, mesh, options, index, true);
    if (options.triangulate) {
        // LoadOptions::triangulate only promises to remove facets with more than 3 vertices.
        TriangulationOptions tri_options;
        tri_options.preserve_edges = true;
        triangulate_polygonal_facets(mesh, tri_options);
    }

    return mesh;
}

template <typename MeshType>
MeshType load_mesh_msh(const fs::path& filename, const LoadOptions& options)
{
    fs::ifstream fin(filename, std::ios::binary);
    la_runtime_assert(fin.good(), format("Unable to open file {}", filename.string()));
    return load_mesh_msh<MeshType>(fin, options);
}

#define LA_X_load_mesh_msh(_, S, I)                                                                \
    template LA_IO_API SurfaceMesh<S, I> load_mesh_msh(std::istream&, const LoadOptions& options); \
    template LA_IO_API SurfaceMesh<S, I> load_mesh_msh(                                            \
        const fs::path& filename,                                                                  \
        const LoadOptions& options);
LA_SURFACE_MESH_X(load_mesh_msh, 0)
#undef LA_X_load_mesh_msh

} // namespace lagrange::io
