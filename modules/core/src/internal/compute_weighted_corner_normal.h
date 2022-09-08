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
#pragma once

#include <lagrange/NormalWeightingType.h>
#include <lagrange/SurfaceMesh.h>

#include <Eigen/Core>

namespace lagrange::internal {

/**
 * Compute weighted corner normal based on the weighting type.
 *
 * @param[in] mesh       The input mesh.
 * @param[in] ci         The target corner index.
 * @param[in] weighting  The weighting type.
 *
 * @returns the weighted normal vector corresponding to the target corner.
 */
template <typename Scalar, typename Index>
Eigen::Matrix<Scalar, 3, 1> compute_weighted_corner_normal(
    SurfaceMesh<Scalar, Index>& mesh,
    Index ci,
    NormalWeightingType weighting = NormalWeightingType::CornerTriangleArea);

} // namespace lagrange::internal
