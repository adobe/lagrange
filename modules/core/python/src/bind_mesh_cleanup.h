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
#include <lagrange/mesh_cleanup/remove_duplicate_vertices.h>
#include <lagrange/mesh_cleanup/remove_isolated_vertices.h>
#include <lagrange/mesh_cleanup/remove_null_area_facets.h>

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

    m.def("remove_isolated_vertices", &remove_isolated_vertices<Scalar, Index>, "mesh"_a);
    m.def("detect_degenerate_facets", &detect_degenerate_facets<Scalar, Index>, "mesh"_a);

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
        [](MeshType& mesh, std::optional<std::vector<AttributeId>> extra_attributes) {
            RemoveDuplicateVerticesOptions opts;
            if (extra_attributes.has_value()) {
                opts.extra_attributes = std::move(extra_attributes.value());
            }
            remove_duplicate_vertices(mesh, opts);
        },
        "mesh"_a,
        "extra_attributes"_a = nb::none(),
        R"(Remove duplicate vertices from a mesh.

:param mesh:             The input mesh.
:param extra_attributes: Two vertices are considered duplicates if they have the same position and
                         the same values for all attributes in `extra_attributes`.)");
}

} // namespace lagrange::python
