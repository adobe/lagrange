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
#include <lagrange/polyddg/compute_principal_curvatures.h>

#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/internal/find_attribute_utils.h>
#include <lagrange/utils/warning.h>
#include <lagrange/views.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <tbb/parallel_for.h>
#include <lagrange/utils/warnon.h>
// clang-format on

// Include early to silence -Wmaybe-uninitialized with GCC 13 (see DifferentialOperators.cpp).
LA_IGNORE_MAYBE_UNINITIALIZED_START
#include <Eigen/Eigenvalues>
LA_IGNORE_MAYBE_UNINITIALIZED_END

#include <limits>
#include <utility>

namespace lagrange::polyddg {

template <typename Scalar, typename Index>
PrincipalCurvaturesResult compute_principal_curvatures(
    SurfaceMesh<Scalar, Index>& mesh,
    const DifferentialOperators<Scalar, Index>& ops,
    PrincipalCurvaturesOptions options)
{
    const Index num_vertices = mesh.get_num_vertices();

    // Create or reuse output attributes.
    const auto kappa_min_id = internal::find_or_create_attribute<Scalar>(
        mesh,
        options.kappa_min_attribute,
        AttributeElement::Vertex,
        AttributeUsage::Scalar,
        1,
        internal::ResetToDefault::No);
    const auto kappa_max_id = internal::find_or_create_attribute<Scalar>(
        mesh,
        options.kappa_max_attribute,
        AttributeElement::Vertex,
        AttributeUsage::Scalar,
        1,
        internal::ResetToDefault::No);
    const auto direction_min_id = internal::find_or_create_attribute<Scalar>(
        mesh,
        options.direction_min_attribute,
        AttributeElement::Vertex,
        AttributeUsage::Vector,
        3,
        internal::ResetToDefault::No);
    const auto direction_max_id = internal::find_or_create_attribute<Scalar>(
        mesh,
        options.direction_max_attribute,
        AttributeElement::Vertex,
        AttributeUsage::Vector,
        3,
        internal::ResetToDefault::No);

    auto kappa_min_data = attribute_matrix_ref<Scalar>(mesh, kappa_min_id);
    auto kappa_max_data = attribute_matrix_ref<Scalar>(mesh, kappa_max_id);
    auto direction_min_data = attribute_matrix_ref<Scalar>(mesh, direction_min_id);
    auto direction_max_data = attribute_matrix_ref<Scalar>(mesh, direction_max_id);

    // Each vertex is independent: reads from mesh attributes (const) and writes to distinct rows.
    tbb::parallel_for(Index(0), num_vertices, [&](Index vid) {
        // Eigendecompose the symmetric 2×2 adjoint shape operator.
        // SelfAdjointEigenSolver returns eigenvalues sorted in ascending order.
        Eigen::Matrix<Scalar, 2, 2> S = ops.adjoint_shape_operator(vid);
        Eigen::SelfAdjointEigenSolver<Eigen::Matrix<Scalar, 2, 2>> solver(S);

        if (solver.info() != Eigen::Success) {
            // Eigen-decomposition failed (e.g. NaN/Inf in S from degenerate geometry).
            // Write sentinel NaNs and skip normalization.
            logger().warn(
                "compute_principal_curvatures: eigen-decomposition failed for vertex {}",
                vid);
            kappa_min_data(vid, 0) = std::numeric_limits<Scalar>::quiet_NaN();
            kappa_max_data(vid, 0) = std::numeric_limits<Scalar>::quiet_NaN();
            direction_min_data.row(vid).setConstant(std::numeric_limits<Scalar>::quiet_NaN());
            direction_max_data.row(vid).setConstant(std::numeric_limits<Scalar>::quiet_NaN());
        } else {
            kappa_min_data(vid, 0) = solver.eigenvalues()(0);
            kappa_max_data(vid, 0) = solver.eigenvalues()(1);

            // Map 2-D eigenvectors back to 3-D through the vertex tangent basis.
            Eigen::Matrix<Scalar, 3, 2> B = ops.vertex_basis(vid);
            LA_IGNORE_ARRAY_BOUNDS_BEGIN
            direction_min_data.row(vid) =
                (B * solver.eigenvectors().col(0)).normalized().transpose();
            direction_max_data.row(vid) =
                (B * solver.eigenvectors().col(1)).normalized().transpose();
            LA_IGNORE_ARRAY_BOUNDS_END
        }
    });

    return {kappa_min_id, kappa_max_id, direction_min_id, direction_max_id};
}

template <typename Scalar, typename Index>
PrincipalCurvaturesResult compute_principal_curvatures(
    SurfaceMesh<Scalar, Index>& mesh,
    PrincipalCurvaturesOptions options)
{
    DifferentialOperators<Scalar, Index> ops(mesh);
    return compute_principal_curvatures(mesh, ops, std::move(options));
}

#define LA_X_compute_principal_curvatures(_, Scalar, Index)                                        \
    template LA_POLYDDG_API PrincipalCurvaturesResult compute_principal_curvatures<Scalar, Index>( \
        SurfaceMesh<Scalar, Index>&,                                                               \
        const DifferentialOperators<Scalar, Index>&,                                               \
        PrincipalCurvaturesOptions);                                                               \
    template LA_POLYDDG_API PrincipalCurvaturesResult compute_principal_curvatures<Scalar, Index>( \
        SurfaceMesh<Scalar, Index>&,                                                               \
        PrincipalCurvaturesOptions);
LA_SURFACE_MESH_X(compute_principal_curvatures, 0)

} // namespace lagrange::polyddg
