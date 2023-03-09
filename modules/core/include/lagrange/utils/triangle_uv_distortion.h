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
#include <lagrange/DistortionMetric.h>
#include <lagrange/utils/span.h>

#include <cmath>
#include <type_traits>

namespace lagrange {

/**
 * Compute uv distortion of a single triangle.
 *
 * Let φ be the mapping from the 3D triangle (V0, V1, V2) to the uv triangle (v0, v1, v2), and let
 * J = ∇φ be its Jacobian. A distortion metric measures the amount of deviation from isotropic or
 * conformal mapping. It can often be expressed concisely in terms of the singular values, (s0, s1)
 * of J.  Here are a list of supported distortion metrics:
 *
 * *           Dirichlet: s0*s0 + s1*s1
 * *   Inverse Dirichlet: 1/(s0*s0) + 1/(s1*s1)
 * * Symmetric Dirichlet: s0*s0 + s1*s1 + 1/(s0*s0) + 1/(s1*s1)
 * *                MIPS: s0/s1 + s1/s0
 * *          Area ratio: s0*s1
 *
 * @note While it is easy to express distortion measures in terms of s0 and s1, it is often
 * computationally unstable or expensive to explicitly compute these singular values.  Instead, this
 * method computes the distortion measures geometrically.
 *
 * @tparam metric The type of distortion metric to use.
 * @tparam Scalar The scalar type.
 *
 * @param V0, V1, V2  The coordinates of the 3D triangle.
 * @param v0, v1, v2  The coordinates of the uv triangle.
 *
 * @return The distortion measure of the uv mapping for this triangle.
 */
template <DistortionMetric metric, typename Scalar>
Scalar triangle_uv_distortion(
    span<const Scalar, 3> V0,
    span<const Scalar, 3> V1,
    span<const Scalar, 3> V2,
    span<const Scalar, 2> v0,
    span<const Scalar, 2> v1,
    span<const Scalar, 2> v2);

/**
 * Compute uv distortion of a single triangle.
 *
 * @tparam Scalar The scalar type.
 *
 * @param V0, V1, V2  The coordinates of the 3D triangle.
 * @param v0, v1, v2  The coordinates of the uv triangle.
 * @param metric      The distorsion metric to compute.
 *
 * @return The distortion measure of the uv mapping for this triangle.
 *
 * @overload
 */
template <typename Scalar>
Scalar triangle_uv_distortion(
    span<const Scalar, 3> V0,
    span<const Scalar, 3> V1,
    span<const Scalar, 3> V2,
    span<const Scalar, 2> v0,
    span<const Scalar, 2> v1,
    span<const Scalar, 2> v2,
    DistortionMetric metric);

} // namespace lagrange
