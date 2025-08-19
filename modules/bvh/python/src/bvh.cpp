/*
 * Copyright 2025 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */

#include <lagrange/bvh/weld_vertices.h>
#include <lagrange/python/bvh.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <nanobind/nanobind.h>
#include <nanobind/eigen/dense.h>
#include <lagrange/utils/warnon.h>
// clang-format on

namespace nb = nanobind;
using namespace nb::literals;

namespace lagrange::python {

void populate_bvh_module(nb::module_& m)
{
    using Scalar = double;
    using Index = uint32_t;
    using MeshType = SurfaceMesh<Scalar, Index>;

    m.def(
        "weld_vertices",
        [](MeshType& mesh, Scalar radius, bool boundary_only) {
            bvh::WeldOptions options;
            options.radius = radius;
            options.boundary_only = boundary_only;
            bvh::weld_vertices(mesh, options);
        },
        "mesh"_a,
        "radius"_a = 1e-6f,
        "boundary_only"_a = false,
        R"(Weld nearby vertices together of a surface mesh.

:param mesh: The target surface mesh to be welded in place.
:param radius: The maximum distance between vertices to be considered for welding. Default is 1e-6.
:param boundary_only: If true, only boundary vertices will be considered for welding. Defaults to False.

.. warning:: This method may introduce non-manifoldness and degeneracy in the mesh.)");
}

} // namespace lagrange::python
