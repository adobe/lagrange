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

#include "bind_simple_scene.h"

#include <lagrange/scene/RemeshingOptions.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <nanobind/nanobind.h>
#include <lagrange/utils/warnon.h>
// clang-format on


namespace lagrange::python {

void populate_scene_module(nb::module_& m)
{
    using Scalar = double;
    using Index = uint32_t;

    bind_simple_scene<Scalar, Index>(m);

    nb::enum_<lagrange::scene::FacetAllocationStrategy>(m, "FacetAllocationStrategy")
        .value("EvenSplit", lagrange::scene::FacetAllocationStrategy::EvenSplit)
        .value("RelativeToMeshArea", lagrange::scene::FacetAllocationStrategy::RelativeToMeshArea)
        .value(
            "RelativeToNumFacets",
            lagrange::scene::FacetAllocationStrategy::RelativeToNumFacets);

    nb::class_<lagrange::scene::RemeshingOptions>(m, "RemeshingOptions")
        .def(nb::init<>())
        .def_rw(
            "facet_allocation_strategy",
            &lagrange::scene::RemeshingOptions::facet_allocation_strategy)
        .def_rw("min_facets", &lagrange::scene::RemeshingOptions::min_facets);
}

} // namespace lagrange::python
