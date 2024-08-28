/*
 * Copyright 2024 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */

#include <lagrange/poisson/mesh_from_oriented_points.h>

#include <Eigen/Core>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <nanobind/nanobind.h>
#include <nanobind/eigen/dense.h>
#include <nanobind/stl/pair.h>
#include <lagrange/utils/warnon.h>
// clang-format on

namespace lagrange::python {

namespace nb = nanobind;
using namespace nb::literals;

void populate_poisson_module(nb::module_& m)
{
    using Scalar = double;
    using Index = uint32_t;

    // TODO: Fill-in later
}

} // namespace lagrange::python
