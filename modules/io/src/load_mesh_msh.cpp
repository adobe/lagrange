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
#include <lagrange/utils/assert.h>
#include <lagrange/utils/invalid.h>
#include <lagrange/utils/range.h>
#include <lagrange/utils/strings.h>

#include <mshio/mshio.h>

#include <algorithm>

namespace lagrange::io {

namespace {

/**
 * @private
 *
 * Extract vertex positions from msh spec.
 */
template <typename Scalar, typename Index>
void extract_vertices(const mshio::MshSpec& spec, SurfaceMesh<Scalar, Index>& mesh)
{
    std::vector<Scalar> uvs;
    for (const auto& node_block : spec.nodes.entity_blocks) {
        if (node_block.entity_dim != 2) {
            logger().warn("Skipping non-surface vertex blocks.");
            continue; // Surface mesh only.
        }

        if (!node_block.parametric) {
            if constexpr (std::is_same<double, Scalar>::value) {
                mesh.add_vertices(
                    static_cast<Index>(node_block.num_nodes_in_block),
                    node_block.data);
            } else {
                if (!node_block.parametric) {
                    mesh.add_vertices(
                        static_cast<Index>(node_block.num_nodes_in_block),
                        [&](Index i, span<Scalar> buffer) {
                            buffer[0] = static_cast<Scalar>(node_block.data[i * 3]);
                            buffer[1] = static_cast<Scalar>(node_block.data[i * 3 + 1]);
                            buffer[2] = static_cast<Scalar>(node_block.data[i * 3 + 2]);
                        });
                }
            }
        } else {
            mesh.add_vertices(
                static_cast<Index>(node_block.num_nodes_in_block),
                [&](Index i, span<Scalar> buffer) {
                    buffer[0] = static_cast<Scalar>(node_block.data[i * 5]);
                    buffer[1] = static_cast<Scalar>(node_block.data[i * 5 + 1]);
                    buffer[2] = static_cast<Scalar>(node_block.data[i * 5 + 2]);
                    uvs.push_back(static_cast<Scalar>(node_block.data[i * 5 + 3]));
                    uvs.push_back(static_cast<Scalar>(node_block.data[i * 5 + 4]));
                });
        }
    }
    if (!uvs.empty()) {
        if (static_cast<Index>(uvs.size()) == mesh.get_num_vertices() * 2) {
            mesh.template create_attribute<Scalar>(
                AttributeName::texcoord,
                AttributeElement::Vertex,
                AttributeUsage::UV,
                2,
                uvs);
        } else {
            logger().warn("The number of uvs does not match the number of vertices");
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
        Index nodes_per_element =
            static_cast<Index>(mshio::nodes_per_element(element_block.element_type));

        mesh.add_polygons(
            static_cast<Index>(element_block.num_elements_in_block),
            nodes_per_element,
            [&](Index fid, span<Index> f) {
                Index offset = fid * (nodes_per_element + 1) + 1;
                std::transform(
                    element_block.data.begin() + offset,
                    element_block.data.begin() + offset + nodes_per_element,
                    f.begin(),
                    [](size_t vid) { return static_cast<Index>(vid - 1); });
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
    AttributeElement element_type,
    const LoadOptions& options)
{
    la_runtime_assert(data.header.string_tags.size() > 0);
    la_runtime_assert(data.header.int_tags.size() > 2);
    const auto& attr_name = data.header.string_tags[0];
    const int num_fields = data.header.int_tags[1];
    const int num_entries = data.header.int_tags[2];
    la_runtime_assert(num_entries == static_cast<int>(data.entries.size()));

    // Determine attribute usage from attribute name prefix.
    AttributeUsage usage = AttributeUsage::Scalar;
    for (auto special_usage : {AttributeUsage::Normal, AttributeUsage::UV, AttributeUsage::Color}) {
        if (starts_with(
                attr_name,
                fmt::format(
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

    auto id = mesh.template create_attribute<Scalar>(attr_name, element_type, usage, num_fields);
    auto& attr = mesh.template ref_attribute<Scalar>(id);
    auto buffer = attr.ref_all();
    int num_nodes_per_element = invalid<int>();

    for (auto i : range(num_entries)) {
        const auto& entry = data.entries[i];
        la_debug_assert(i + 1 == static_cast<int>(entry.tag));
        if (element_type != AttributeElement::Corner) {
            // Node or element data.
            std::transform(
                entry.data.begin(),
                entry.data.end(),
                buffer.begin() + i * num_fields,
                [](double val) { return static_cast<Scalar>(val); });
        } else {
            // Node-element data.
            if (num_nodes_per_element == invalid<int>()) {
                num_nodes_per_element = entry.num_nodes_per_element;
            } else if (num_nodes_per_element != entry.num_nodes_per_element) {
                throw Error("Invalid mixed element detected in node-element data.");
            }

            std::transform(
                entry.data.begin(),
                entry.data.end(),
                buffer.begin() + i * num_nodes_per_element * num_fields,
                [](double val) { return static_cast<Scalar>(val); });
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
    const LoadOptions& options)
{
    for (const auto& data : spec.node_data) {
        extract_attribute(data, mesh, AttributeElement::Vertex, options);
    }
}

/**
 * @private
 */
template <typename Scalar, typename Index>
void extract_facet_attributes(
    const mshio::MshSpec& spec,
    SurfaceMesh<Scalar, Index>& mesh,
    const LoadOptions& options)
{
    for (const auto& data : spec.element_data) {
        extract_attribute(data, mesh, AttributeElement::Facet, options);
    }
}

/**
 * @private
 */
template <typename Scalar, typename Index>
void extract_corner_attributes(
    const mshio::MshSpec& spec,
    SurfaceMesh<Scalar, Index>& mesh,
    const LoadOptions& options)
{
    for (const auto& data : spec.element_node_data) {
        extract_attribute(data, mesh, AttributeElement::Corner, options);
    }
}

} // namespace

template <typename MeshType>
MeshType load_mesh_msh(std::istream& input_stream, const LoadOptions& options)
{
    mshio::MshSpec spec = mshio::load_msh(input_stream);
    MeshType mesh;

    extract_vertices(spec, mesh);
    extract_facets(spec, mesh);
    extract_vertex_attributes(spec, mesh, options);
    extract_facet_attributes(spec, mesh, options);
    extract_corner_attributes(spec, mesh, options);

    return mesh;
}

template <typename MeshType>
MeshType load_mesh_msh(const fs::path& filename, const LoadOptions& options)
{
    fs::ifstream fin(filename, std::ios::binary);
    la_runtime_assert(fin.good(), fmt::format("Unable to open file {}", filename.string()));
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
