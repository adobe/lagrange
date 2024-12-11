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
#pragma once

#include <lagrange/mesh_cleanup/detect_degenerate_facets.h>
#include <lagrange/mesh_cleanup/remove_degenerate_facets.h>
#include <lagrange/mesh_cleanup/remove_duplicate_facets.h>
#include <lagrange/mesh_cleanup/remove_duplicate_vertices.h>
#include <lagrange/mesh_cleanup/remove_isolated_vertices.h>
#include <lagrange/mesh_cleanup/remove_null_area_facets.h>
#include <lagrange/mesh_cleanup/remove_short_edges.h>
#include <lagrange/mesh_cleanup/remove_topologically_degenerate_facets.h>
#include <lagrange/mesh_cleanup/resolve_nonmanifoldness.h>
#include <lagrange/mesh_cleanup/resolve_vertex_nonmanifoldness.h>
#include <lagrange/mesh_cleanup/split_long_edges.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <nanobind/nanobind.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/vector.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <vector>

namespace lagrange::python {

template <typename Scalar, typename Index>
void bind_mesh_cleanup(nanobind::module_& m)
{
    namespace nb = nanobind;
    using namespace nb::literals;
    using MeshType = SurfaceMesh<Scalar, Index>;

    m.def(
        "remove_isolated_vertices",
        &remove_isolated_vertices<Scalar, Index>,
        "mesh"_a,
        R"(Remove isolated vertices from a mesh.

.. note:
    A vertex is considered isolated if it is not referenced by any facet.

:param mesh: The input mesh for inplace modification.)");

    m.def(
        "detect_degenerate_facets",
        &detect_degenerate_facets<Scalar, Index>,
        "mesh"_a,
        R"(Detect degenerate facets in a mesh.

.. note:
    Only __exaxtly__ degenerate facets are detected.

:param mesh: The input mesh.

:returns: A list of indices of degenerate facets.)");

    m.def(
        "remove_null_area_facets",
        [](MeshType& mesh, double null_area_threshold, bool remove_isolated_vertices) {
            RemoveNullAreaFacetsOptions opts;
            opts.null_area_threshold = null_area_threshold;
            opts.remove_isolated_vertices = remove_isolated_vertices;
            remove_null_area_facets(mesh, opts);
        },
        "mesh"_a,
        "null_area_threshold"_a = 0,
        "remove_isolated_vertices"_a = false,
        R"(Remove facets with unsigned facets area <= `null_area_threhsold`.

:param mesh:                     The input mesh.
:param null_area_threshold:      The area threshold below which a facet is considered as null facet.
:param remove_isolated_vertices: Whether to remove isolated vertices after removing null area facets.)");

    m.def(
        "remove_duplicate_vertices",
        [](MeshType& mesh,
           std::optional<std::vector<AttributeId>> extra_attributes,
           bool boundary_only) {
            RemoveDuplicateVerticesOptions opts;
            if (extra_attributes.has_value()) {
                opts.extra_attributes = std::move(extra_attributes.value());
            }
            opts.boundary_only = boundary_only;
            remove_duplicate_vertices(mesh, opts);
        },
        "mesh"_a,
        "extra_attributes"_a = nb::none(),
        "boundary_only"_a = false,
        R"(Remove duplicate vertices from a mesh.

:param mesh:             The input mesh.
:param extra_attributes: Two vertices are considered duplicates if they have the same position and the same values for all attributes in `extra_attributes`.
:param boundary_only:    Only remove duplicate vertices on the boundary.)");

    m.def(
        "remove_duplicate_facets",
        [](MeshType& mesh, bool consider_orientation) {
            RemoveDuplicateFacetOptions opts;
            opts.consider_orientation = consider_orientation;
            remove_duplicate_facets(mesh, opts);
        },
        "mesh"_a,
        "consider_orientation"_a = false,
        R"(Remove duplicate facets from a mesh.

Facets of different orientations (e.g. (0, 1, 2) and (2, 1, 0)) are considered as duplicates.
If both orientations have equal number of duplicate facets, all of them are removed.
If one orientation has more duplicate facets, all but one facet with the majority orientation are removed.

:param mesh: The input mesh. The mesh is modified in place.
:param consider_orientation: Whether to consider orientation when detecting duplicate facets.)");

    m.def(
        "remove_topologically_degenerate_facets",
        &remove_topologically_degenerate_facets<Scalar, Index>,
        "mesh"_a,
        R"(Remove topologically degenerate facets such as (0, 1, 1)

For general polygons, topologically degeneracy means that the polygon is made of at most two unique
vertices. E.g. a quad of the form (0, 0, 1, 1) is degenerate, while a quad of the form (1, 1, 2, 3)
is not.

:param mesh: The input mesh for inplace modification.)");

    m.def(
        "remove_short_edges",
        &remove_short_edges<Scalar, Index>,
        "mesh"_a,
        "threshold"_a = 0,
        R"(Remove short edges from a mesh.

:param mesh:      The input mesh for inplace modification.
:param threshold: The minimum edge length below which an edge is considered short.)");

    m.def(
        "resolve_vertex_nonmanifoldness",
        &resolve_vertex_nonmanifoldness<Scalar, Index>,
        "mesh"_a,
        R"(Resolve vertex non-manifoldness in a mesh.

:param mesh: The input mesh for inplace modification.

:raises RuntimeError: If the input mesh is not edge-manifold.)");

    m.def(
        "resolve_nonmanifoldness",
        &resolve_nonmanifoldness<Scalar, Index>,
        "mesh"_a,
        R"(Resolve both vertex and edge nonmanifoldness in a mesh.

:param mesh: The input mesh for inplace modification.)");

    m.def(
        "split_long_edges",
        [](MeshType& mesh,
           float max_edge_length,
           bool recursive,
           std::optional<std::string_view> active_region_attribute,
           std::optional<std::string_view> edge_length_attribute) {
            SplitLongEdgesOptions opts;
            opts.max_edge_length = max_edge_length;
            opts.recursive = recursive;
            if (active_region_attribute.has_value())
                opts.active_region_attribute = active_region_attribute.value();
            if (edge_length_attribute.has_value())
                opts.edge_length_attribute = edge_length_attribute.value();
            split_long_edges(mesh, std::move(opts));
        },
        "mesh"_a,
        "max_edge_length"_a = 0.1f,
        "recursive"_a = true,
        "active_region_attribute"_a = nb::none(),
        "edge_length_attribute"_a = nb::none(),
        R"(Split edges that are longer than `max_edge_length`.

:param mesh:                  The input mesh for inplace modification.
:param max_edge_length:       Maximum edge length. Edges longer than this value will be split.
:param recursive:              If true, the operation will be applied recursively until no edge is longer than `max_edge_length`.
:param active_region_attribute: The facet attribute name that will be used to determine the active region.
                                If `None`, all edges will be considered.
                                Otherwise, only edges that are part of the active region will be split.
                                The attribute must be of value type `uint8_t`.
:param edge_length_attribute:  The edge length attribute name that will be used to determine the edge length.
                               If `None`, the function will compute the edge lengths.
)");

    m.def(
        "remove_degenerate_facets",
        &remove_degenerate_facets<Scalar, Index>,
        "mesh"_a,
        R"(Remove degenerate facets from a mesh.

..note:
    The current method assumes input mesh is triangular.
    Use `triangulate_polygonal_facets` if the input mesh is not triangular.
    Non-degenerate facets adjacent to degenerte facets may be re-triangulated as a result of the removal.

:param mesh: The mesh for inplace modification.)");
}

} // namespace lagrange::python
