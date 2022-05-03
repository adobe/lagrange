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
#include <lagrange/io/load_mesh_msh.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/range.h>
#include <lagrange/utils/invalid.h>

#include <mshio/mshio.h>

#include <algorithm>

namespace lagrange::io {

/**
 * @private
 *
 * Extract vertex positions from msh spec.
 */
template <typename Scalar, typename Index>
void extract_vertices(const mshio::MshSpec& spec, SurfaceMesh<Scalar, Index>& mesh)
{
    for (const auto& node_block : spec.nodes.entity_blocks) {
        if (node_block.entity_dim != 2) {
            logger().warn("Skipping non-surface vertex blocks.");
            continue; // Surface mesh only.
        }

        if constexpr (std::is_same<double, Scalar>::value) {
            mesh.add_vertices(node_block.num_nodes_in_block, node_block.data);
        } else {
            mesh.add_vertices(node_block.num_nodes_in_block, [&](Index i, span<Scalar> buffer) {
                buffer[0] = static_cast<Scalar>(node_block.data[i * 3]);
                buffer[1] = static_cast<Scalar>(node_block.data[i * 3 + 1]);
                buffer[2] = static_cast<Scalar>(node_block.data[i * 3 + 2]);
            });
        }
    }
}

/**
 * @private
 *
 * Extract facets from msh spec.
 */
template <typename Scalar, typename Index>
void extract_facets(const mshio::MshSpec& spec, SurfaceMesh<Scalar, Index>& mesh)
{
    for (const auto& element_block : spec.elements.entity_blocks) {
        if (element_block.entity_dim != 2) {
            logger().warn("Skipping non-surface element blocks.");
            continue; // Surface mesh only.
        }
        size_t nodes_per_element = mshio::nodes_per_element(element_block.element_type);

        mesh.add_polygons(
            element_block.num_elements_in_block,
            static_cast<Index>(nodes_per_element),
            [&](Index fid, span<Index> f) {
                std::copy_n(
                    element_block.data.begin() + fid * (nodes_per_element + 1) + 1,
                    nodes_per_element,
                    f.begin());
            });
    }
}

/**
 * @private
 *
 * Extract attribute from a data section in the spec.
 */
template <typename Scalar, typename Index>
void extract_attribute(
    const mshio::Data& data,
    SurfaceMesh<Scalar, Index>& mesh,
    AttributeElement element_type)
{
    la_runtime_assert(data.header.string_tags.size() > 0);
    la_runtime_assert(data.header.int_tags.size() > 2);
    const auto& attr_name = data.header.string_tags[0];
    const int num_fields = data.header.int_tags[1];
    const int num_entries = data.header.int_tags[2];
    la_runtime_assert(num_entries == static_cast<int>(data.entries.size()));

    AttributeUsage usage = AttributeUsage::Scalar;
    if (attr_name == "@normal") {
        usage = AttributeUsage::Normal;
    } else if (attr_name == "@uv") {
        usage = AttributeUsage::UV;
    } else if (attr_name == "@color") {
        usage = AttributeUsage::Color;
    } else if (num_fields > 1) {
        usage = AttributeUsage::Vector;
    }

    auto id = mesh.template create_attribute<Scalar>(attr_name, element_type, usage, num_fields);
    auto& attr = mesh.template ref_attribute<Scalar>(id);
    auto buffer = attr.ref_all();
    int num_nodes_per_element = invalid<int>();

    for (auto i : range(num_entries)) {
        const auto& entry = data.entries[i];
        la_debug_assert(i + 1 == static_cast<int>(entry.tag));
        if (element_type != AttributeElement::Corner) {
            // Node or element data.
            std::copy_n(entry.data.begin(), num_fields, buffer.begin() + i * num_fields);
        } else {
            // Node-element data.
            if (num_nodes_per_element == invalid<int>()) {
                num_nodes_per_element = entry.num_nodes_per_element;
            } else if (num_nodes_per_element != entry.num_nodes_per_element) {
                throw Error("Invalid mixed element detected in node-element data.");
            }

            std::copy_n(
                entry.data.begin(),
                num_nodes_per_element * num_fields,
                buffer.begin() + i * num_nodes_per_element * num_fields);
        }
    }
}

/**
 * @private
 */
template <typename Scalar, typename Index>
void extract_vertex_attributes(const mshio::MshSpec& spec, SurfaceMesh<Scalar, Index>& mesh)
{
    for (const auto& data : spec.node_data) {
        extract_attribute(data, mesh, AttributeElement::Vertex);
    }
}

/**
 * @private
 */
template <typename Scalar, typename Index>
void extract_facet_attributes(const mshio::MshSpec& spec, SurfaceMesh<Scalar, Index>& mesh)
{
    for (const auto& data : spec.element_data) {
        extract_attribute(data, mesh, AttributeElement::Facet);
    }
}

/**
 * @private
 */
template <typename Scalar, typename Index>
void extract_corner_attributes(const mshio::MshSpec& spec, SurfaceMesh<Scalar, Index>& mesh)
{
    for (const auto& data : spec.element_node_data) {
        extract_attribute(data, mesh, AttributeElement::Corner);
    }
}

template <typename MeshType>
MeshType load_mesh_msh(std::istream& input_stream)
{
    mshio::MshSpec spec = mshio::load_msh(input_stream);
    MeshType mesh;

    extract_vertices(spec, mesh);
    extract_facets(spec, mesh);
    extract_vertex_attributes(spec, mesh);
    extract_facet_attributes(spec, mesh);
    extract_corner_attributes(spec, mesh);

    return mesh;
}

#define LA_X_load_mesh_msh(_, S, I) \
    template SurfaceMesh<S, I> load_mesh_msh(std::istream& input_stream);
LA_SURFACE_MESH_X(load_mesh_msh, 0)
#undef LA_X_load_mesh_msh


} // namespace lagrange::io

