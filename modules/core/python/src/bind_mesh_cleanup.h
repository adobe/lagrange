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

#include <lagrange/mesh_cleanup/close_small_holes.h>
#include <lagrange/mesh_cleanup/detect_degenerate_facets.h>
#include <lagrange/mesh_cleanup/remove_degenerate_facets.h>
#include <lagrange/mesh_cleanup/remove_duplicate_facets.h>
#include <lagrange/mesh_cleanup/remove_duplicate_vertices.h>
#include <lagrange/mesh_cleanup/remove_isolated_vertices.h>
#include <lagrange/mesh_cleanup/remove_null_area_facets.h>
#include <lagrange/mesh_cleanup/remove_short_edges.h>
#include <lagrange/mesh_cleanup/remove_topologically_degenerate_facets.h>
#include <lagrange/mesh_cleanup/rescale_uv_charts.h>
#include <lagrange/mesh_cleanup/resolve_nonmanifoldness.h>
#include <lagrange/mesh_cleanup/resolve_vertex_nonmanifoldness.h>
#include <lagrange/mesh_cleanup/split_long_edges.h>
#include <lagrange/python/binding.h>

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

.. note::
    A vertex is considered isolated if it is not referenced by any facet.

:param mesh: Input mesh (modified in place).)");

    m.def(
        "detect_degenerate_facets",
        &detect_degenerate_facets<Scalar, Index>,
        "mesh"_a,
        R"(Detect degenerate facets in a mesh.

.. note::
    Only exactly degenerate facets are detected.

:param mesh: Input mesh.

:returns: List of degenerate facet indices.)");

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

:param mesh: Input mesh (modified in place).
:param null_area_threshold: Area threshold below which facets are considered null.
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

:param mesh: Input mesh (modified in place).
:param extra_attributes: Additional attributes to consider when detecting duplicates.
:param boundary_only: Only remove duplicate vertices on the boundary.)");

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

Facets with different orientations (e.g. (0,1,2) and (2,1,0)) are considered duplicates.
If both orientations have equal counts, all are removed.
If one orientation has more duplicates, all but one of the majority orientation are kept.

:param mesh: Input mesh (modified in place).
:param consider_orientation: Whether to consider orientation when detecting duplicates.)");

    m.def(
        "remove_topologically_degenerate_facets",
        &remove_topologically_degenerate_facets<Scalar, Index>,
        "mesh"_a,
        R"(Remove topologically degenerate facets such as (0,1,1).

For polygons, topological degeneracy means the polygon has at most two unique vertices.
E.g. quad (0,0,1,1) is degenerate, while (1,1,2,3) is not.

:param mesh: Input mesh (modified in place).)");

    m.def(
        "remove_short_edges",
        &remove_short_edges<Scalar, Index>,
        "mesh"_a,
        "threshold"_a = 0,
        R"(Remove short edges from a mesh.

:param mesh: Input mesh (modified in place).
:param threshold: Minimum edge length below which edges are considered short.)");

    m.def(
        "resolve_vertex_nonmanifoldness",
        &resolve_vertex_nonmanifoldness<Scalar, Index>,
        "mesh"_a,
        R"(Resolve vertex non-manifoldness in a mesh.

:param mesh: Input mesh (modified in place).

:raises RuntimeError: If the input mesh is not edge-manifold.)");

    m.def(
        "resolve_nonmanifoldness",
        &resolve_nonmanifoldness<Scalar, Index>,
        "mesh"_a,
        R"(Resolve both vertex and edge nonmanifoldness in a mesh.

:param mesh: Input mesh (modified in place).)");

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
        R"(Split edges longer than max_edge_length.

:param mesh: Input mesh (modified in place).
:param max_edge_length: Maximum edge length threshold.
:param recursive: If true, apply recursively until no edge exceeds threshold.
:param active_region_attribute: Facet attribute name for active region (uint8_t type).
                                If None, all edges are considered.
:param edge_length_attribute: Edge length attribute name.
                              If None, edge lengths are computed.
)");

    m.def(
        "remove_degenerate_facets",
        &remove_degenerate_facets<Scalar, Index>,
        "mesh"_a,
        R"(Remove degenerate facets from a mesh.

.. note::
    Assumes triangular mesh. Use `triangulate_polygonal_facets` for non-triangular meshes.
    Adjacent non-degenerate facets may be re-triangulated during removal.

:param mesh: Input mesh (modified in place).)");

    m.def(
        "close_small_holes",
        [](MeshType& mesh, size_t max_hole_size, bool triangulate_holes) {
            CloseSmallHolesOptions options;
            options.max_hole_size = max_hole_size;
            options.triangulate_holes = triangulate_holes;
            close_small_holes(mesh, options);
        },
        "mesh"_a,
        "max_hole_size"_a = CloseSmallHolesOptions().max_hole_size,
        "triangulate_holes"_a = CloseSmallHolesOptions().triangulate_holes,
        R"(Close small holes in a mesh.

:param mesh: Input mesh (modified in place).
:param max_hole_size: Maximum number of vertices on a hole to be closed.
:param triangulate_holes: Whether to triangulate holes (if false, fill with polygons).)");

    m.def(
        "rescale_uv_charts",
        [](MeshType& mesh,
           std::string_view uv_attribute_name,
           std::string_view chart_id_attribute_name,
           double uv_area_threshold) {
            RescaleUVOptions options;
            options.uv_attribute_name = uv_attribute_name;
            options.chart_id_attribute_name = chart_id_attribute_name;
            options.uv_area_threshold = uv_area_threshold;
            rescale_uv_charts(mesh, options);
        },
        "mesh"_a,
        "uv_attribute_name"_a = RescaleUVOptions().uv_attribute_name,
        "chart_id_attribute_name"_a = RescaleUVOptions().chart_id_attribute_name,
        "uv_area_threshold"_a = RescaleUVOptions().uv_area_threshold,
        R"(Rescale UV charts to match their 3D aspect ratios.

:param mesh: Input mesh (modified in place).
:param uv_attribute_name: UV attribute name for rescaling.
                         If empty, uses first UV attribute found.
:param chart_id_attribute_name: Patch ID attribute name.
                                If empty, computes patches from UV chart connectivity.
:param uv_area_threshold: UV area threshold.
                         Triangles below this threshold don't contribute to scale computation.
)");
}

} // namespace lagrange::python
