/*
 * Copyright 2023 Adobe. All rights reserved.
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
#include <lagrange/extract_submesh.h>
#include <lagrange/foreach_attribute.h>
#include <lagrange/internal/map_attributes.h>
#include <lagrange/views.h>

#include <algorithm>
#include <vector>

namespace lagrange {

template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> extract_submesh(
    const SurfaceMesh<Scalar, Index>& mesh,
    span<const Index> selected_facets,
    const SubmeshOptions& options)
{
    SurfaceMesh<Scalar, Index> output_mesh(mesh.get_dimension());

    // Compute vertex mapping.
    const auto num_vertices = mesh.get_num_vertices();
    std::vector<Index> vertex_old2new(num_vertices, invalid<Index>());
    std::vector<Index> vertex_new2old;
    vertex_new2old.reserve(num_vertices);

    for (auto fid : selected_facets) {
        auto f = mesh.get_facet_vertices(fid);
        for (Index vid : f) {
            if (vertex_old2new[vid] == invalid<Index>()) {
                vertex_old2new[vid] = static_cast<Index>(vertex_new2old.size());
                vertex_new2old.push_back(vid);
            }
        }
    }

    // Map vertices.
    Index num_output_vertices = static_cast<Index>(vertex_new2old.size());
    output_mesh.add_vertices(num_output_vertices);

    auto in_vertices = vertex_view(mesh);
    auto out_vertices = vertex_ref(output_mesh);
    for (Index i = 0; i < num_output_vertices; i++) {
        out_vertices.row(i) = in_vertices.row(vertex_new2old[i]);
    }

    // Map facets
    Index num_output_facets = static_cast<Index>(selected_facets.size());
    if (!selected_facets.empty()) {
        if (mesh.is_regular()) {
            Index vertex_per_facet = mesh.get_facet_size(selected_facets.front());
            output_mesh.add_polygons(num_output_facets, vertex_per_facet);
            auto in_facets = facet_view(mesh);
            auto out_facets = facet_ref(output_mesh);
            for (Index i = 0; i < num_output_facets; i++) {
                out_facets.row(i) = in_facets.row(selected_facets[i]);
            }
            std::transform(
                out_facets.data(),
                out_facets.data() + num_output_facets * vertex_per_facet,
                out_facets.data(),
                [&](Index i) { return vertex_old2new[i]; });
        } else {
            output_mesh.add_hybrid(
                num_output_facets,
                [&](Index fid) { return mesh.get_facet_size(selected_facets[fid]); },
                [&](Index fid, span<Index> f) {
                    auto old_f = mesh.get_facet_vertices(selected_facets[fid]);
                    la_debug_assert(f.size() == old_f.size());
                    std::transform(old_f.begin(), old_f.end(), f.begin(), [&](Index vid) {
                        return vertex_old2new[vid];
                    });
                });

            output_mesh.compress_if_regular();
        }
    }

    if (!options.source_vertex_attr_name.empty()) {
        output_mesh.template create_attribute<Index>(
            options.source_vertex_attr_name,
            AttributeElement::Vertex,
            AttributeUsage::Scalar,
            1,
            {vertex_new2old.data(), static_cast<size_t>(num_output_vertices)});
    }
    if (!options.source_facet_attr_name.empty()) {
        output_mesh.template create_attribute<Index>(
            options.source_facet_attr_name,
            AttributeElement::Facet,
            AttributeUsage::Scalar,
            1,
            selected_facets);
    }

    if (options.map_attributes) {
        // Map vertex attributes.
        internal::map_attributes<Vertex>(
            mesh,
            output_mesh,
            {vertex_new2old.data(), vertex_new2old.size()});

        // Map facet attributes.
        internal::map_attributes<Facet>(mesh, output_mesh, selected_facets);

        // Map corner attributes.
        const Index num_corners = output_mesh.get_num_corners();
        std::vector<Index> corner_mapping(num_corners);
        for (Index ci = 0; ci < num_corners; ci++) {
            Index fi = output_mesh.get_corner_facet(ci);
            Index lv = ci - output_mesh.get_facet_corner_begin(fi);
            Index source_fi = selected_facets[fi];
            Index source_ci = mesh.get_facet_corner_begin(source_fi) + lv;
            corner_mapping[ci] = source_ci;
        }
        internal::map_attributes<Corner>(
            mesh,
            output_mesh,
            {corner_mapping.data(), corner_mapping.size()});

        // Map indexed attributes.
        seq_foreach_named_attribute_read<Indexed>(mesh, [&](std::string_view name, auto&& attr) {
            using AttributeType = std::decay_t<decltype(attr)>;
            using ValueType = typename AttributeType::ValueType;
            if (output_mesh.attr_name_is_reserved(name)) return;

            const auto& values = attr.values();
            const auto& indices = attr.indices();

            auto id = output_mesh.template create_attribute<ValueType>(
                name,
                Indexed,
                attr.get_usage(),
                attr.get_num_channels());
            auto& target_attr = output_mesh.template ref_indexed_attribute<ValueType>(id);
            target_attr.values() = values;
            auto& target_indices = target_attr.indices();
            for (Index ci = 0; ci < num_corners; ci++) {
                target_indices.ref(ci) = indices.get(corner_mapping[ci]);
            }
        });

        // Edge attributes
        {
            bool has_edge_attr = false;
            seq_foreach_attribute_read<AttributeElement::Edge>(mesh, [&](auto&&) {
                has_edge_attr = true;
            });
            if (has_edge_attr) {
                logger().warn("`extract_submesh`: Edge attributes remapping is not supported.");
            }
        }
    }

    return output_mesh;
}


#define LA_X_extract_submesh(_, Scalar, Index)                       \
    template LA_CORE_API SurfaceMesh<Scalar, Index> extract_submesh( \
        const SurfaceMesh<Scalar, Index>&,                           \
        span<const Index>,                                           \
        const SubmeshOptions&);

LA_SURFACE_MESH_X(extract_submesh, 0)

} // namespace lagrange
