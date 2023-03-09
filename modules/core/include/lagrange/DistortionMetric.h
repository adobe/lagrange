/*
 * Copyright 2023 Adobe. All rights reserved.
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

namespace lagrange {
///
/// @defgroup   group-surfacemesh-utils Mesh utilities
/// @ingroup    group-surfacemesh
///
/// Various mesh processing utilities.
///
/// @{

///
/// UV distortion metric type.
///
enum class DistortionMetric {
    Dirichlet, ///< Dirichlet energy.
    InverseDirichlet, ///< Inverse Dirichlet energy.
    SymmetricDirichlet, ///< Symmetric Dirichlet energy.
    AreaRatio, /// UV triangle area / 3D triangle area.
    MIPS, /// Most Isotropic Parameterizations energy.
};

/// @}
} // namespace lagrange
