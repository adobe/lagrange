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

#include <lagrange/packing/repack_uv_charts.h>

#include <lagrange/python/binding.h>

namespace lagrange::python {

namespace nb = nanobind;
using namespace nb::literals;

void populate_packing_module(nb::module_& m)
{
    using Scalar = double;
    using Index = uint32_t;

    using Options = lagrange::packing::RepackOptions;

    m.def(
        "repack_uv_charts",
        [](SurfaceMesh<Scalar, Index>& mesh,
           std::string_view uv_attribute_name,
           std::string_view chart_attribute_name,
#ifndef RECTANGLE_BIN_PACK_OSS
           bool allow_rotation,
#endif
           bool normalize,
           float margin) {
            Options options;
            options.uv_attribute_name = uv_attribute_name;
            options.chart_attribute_name = chart_attribute_name;
#ifndef RECTANGLE_BIN_PACK_OSS
            options.allow_rotation = allow_rotation;
#endif
            options.normalize = normalize;
            options.margin = margin;
            lagrange::packing::repack_uv_charts(mesh, options);
        },
        "mesh"_a,
        nb::kw_only(),
        "uv_attribute_name"_a = "",
        "chart_attribute_name"_a = "",
#ifndef RECTANGLE_BIN_PACK_OSS
        "allow_rotation"_a = Options().allow_rotation,
#endif
        "normalize"_a = Options().normalize,
        "margin"_a = Options().margin,
        R"(Pack UV charts of a given mesh.

The UV attribute is updated in place.

:param mesh: The mesh with UV attribute.
:param uv_attribute_name: Name of the indexed attribute to use as UV coordinates. If empty, the first indexed UV attribute will be used.
:param chart_attribute_name: Name of the facet attribute that groups facets into UV charts. If empty, it will be computed based on UV chart connectivity.
)"
#ifndef RECTANGLE_BIN_PACK_OSS
        R"(:param allow_rotation: Whether to allow boxes to rotate by 90 degrees when packing.
)"
#endif
        R"(:param normalize: Whether the output should be normalized to fit into a unit box. When false, the packed charts preserve their original scale but are still translated so the minimum UV is at the origin.
:param margin: Minimum allowed distance between two boxes. When ``normalize`` is true, this value is measured in the normalized ``[0, 1]`` output domain. When ``normalize`` is false, it is interpreted as an absolute distance in the original UV units.

:return: None. The UV attribute is modified in place.)");
}

} // namespace lagrange::python
