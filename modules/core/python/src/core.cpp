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

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <nanobind/nanobind.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <lagrange/AttributeTypes.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/python/core.h>
#include "bind_attribute.h"
#include "bind_enum.h"
#include "bind_indexed_attribute.h"
#include "bind_surface_mesh.h"
#include "bind_utilities.h"

namespace lagrange::python {

namespace nb = nanobind;
void populate_core_module(nb::module_& m)
{
    using Scalar = double;
    using Index = uint32_t;

    m.attr("invalid_scalar") = lagrange::invalid<Scalar>();
    m.attr("invalid_index") = nb::int_(lagrange::invalid<Index>());

    lagrange::python::bind_enum(m);
    lagrange::python::bind_surface_mesh<Scalar, Index>(m);
    lagrange::python::bind_attribute(m);
    lagrange::python::bind_indexed_attribute(m);
    lagrange::python::bind_utilities<Scalar, Index>(m);
}

} // namespace lagrange::python
