/*
 * Copyright 2017 Adobe. All rights reserved.
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
#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
    #include <lagrange/legacy/compute_uv_distortion.h>
#endif

#include <lagrange/DistortionMetric.h>
#include <lagrange/SurfaceMesh.h>

namespace lagrange {
///
/// @defgroup   group-surfacemesh-utils Mesh utilities
/// @ingroup    group-surfacemesh
///
/// Various mesh processing utilities.
///
/// @{

///
/// Option struct for compute uv distortion.
///
struct UVDistortionOptions
{
    /// Input UV indexed attribute name.
    std::string_view uv_attribute_name = "@uv";

    /// The output attribute name.
    std::string_view output_attribute_name = "@uv_measure";

    /// The distortion measure.
    DistortionMetric metric = DistortionMetric::MIPS;
};

///
/// Compute uv distortion using the selected distortion measure.
///
/// @param[in]  mesh     The input mesh.
/// @param[in]  options  The computation option settings.
///
/// @tparam     Scalar          Mesh scalar type.
/// @tparam     Index           Mesh index type.
///
/// @return     The attribute id of the distortion measure facet attribute.
///
/// @see @ref UVDistortionOptions
///
template <typename Scalar, typename Index>
AttributeId compute_uv_distortion(
    SurfaceMesh<Scalar, Index>& mesh,
    const UVDistortionOptions& options = {});

/// @}
} // namespace lagrange
