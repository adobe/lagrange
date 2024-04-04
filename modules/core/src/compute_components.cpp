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
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/compute_components.h>
#include <lagrange/utils/DisjointSets.h>
#include <lagrange/utils/Error.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/invalid.h>
#include <lagrange/utils/range.h>

#include <algorithm>
#include <numeric>

namespace lagrange {

template <typename Scalar, typename Index>
size_t compute_vertex_based_components(
    SurfaceMesh<Scalar, Index>& mesh,
    AttributeId id,
    span<const Index> blocker_vertices)
{
    constexpr Index invalid_index = invalid<Index>();
    auto& attr = mesh.template ref_attribute<Index>(id);
    auto component_id = attr.ref_all();
    const auto num_vertices = mesh.get_num_vertices();
    const auto num_facets = mesh.get_num_facets();
    la_debug_assert(component_id.size() == static_cast<size_t>(num_facets));

    std::vector<bool> is_blocked(num_vertices, false);
    std::for_each(blocker_vertices.begin(), blocker_vertices.end(), [&](Index vi) {
        is_blocked[vi] = true;
    });

    DisjointSets<Index> components(num_facets);
    for (auto vi : range(num_vertices)) {
        if (is_blocked[vi]) continue;
        Index rep_facet_id = invalid_index;
        for (Index ci = mesh.get_first_corner_around_vertex(vi); ci != invalid_index;
             ci = mesh.get_next_corner_around_vertex(ci)) {
            Index fi = mesh.get_corner_facet(ci);
            if (rep_facet_id == invalid_index) {
                rep_facet_id = fi;
            } else {
                components.merge(rep_facet_id, fi);
            }
        }
    }

    return components.extract_disjoint_set_indices(component_id);
};

template <typename Scalar, typename Index>
size_t compute_edge_based_components(
    SurfaceMesh<Scalar, Index>& mesh,
    AttributeId id,
    span<const Index> blocker_edges)
{
    constexpr Index invalid_index = invalid<Index>();
    auto& attr = mesh.template ref_attribute<Index>(id);
    auto component_id = attr.ref_all();
    const auto num_edges = mesh.get_num_edges();
    const auto num_facets = mesh.get_num_facets();
    la_debug_assert(component_id.size() == static_cast<size_t>(num_facets));

    std::vector<bool> is_blocked(num_edges, false);
    std::for_each(blocker_edges.begin(), blocker_edges.end(), [&](Index ei) {
        is_blocked[ei] = true;
    });

    DisjointSets<Index> components(num_facets);
    for (auto ei : range(num_edges)) {
        if (is_blocked[ei]) continue;
        Index rep_facet_id = invalid_index;
        for (Index ci = mesh.get_first_corner_around_edge(ei); ci != invalid_index;
             ci = mesh.get_next_corner_around_edge(ci)) {
            Index fi = mesh.get_corner_facet(ci);
            if (rep_facet_id == invalid_index) {
                rep_facet_id = fi;
            } else {
                components.merge(rep_facet_id, fi);
            }
        }
    }

    return components.extract_disjoint_set_indices(component_id);
};

template <typename Scalar, typename Index>
size_t compute_components(SurfaceMesh<Scalar, Index>& mesh, ComponentOptions options)
{
    return compute_components(mesh, {}, std::move(options));
}

template <typename Scalar, typename Index>
size_t compute_components(
    SurfaceMesh<Scalar, Index>& mesh,
    span<const Index> blocker_elements,
    ComponentOptions options)
{
    AttributeId id;

    if (mesh.has_attribute(options.output_attribute_name)) {
        id = mesh.get_attribute_id(options.output_attribute_name);
        la_runtime_assert(mesh.template is_attribute_type<Index>(id));
        la_runtime_assert(!mesh.is_attribute_indexed(id));
    } else {
        id = mesh.template create_attribute<Index>(
            options.output_attribute_name,
            AttributeElement::Facet,
            AttributeUsage::Scalar,
            1);
    }

    mesh.initialize_edges();

    switch (options.connectivity_type) {
    case ComponentOptions::ConnectivityType::Vertex:
        return compute_vertex_based_components(mesh, id, blocker_elements);
    case ComponentOptions::ConnectivityType::Edge:
        return compute_edge_based_components(mesh, id, blocker_elements);
    default: throw Error("Unsupported connectivity type");
    }
}

#define LA_X_compute_components(_, Scalar, Index)      \
    template LA_CORE_API size_t compute_components<Scalar, Index>( \
        SurfaceMesh<Scalar, Index>&,                   \
        ComponentOptions);                             \
    template LA_CORE_API size_t compute_components<Scalar, Index>( \
        SurfaceMesh<Scalar, Index>&,                   \
        span<const Index>,                             \
        ComponentOptions);
LA_SURFACE_MESH_X(compute_components, 0)

} // namespace lagrange
