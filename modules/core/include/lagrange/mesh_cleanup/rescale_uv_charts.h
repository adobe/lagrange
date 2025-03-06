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

#include <string_view>

namespace lagrange {

struct RescaleUVOptions
{
    /**
     * The uv attribute name to use for rescaling.
     *
     * If empty, the first UV attribute found will be used.
     */
    std::string_view uv_attribute_name = "";

    /**
     * The name used for storing the patch ID attribute.
     *
     * If empty, patches will be computed based on UV chart connectivity.
     */
    std::string_view chart_id_attribute_name = "";

    /**
     * The threshold for UV area.
     *
     * UV triangles with area smaller than this threshold will not contribute to scale factor
     * computation.
     */
    double uv_area_threshold = 1e-6;
};

///
/// Rescale UV charts such that they are isotropic to their 3D images.
///
/// @param mesh    Input mesh, which will be modified in place
/// @param options Rescale options
///
template <typename Scalar, typename Index>
void rescale_uv_charts(SurfaceMesh<Scalar, Index>& mesh, const RescaleUVOptions& options = {});

} // namespace lagrange
