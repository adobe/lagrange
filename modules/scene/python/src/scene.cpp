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

#include "bind_scene.h"
#include "bind_simple_scene.h"

#include <lagrange/python/binding.h>
#include <lagrange/scene/RemeshingOptions.h>

namespace lagrange::python {

void populate_scene_module(nb::module_& m)
{
    using Scalar = double;
    using Index = uint32_t;

    bind_simple_scene<Scalar, Index>(m);

    nb::enum_<lagrange::scene::FacetAllocationStrategy>(
        m,
        "FacetAllocationStrategy",
        "Facet allocation strategy for meshes in the scene during decimation or remeshing.")
        .value(
            "EvenSplit",
            lagrange::scene::FacetAllocationStrategy::EvenSplit,
            "Split facet budget evenly between all meshes in a scene.")
        .value(
            "RelativeToMeshArea",
            lagrange::scene::FacetAllocationStrategy::RelativeToMeshArea,
            "Allocate facet budget according to the mesh area in the scene.")
        .value(
            "RelativeToNumFacets",
            lagrange::scene::FacetAllocationStrategy::RelativeToNumFacets,
            "Allocate facet budget according to the number of facets.")
        .value(
            "Synchronized",
            lagrange::scene::FacetAllocationStrategy::Synchronized,
            "Synchronize simplification between multiple meshes in a scene by computing a "
            "conservative threshold on the QEF error of all edges in the scene. This option gives "
            "the best result in terms of facet budget allocation, but is a bit slower than other "
            "options.");

    nb::class_<lagrange::scene::RemeshingOptions>(m, "RemeshingOptions")
        .def(nb::init<>())
        .def_rw(
            "facet_allocation_strategy",
            &lagrange::scene::RemeshingOptions::facet_allocation_strategy,
            "Facet allocation strategy for meshes in the scene.")
        .def_rw(
            "min_facets",
            &lagrange::scene::RemeshingOptions::min_facets,
            "Minimum amount of facets for meshes in the scene.");

    bind_scene(m);
}

} // namespace lagrange::python
