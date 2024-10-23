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
#include <lagrange/AttributeFwd.h>
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/compute_weighted_corner_normal.h>
#include <lagrange/internal/find_attribute_utils.h>
#include <lagrange/utils/assert.h>
#include <lagrange/views.h>

#include "internal/compute_weighted_corner_normal.h"

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <tbb/parallel_for.h>
#include <lagrange/utils/warnon.h>
// clang-format on

namespace lagrange {

template <typename Scalar, typename Index>
AttributeId compute_weighted_corner_normal(
    SurfaceMesh<Scalar, Index>& mesh,
    CornerNormalOptions options)
{
    const Index num_corners = mesh.get_num_corners();

    AttributeId id = internal::find_or_create_attribute<Scalar>(
        mesh,
        options.output_attribute_name,
        Corner,
        AttributeUsage::Normal,
        3,
        internal::ResetToDefault::Yes);

    auto normals = matrix_ref(mesh.template ref_attribute<Scalar>(id));
    la_debug_assert(static_cast<Index>(normals.rows()) == num_corners);

    tbb::parallel_for(Index(0), num_corners, [&](Index ci) {
        normals.row(ci).template head<3>() +=
            internal::compute_weighted_corner_normal(mesh, ci, options.weight_type);
    });

    return id;
}

#define LA_X_compute_weighted_corner_normal(_, Scalar, Index)                       \
    template LA_CORE_API AttributeId compute_weighted_corner_normal<Scalar, Index>( \
        SurfaceMesh<Scalar, Index>&,                                                \
        CornerNormalOptions);
LA_SURFACE_MESH_X(compute_weighted_corner_normal, 0)

} // namespace lagrange
