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
#pragma once

#include <lagrange/SurfaceMesh.h>
#include <lagrange/types/ConnectivityType.h>

namespace lagrange {

struct UVChartOptions
{
    using ConnectivityType = lagrange::ConnectivityType;

    /// Input UV attribute name.
    /// If empty, the first indexed UV attribute will be used.
    std::string_view uv_attribute_name = "";

    /// Output chart id attribute name.
    std::string_view output_attribute_name = "@chart_id";

    /// Connectivity type used for chart computation.
    ConnectivityType connectivity_type = ConnectivityType::Edge;
};

/**
 * Compute UV charts of an input mesh.
 *
 * This method will create a per-vertex chart id in an attribute named
 * `UVChartOptions::output_attribute_name`.  Each chart id is in [0, num_charts-1].
 *
 * @param      mesh     Input mesh.
 * @param      options  Options to control chart computation.
 *
 * @tparam     Scalar   Mesh scalar type.
 * @tparam     Index    Mesh index type.
 *
 * @return     The total number of UV charts.
 *
 * @see        `UVChartOptions`
 */
template <typename Scalar, typename Index>
size_t compute_uv_charts(SurfaceMesh<Scalar, Index>& mesh, const UVChartOptions& options = {});

} // namespace lagrange
