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
#pragma once

#include <lagrange/SurfaceMesh.h>

#include <string_view>

namespace lagrange {

///
/// @addtogroup group-surfacemesh-utils
/// @{
///

struct DisconnectUVChartsOptions
{
    /// Input UV attribute name.
    /// If empty, the first indexed UV attribute will be used. The attribute must be indexed.
    std::string_view uv_attribute_name = "";

    /// Optional per-facet chart id attribute name.
    /// If empty, chart ids are computed automatically using edge connectivity on the UV mesh.
    std::string_view chart_id_attribute_name = "";
};

/**
 * Disconnect UV charts by duplicating UV vertices shared across different charts.
 *
 * After this operation, no two facets belonging to different UV charts will share a UV vertex
 * index. Without any input chart id attribute, this eliminates non-manifold UV vertices (pinch
 * points) where charts touch at a single vertex.
 *
 * @param      mesh     Input mesh. The UV attribute must be indexed.
 * @param      options  Options to control chart disconnection.
 *
 * @tparam     Scalar   Mesh scalar type.
 * @tparam     Index    Mesh index type.
 *
 * @return     The number of UV vertices that were duplicated.
 *
 * @see        @ref DisconnectUVChartsOptions
 */
template <typename Scalar, typename Index>
size_t disconnect_uv_charts(
    SurfaceMesh<Scalar, Index>& mesh,
    const DisconnectUVChartsOptions& options = {});

/// @}

} // namespace lagrange
