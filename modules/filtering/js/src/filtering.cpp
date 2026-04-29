/*
 * Copyright 2026 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */

#include "bind_types.h"

#include <lagrange/filtering/attribute_smoothing.h>
#include <lagrange/filtering/mesh_smoothing.h>

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <string>

namespace {

using namespace lagrange;
using namespace lagrange::js::bind;
using val = emscripten::val;

} // namespace

EMSCRIPTEN_BINDINGS(lagrange_filtering)
{
    using namespace emscripten;

    function(
        "meshSmoothing",
        +[](MeshType& mesh, val opts) {
            filtering::SmoothingOptions o;
            if (!opts.isUndefined()) {
                auto method = opts["method"];
                if (!method.isUndefined()) {
                    auto s = method.as<std::string>();
                    if (s == "vertexSmoothing") {
                        o.filter_method =
                            filtering::SmoothingOptions::FilterMethod::VertexSmoothing;
                    } else {
                        o.filter_method =
                            filtering::SmoothingOptions::FilterMethod::NormalSmoothing;
                    }
                }
                apply_opt(opts, "curvatureWeight", o.curvature_weight);
                apply_opt(opts, "normalSmoothingWeight", o.normal_smoothing_weight);
                apply_opt(opts, "gradientWeight", o.gradient_weight);
                apply_opt(opts, "gradientModulationScale", o.gradient_modulation_scale);
                apply_opt(opts, "normalProjectionWeight", o.normal_projection_weight);
            }
            filtering::mesh_smoothing(mesh, o);
        });

    function(
        "scalarAttributeSmoothing",
        +[](MeshType& mesh, const std::string& attribute_name, val opts) {
            filtering::AttributeSmoothingOptions o;
            if (!opts.isUndefined()) {
                apply_opt(opts, "curvatureWeight", o.curvature_weight);
                apply_opt(opts, "normalSmoothingWeight", o.normal_smoothing_weight);
                apply_opt(opts, "gradientWeight", o.gradient_weight);
                apply_opt(opts, "gradientModulationScale", o.gradient_modulation_scale);
            }
            filtering::scalar_attribute_smoothing(mesh, attribute_name, o);
        });
}
