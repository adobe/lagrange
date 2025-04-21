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

#include <lagrange/filtering/mesh_smoothing.h>

#include <lagrange/AttributeTypes.h>
#include <lagrange/Logger.h>
#include <lagrange/python/tensor_utils.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/invalid.h>

#include <lagrange/python/setup_mkl.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <nanobind/nanobind.h>
#include <nanobind/eigen/dense.h>
#include <nanobind/stl/array.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/string_view.h>
#include <lagrange/utils/warnon.h>
// clang-format on

namespace lagrange::python {

namespace nb = nanobind;
using namespace nb::literals;

void populate_filtering_module(nb::module_& m)
{
    setup_mkl();

    using Scalar = double;
    using Index = uint32_t;

    using Options = lagrange::filtering::SmoothingOptions;

    m.def(
        "mesh_smoothing",
        [](SurfaceMesh<Scalar, Index>& mesh,
           double curvature_weight,
           double normal_smoothing_weight,
           double gradient_weight,
           double gradient_modulation_scale,
           double normal_projection_weight) {
            Options options;
            options.curvature_weight = curvature_weight;
            options.normal_smoothing_weight = normal_smoothing_weight;
            options.gradient_weight = gradient_weight;
            options.gradient_modulation_scale = gradient_modulation_scale;
            options.normal_projection_weight = normal_projection_weight;
            lagrange::filtering::mesh_smoothing(mesh, options);
        },
        "mesh"_a,
        "curvature_weight"_a = Options().curvature_weight,
        "normal_smoothing_weight"_a = Options().normal_smoothing_weight,
        "gradient_weight"_a = Options().gradient_weight,
        "gradient_modulation_scale"_a = Options().gradient_modulation_scale,
        "normal_projection_weight"_a = Options().normal_projection_weight,
        R"(Smooths a mesh using anisotropic mesh smoothing.

:param mesh: Input mesh.
:param curvature_weight: The curvature/inhomogeneity weight. Specifies the extent to which total curvature should be used to change the underlying metric. Setting =0 is equivalent to using standard homogeneous/anisotropic diffusion.
:param normal_smoothing_weight: The normal smoothing weight. Specifies the extent to which normals should be diffused before curvature is estimated. Formally, this is the time-step for heat-diffusion performed on the normals. Setting =0 will reproduce the original normals.
:param gradient_weight: Gradient fitting weight. Specifies the importance of matching the gradient constraints (objective #2) relative to matching the positional constraints (objective #1). Setting =0 reproduces the original normals.
:param gradient_modulation_scale: Gradient modulation scale. Prescribes the scale factor relating the gradients of the source to those of the target. <1 => gradients are dampened => smoothing. >1 => gradients are amplified => sharpening. Setting =0 is equivalent to performing a semi-implicit step of heat-diffusion, with time-step equal to gradient_weight. Setting =1 reproduces the original normals.
:param normal_projection_weight: Weight for fitting the surface to prescribed normals. Specifies the importance of matching the target normals (objective #2) relative to matching the original positions (objective #1). Setting =0 will reproduce the original geometry.

:return: The smoothed mesh.)");
}

} // namespace lagrange::python
