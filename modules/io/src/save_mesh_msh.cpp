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
#include <lagrange/io/save_mesh_msh.h>
#include <lagrange/utils/Error.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/range.h>

#include <mshio/mshio.h>

namespace lagrange {
namespace io {

namespace {

template <typename Scalar, typename Index>
void populate_nodes(mshio::MshSpec& spec, const SurfaceMesh<Scalar, Index>& mesh)
{
    const Index dim = mesh.get_dimension();
    la_runtime_assert(dim == 3, "Only 3D meshes are supported!");

    const Index num_vertices = mesh.get_num_vertices();
    spec.nodes.num_entity_blocks = 1;
    spec.nodes.num_nodes = num_vertices;
    spec.nodes.min_node_tag = 1;
    spec.nodes.max_node_tag = num_vertices;
    spec.nodes.entity_blocks.emplace_back();

    auto& node_block = spec.nodes.entity_blocks.front();
    node_block.entity_dim = 2; // Encoding surfaces.
    node_block.entity_tag = 1;
    node_block.parametric = 0; // TODO for UV.
    node_block.num_nodes_in_block = num_vertices;
    node_block.data.reserve(dim * num_vertices);
    node_block.tags.reserve(num_vertices);
    for (Index i = 0; i < num_vertices; i++) {
        auto p = mesh.get_position(i);
        node_block.tags.push_back(i + 1);
        node_block.data.push_back(p[0]);
        node_block.data.push_back(p[1]);
        node_block.data.push_back(p[2]);
    }
}

template <typename Scalar, typename Index>
void populate_elements(mshio::MshSpec& spec, const SurfaceMesh<Scalar, Index>& mesh)
{
    bool is_tri_mesh = mesh.is_triangle_mesh();
    const Index vertex_per_facet = mesh.get_vertex_per_facet();

    const Index num_facets = mesh.get_num_facets();
    spec.elements.num_entity_blocks = 1;
    spec.elements.num_elements = num_facets;
    spec.elements.min_element_tag = 1;
    spec.elements.max_element_tag = num_facets;
    spec.elements.entity_blocks.emplace_back();

    auto& element_block = spec.elements.entity_blocks.front();
    element_block.entity_dim = 2;
    element_block.entity_tag = 1;
    element_block.element_type = is_tri_mesh ? 2 : 3;
    element_block.num_elements_in_block = num_facets;
    element_block.data.reserve((vertex_per_facet + 1) * num_facets);
    for (Index i = 0; i < num_facets; i++) {
        element_block.data.push_back(i + 1); // element tag.
        for (Index j = 0; j < vertex_per_facet; j++) {
            element_block.data.push_back(mesh.get_facet_vertex(i, j) + 1);
        }
    }
}

template <typename Scalar, typename Index>
void populate_indexed_attribute(
    mshio::MshSpec& /*spec*/,
    const SurfaceMesh<Scalar, Index>& mesh,
    AttributeId id)
{
    // TODO add support. 
    // can reference save_gltf. gltf also does not support indexed attributes. 
    std::string_view name = mesh.get_attribute_name(id);
    logger().warn("Skipping attribute {}: unsupported non-indexed", name);
}

template <typename Scalar, typename Index, typename Value>
void populate_non_indexed_vertex_attribute(
    mshio::MshSpec& spec,
    const SurfaceMesh<Scalar, Index>& mesh,
    AttributeId id)
{
    la_debug_assert(mesh.template is_attribute_type<Value>(id));
    const auto& attr = mesh.template get_attribute<Value>(id);
    assert(attr.get_element_type() == AttributeElement::Vertex);
    const auto name = mesh.get_attribute_name(id);

    const auto num_vertices = mesh.get_num_vertices();
    const auto num_channels = attr.get_num_channels();

    spec.node_data.emplace_back();
    auto& node_data = spec.node_data.back();
    node_data.header.string_tags.emplace_back(name);
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
        entry.tag = static_cast<size_t>(i + 1);
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
    AttributeId id)
{
    la_debug_assert(mesh.template is_attribute_type<Value>(id));
    const auto& attr = mesh.template get_attribute<Value>(id);
    assert(attr.get_element_type() == AttributeElement::Facet);
    const auto name = mesh.get_attribute_name(id);

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
        entry.tag = static_cast<size_t>(i + 1);
        entry.data.reserve(num_channels);

        for (auto j : range(num_channels)) {
            entry.data.push_back(static_cast<double>(attr.get(i, j)));
        }
    }
}

template <typename Scalar, typename Index, typename Value>
void populate_non_indexed_edge_attribute(
    mshio::MshSpec& /*spec*/,
    const SurfaceMesh<Scalar, Index>& /*mesh*/,
    AttributeId /*id*/)
{
    throw Error("Saving edge attribute in MSH format is not yet supported.");
}

template <typename Scalar, typename Index, typename Value>
void populate_non_indexed_corner_attribute(
    mshio::MshSpec& spec,
    const SurfaceMesh<Scalar, Index>& mesh,
    AttributeId id)
{
    la_debug_assert(mesh.template is_attribute_type<Value>(id));
    const auto& attr = mesh.template get_attribute<Value>(id);
    assert(attr.get_element_type() == AttributeElement::Corner);
    const auto name = mesh.get_attribute_name(id);

    const auto num_facets = mesh.get_num_facets();
    const auto vertex_per_facet = mesh.get_vertex_per_facet();
    assert(mesh.get_num_corners() == num_facets * vertex_per_facet);
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
        element_node_data.entries.emplace_back();
        auto& entry = element_node_data.entries.back();
        entry.tag = static_cast<size_t>(i + 1);
        entry.num_nodes_per_element = int(vertex_per_facet);
        entry.data.reserve(num_channels * vertex_per_facet);
        for (auto j : range(vertex_per_facet)) {
            for (auto k : range(num_channels)) {
                entry.data.push_back(static_cast<double>(attr.get(i * vertex_per_facet + j, k)));
            }
        }
    }
}

template <typename Scalar, typename Index>
void populate_non_indexed_attribute(
    mshio::MshSpec& spec,
    const SurfaceMesh<Scalar, Index>& mesh,
    AttributeId id)
{
    la_runtime_assert(!mesh.is_attribute_indexed(id));
    const auto& attr_base = mesh.get_attribute_base(id);

    switch (attr_base.get_element_type()) {
    case AttributeElement::Vertex: {
#define LA_X_try_attribute(_, T)                \
    if (mesh.template is_attribute_type<T>(id)) \
        populate_non_indexed_vertex_attribute<Scalar, Index, T>(spec, mesh, id);
        LA_ATTRIBUTE_X(try_attribute, 0)
#undef LA_X_try_attribute
        break;
    }
    case AttributeElement::Facet: {
#define LA_X_try_attribute(_, T)                \
    if (mesh.template is_attribute_type<T>(id)) \
        populate_non_indexed_facet_attribute<Scalar, Index, T>(spec, mesh, id);
        LA_ATTRIBUTE_X(try_attribute, 0)
#undef LA_X_try_attribute
        break;
    }
    case AttributeElement::Edge: {
#define LA_X_try_attribute(_, T)                \
    if (mesh.template is_attribute_type<T>(id)) \
        populate_non_indexed_edge_attribute<Scalar, Index, T>(spec, mesh, id);
        LA_ATTRIBUTE_X(try_attribute, 0)
#undef LA_X_try_attribute
        break;
    }
    case AttributeElement::Corner: {
#define LA_X_try_attribute(_, T)                \
    if (mesh.template is_attribute_type<T>(id)) \
        populate_non_indexed_corner_attribute<Scalar, Index, T>(spec, mesh, id);
        LA_ATTRIBUTE_X(try_attribute, 0)
#undef LA_X_try_attribute
        break;
    }
    default: throw Error("Unsupported attribute element type!"); break;
    }
}

template <typename Scalar, typename Index>
void populate_attribute(
    mshio::MshSpec& spec,
    const SurfaceMesh<Scalar, Index>& mesh,
    AttributeId id)
{
    if (mesh.is_attribute_indexed(id)) {
        populate_indexed_attribute(spec, mesh, id);
    } else {
        populate_non_indexed_attribute(spec, mesh, id);
    }
}

} // namespace

template <typename Scalar, typename Index>
void save_mesh_msh(
    std::ostream& output_stream,
    const SurfaceMesh<Scalar, Index>& mesh,
    const SaveOptions& options)
{
    if constexpr (sizeof(size_t) != 8) {
        logger().error("MSH format requries `size_t` to be 8 bytes!");
        throw Error("Incompatible size_t size detected!");
    }

    la_runtime_assert(output_stream, "Invalid output stream");
    la_runtime_assert(
        mesh.is_triangle_mesh() || mesh.is_quad_mesh(),
        "Only triangle and quad mesh are supported for now.");

    mshio::MshSpec spec;
    spec.mesh_format.file_type = options.encoding == FileEncoding::Binary ? 1 : 0;
    populate_nodes(spec, mesh);
    populate_elements(spec, mesh);

    std::vector<AttributeId> ids;
    if (options.output_attributes == SaveOptions::OutputAttributes::All) {
        mesh.seq_foreach_attribute_id([&](std::string_view name, AttributeId id) {
            if (!mesh.attr_name_is_reserved(name)) {
                ids.push_back(id);
            }
        });
        ids.push_back(mesh.attr_id_vertex_to_positions());
        ids.push_back(mesh.attr_id_corner_to_vertex());
    } else {
        ids = options.selected_attributes;
    }

    for (auto id : ids) {
        populate_attribute(spec, mesh, id);
    }

    mshio::save_msh(output_stream, spec);
}

template <typename Scalar, typename Index>
void save_mesh_msh(
    const fs::path& filename,
    const SurfaceMesh<Scalar, Index>& mesh,
    const SaveOptions& options)
{
    fs::ofstream fout(filename);
    save_mesh_msh(fout, mesh, options);
}

#define LA_X_save_mesh_msh(_, S, I)    \
    template void save_mesh_msh(       \
        const fs::path& filename,      \
        const SurfaceMesh<S, I>& mesh, \
        const SaveOptions& options);
LA_SURFACE_MESH_X(save_mesh_msh, 0)
#undef LA_X_save_mesh_msh

} // namespace io
} // namespace lagrange
