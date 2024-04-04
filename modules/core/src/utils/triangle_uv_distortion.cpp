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
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/common.h>
#include <lagrange/compute_uv_distortion.h>
#include <lagrange/utils/Error.h>
#include <lagrange/utils/triangle_area.h>
#include <lagrange/utils/triangle_uv_distortion.h>

namespace lagrange {

namespace {

template <typename Scalar, size_t dim>
Scalar dot(span<const Scalar, dim> a, span<const Scalar, dim> b)
{
    static_assert(dim == 2 || dim == 3, "Only 2D and 3D data are supported.");
    if constexpr (dim == 2) {
        return a[0] * b[0] + a[1] * b[1];
    } else if constexpr (dim == 3) {
        return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
    }
}

/**
 * Compute the Dirichlet distorsion energy of the mapping from a 3D triangle to a 2D (uv) triangle.
 *
 * Let φ be the mapping, and J = ∇φ be its Jacobian.  Dirichlet energy is defined as |J|_F^2 (the
 * squared Frobenius norm of J). It is equivalent to σ1^2 + σ2^2, where σ1 and σ2 are the singular
 * values of J.
 *
 * @tparam Scalar  The scalar type.
 * @param V0, V1, V2  The coordinates of the 3D triangle vertices.
 * @param v0, v1, v2  The coordinates of the 2D uv triangle vertices.
 *
 * @return The Dirichlet distorsion energy of this mapping.
 */
template <typename Scalar>
Scalar triangle_dirichlet(
    span<const Scalar, 3> V0,
    span<const Scalar, 3> V1,
    span<const Scalar, 3> V2,
    span<const Scalar, 2> v0,
    span<const Scalar, 2> v1,
    span<const Scalar, 2> v2)
{
    const Scalar A = triangle_area_3d<Scalar>(V0, V1, V2);
    if (A < std::numeric_limits<Scalar>::epsilon()) {
        return std::numeric_limits<Scalar>::infinity();
    }

    const std::array<Scalar, 3> E0{V2[0] - V1[0], V2[1] - V1[1], V2[2] - V1[2]};
    const std::array<Scalar, 3> E1{V0[0] - V2[0], V0[1] - V2[1], V0[2] - V2[2]};
    const std::array<Scalar, 3> E2{V1[0] - V0[0], V1[1] - V0[1], V1[2] - V0[2]};

    const std::array<Scalar, 2> e0{v2[0] - v1[0], v2[1] - v1[1]};
    const std::array<Scalar, 2> e1{v0[0] - v2[0], v0[1] - v2[1]};
    const std::array<Scalar, 2> e2{v1[0] - v0[0], v1[1] - v0[1]};

    const Scalar l0_sq = dot<Scalar, 2>(e0, e0);
    const Scalar l1_sq = dot<Scalar, 2>(e1, e1);
    const Scalar l2_sq = dot<Scalar, 2>(e2, e2);

    const Scalar Cot_0 = -dot<Scalar, 3>(E1, E2) / (2 * A);
    const Scalar Cot_1 = -dot<Scalar, 3>(E2, E0) / (2 * A);
    const Scalar Cot_2 = -dot<Scalar, 3>(E0, E1) / (2 * A);

    return (Cot_0 * l0_sq + Cot_1 * l1_sq + Cot_2 * l2_sq) / (2 * A);
}

/**
 * Compute the inverse Dirichlet distorsion energy of the mapping from a 3D triangle to a 2D (uv)
 * triangle.
 *
 * Let φ be the mapping, and J = ∇φ be its Jacobian.  The inverse Dirichlet energy is defined as
 * |J^-1|_F^2 (the squared Frobenius norm of inverse of J). It is equivalent to 1/σ1^2 + 1/σ2^2,
 * where σ1 and σ2 are the singular values of J.
 *
 * @tparam Scalar  The scalar type.
 * @param V0, V1, V2  The coordinates of the 3D triangle vertices.
 * @param v0, v1, v2  The coordinates of the 2D uv triangle vertices.
 *
 * @return The inverse Dirichlet distorsion energy of this mapping.
 */
template <typename Scalar>
Scalar triangle_inverse_dirichlet(
    span<const Scalar, 3> V0,
    span<const Scalar, 3> V1,
    span<const Scalar, 3> V2,
    span<const Scalar, 2> v0,
    span<const Scalar, 2> v1,
    span<const Scalar, 2> v2)
{
    const Scalar a = triangle_area_2d<Scalar>(v0, v1, v2);
    if (std::abs(a) < std::numeric_limits<Scalar>::epsilon()) {
        return std::numeric_limits<Scalar>::infinity();
    }

    const std::array<Scalar, 3> E0{V2[0] - V1[0], V2[1] - V1[1], V2[2] - V1[2]};
    const std::array<Scalar, 3> E1{V0[0] - V2[0], V0[1] - V2[1], V0[2] - V2[2]};
    const std::array<Scalar, 3> E2{V1[0] - V0[0], V1[1] - V0[1], V1[2] - V0[2]};

    const std::array<Scalar, 2> e0{v2[0] - v1[0], v2[1] - v1[1]};
    const std::array<Scalar, 2> e1{v0[0] - v2[0], v0[1] - v2[1]};
    const std::array<Scalar, 2> e2{v1[0] - v0[0], v1[1] - v0[1]};

    const Scalar L0_sq = dot<Scalar, 3>(E0, E0);
    const Scalar L1_sq = dot<Scalar, 3>(E1, E1);
    const Scalar L2_sq = dot<Scalar, 3>(E2, E2);

    const Scalar cot_0 = -dot<Scalar, 2>(e1, e2) / (2 * a);
    const Scalar cot_1 = -dot<Scalar, 2>(e2, e0) / (2 * a);
    const Scalar cot_2 = -dot<Scalar, 2>(e0, e1) / (2 * a);

    return (cot_0 * L0_sq + cot_1 * L1_sq + cot_2 * L2_sq) / (2 * a);
}

/**
 * Compute the symmetric Dirichlet distorsion energy of the mapping from a 3D triangle to a 2D (uv)
 * triangle.
 *
 * The symmetric dirichlet energy is defined as the sum of Dirichlet and inverse Dirichlet energy.
 *
 * @tparam Scalar  The scalar type.
 * @param V0, V1, V2  The coordinates of the 3D triangle vertices.
 * @param v0, v1, v2  The coordinates of the 2D uv triangle vertices.
 *
 * @return The symmetric Dirichlet distorsion energy of this mapping.
 */
template <typename Scalar>
Scalar triangle_symmetric_dirichlet(
    span<const Scalar, 3> V0,
    span<const Scalar, 3> V1,
    span<const Scalar, 3> V2,
    span<const Scalar, 2> v0,
    span<const Scalar, 2> v1,
    span<const Scalar, 2> v2)
{
    const Scalar A = triangle_area_3d<Scalar>(V0, V1, V2);
    const Scalar a = triangle_area_2d<Scalar>(v0, v1, v2);
    if (A < std::numeric_limits<Scalar>::epsilon() ||
        std::abs(a) < std::numeric_limits<Scalar>::epsilon()) {
        return std::numeric_limits<Scalar>::infinity();
    }

    const std::array<Scalar, 3> E0{V2[0] - V1[0], V2[1] - V1[1], V2[2] - V1[2]};
    const std::array<Scalar, 3> E1{V0[0] - V2[0], V0[1] - V2[1], V0[2] - V2[2]};
    const std::array<Scalar, 3> E2{V1[0] - V0[0], V1[1] - V0[1], V1[2] - V0[2]};

    const std::array<Scalar, 2> e0{v2[0] - v1[0], v2[1] - v1[1]};
    const std::array<Scalar, 2> e1{v0[0] - v2[0], v0[1] - v2[1]};
    const std::array<Scalar, 2> e2{v1[0] - v0[0], v1[1] - v0[1]};

    const Scalar l0_sq = dot<Scalar, 2>(e0, e0);
    const Scalar l1_sq = dot<Scalar, 2>(e1, e1);
    const Scalar l2_sq = dot<Scalar, 2>(e2, e2);

    const Scalar L0_sq = dot<Scalar, 3>(E0, E0);
    const Scalar L1_sq = dot<Scalar, 3>(E1, E1);
    const Scalar L2_sq = dot<Scalar, 3>(E2, E2);

    const Scalar cot_0 = -dot<Scalar, 2>(e1, e2) / (2 * a);
    const Scalar cot_1 = -dot<Scalar, 2>(e2, e0) / (2 * a);
    const Scalar cot_2 = -dot<Scalar, 2>(e0, e1) / (2 * a);

    const Scalar Cot_0 = -dot<Scalar, 3>(E1, E2) / (2 * A);
    const Scalar Cot_1 = -dot<Scalar, 3>(E2, E0) / (2 * A);
    const Scalar Cot_2 = -dot<Scalar, 3>(E0, E1) / (2 * A);

    return (Cot_0 * l0_sq + Cot_1 * l1_sq + Cot_2 * l2_sq) / (2 * A) +
           (cot_0 * L0_sq + cot_1 * L1_sq + cot_2 * L2_sq) / (2 * a);
}

/**
 * Compute the area ratio defined as the 2D uv triangle area / 3D triangle area.
 *
 * @tparam Scalar  The scalar type.
 *
 * @param V0, V1, V2  The coordinates of the 3D triangle vertices.
 * @param v0, v1, v2  The coordinates of the 2D uv triangle vertices.
 *
 * @return The area ratio.
 */
template <typename Scalar>
Scalar triangle_area_ratio(
    span<const Scalar, 3> V0,
    span<const Scalar, 3> V1,
    span<const Scalar, 3> V2,
    span<const Scalar, 2> v0,
    span<const Scalar, 2> v1,
    span<const Scalar, 2> v2)
{
    const Scalar A = triangle_area_3d<Scalar>(V0, V1, V2);
    const Scalar a = triangle_area_2d<Scalar>(v0, v1, v2);
    if (A < std::numeric_limits<Scalar>::epsilon()) {
        return std::numeric_limits<Scalar>::infinity();
    }
    return a / A;
}

/**
 * Compute the MIPS (Most Isotropic Parameterizations) energy [1] of the mapping from 3D triangle to
 * 2D uv triangle.
 *
 * Let φ be the mapping, and J = ∇φ be its Jacobian.  The MIPS energy is defined as
 * Dirichlet energy / area ratio. It is equivalent to σ1/σ2 + σ2/σ1,
 * where σ1 and σ2 are the singular values of J.
 *
 * [1] Hormann, Kai, and Günther Greiner. MIPS: An efficient global parametrization method.
 * Erlangen-Nuernberg Univ (Germany) Computer Graphics Group, 2000.
 *
 * @tparam Scalar  The scalar type.
 *
 * @param V0, V1, V2  The coordinates of the 3D triangle vertices.
 * @param v0, v1, v2  The coordinates of the 2D uv triangle vertices.
 *
 * @return The MIPS energy of this triangle.
 */
template <typename Scalar>
Scalar triangle_MIPS(
    span<const Scalar, 3> V0,
    span<const Scalar, 3> V1,
    span<const Scalar, 3> V2,
    span<const Scalar, 2> v0,
    span<const Scalar, 2> v1,
    span<const Scalar, 2> v2)
{
    Scalar area_ratio = triangle_area_ratio(V0, V1, V2, v0, v1, v2);
    Scalar dirichlet = triangle_dirichlet(V0, V1, V2, v0, v1, v2);
    if (!std::isfinite(area_ratio))
        return std::numeric_limits<Scalar>::infinity();
    else
        return dirichlet / area_ratio;
}

} // namespace

template <DistortionMetric metric, typename Scalar>
Scalar triangle_uv_distortion(
    span<const Scalar, 3> V0,
    span<const Scalar, 3> V1,
    span<const Scalar, 3> V2,
    span<const Scalar, 2> v0,
    span<const Scalar, 2> v1,
    span<const Scalar, 2> v2)
{
    if constexpr (metric == DistortionMetric::Dirichlet) {
        return triangle_dirichlet(V0, V1, V2, v0, v1, v2);
    } else if constexpr (metric == DistortionMetric::InverseDirichlet) {
        return triangle_inverse_dirichlet(V0, V1, V2, v0, v1, v2);
    } else if constexpr (metric == DistortionMetric::SymmetricDirichlet) {
        return triangle_symmetric_dirichlet(V0, V1, V2, v0, v1, v2);
    } else if constexpr (metric == DistortionMetric::AreaRatio) {
        return triangle_area_ratio(V0, V1, V2, v0, v1, v2);
    } else if constexpr (metric == DistortionMetric::MIPS) {
        return triangle_MIPS(V0, V1, V2, v0, v1, v2);
    } else {
        static_assert(StaticAssertableBool<Scalar>::False, "Unsupported metric!");
    }
}

template <typename Scalar>
Scalar triangle_uv_distortion(
    span<const Scalar, 3> V0,
    span<const Scalar, 3> V1,
    span<const Scalar, 3> V2,
    span<const Scalar, 2> v0,
    span<const Scalar, 2> v1,
    span<const Scalar, 2> v2,
    DistortionMetric metric)
{
    switch (metric) {
    case DistortionMetric::Dirichlet: return triangle_dirichlet(V0, V1, V2, v0, v1, v2);
    case DistortionMetric::InverseDirichlet:
        return triangle_inverse_dirichlet(V0, V1, V2, v0, v1, v2);
    case DistortionMetric::SymmetricDirichlet:
        return triangle_symmetric_dirichlet(V0, V1, V2, v0, v1, v2);
    case DistortionMetric::AreaRatio: return triangle_area_ratio(V0, V1, V2, v0, v1, v2);
    case DistortionMetric::MIPS: return triangle_MIPS(V0, V1, V2, v0, v1, v2);
    default: throw Error("Unkown distortion measure!");
    }
}

#define LA_X_triangle_uv_distortion(_, Scalar)                                            \
    template LA_CORE_API Scalar triangle_uv_distortion<DistortionMetric::Dirichlet, Scalar>(          \
        span<const Scalar, 3>,                                                            \
        span<const Scalar, 3>,                                                            \
        span<const Scalar, 3>,                                                            \
        span<const Scalar, 2>,                                                            \
        span<const Scalar, 2>,                                                            \
        span<const Scalar, 2>);                                                           \
    template LA_CORE_API Scalar triangle_uv_distortion<DistortionMetric::InverseDirichlet, Scalar>(   \
        span<const Scalar, 3>,                                                            \
        span<const Scalar, 3>,                                                            \
        span<const Scalar, 3>,                                                            \
        span<const Scalar, 2>,                                                            \
        span<const Scalar, 2>,                                                            \
        span<const Scalar, 2>);                                                           \
    template LA_CORE_API Scalar triangle_uv_distortion<DistortionMetric::SymmetricDirichlet, Scalar>( \
        span<const Scalar, 3>,                                                            \
        span<const Scalar, 3>,                                                            \
        span<const Scalar, 3>,                                                            \
        span<const Scalar, 2>,                                                            \
        span<const Scalar, 2>,                                                            \
        span<const Scalar, 2>);                                                           \
    template LA_CORE_API Scalar triangle_uv_distortion<DistortionMetric::AreaRatio, Scalar>(          \
        span<const Scalar, 3>,                                                            \
        span<const Scalar, 3>,                                                            \
        span<const Scalar, 3>,                                                            \
        span<const Scalar, 2>,                                                            \
        span<const Scalar, 2>,                                                            \
        span<const Scalar, 2>);                                                           \
    template LA_CORE_API Scalar triangle_uv_distortion<DistortionMetric::MIPS, Scalar>(               \
        span<const Scalar, 3>,                                                            \
        span<const Scalar, 3>,                                                            \
        span<const Scalar, 3>,                                                            \
        span<const Scalar, 2>,                                                            \
        span<const Scalar, 2>,                                                            \
        span<const Scalar, 2>);                                                           \
    template LA_CORE_API Scalar triangle_uv_distortion<Scalar>(                                       \
        span<const Scalar, 3>,                                                            \
        span<const Scalar, 3>,                                                            \
        span<const Scalar, 3>,                                                            \
        span<const Scalar, 2>,                                                            \
        span<const Scalar, 2>,                                                            \
        span<const Scalar, 2>,                                                            \
        DistortionMetric);
LA_SURFACE_MESH_SCALAR_X(triangle_uv_distortion, 0);

} // namespace lagrange
