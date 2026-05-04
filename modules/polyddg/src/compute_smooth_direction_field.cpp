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
#include <lagrange/polyddg/compute_smooth_direction_field.h>

#include "nrosy_utils.h"

#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/internal/find_attribute_utils.h>
#include <lagrange/utils/assert.h>
#include <lagrange/views.h>

#include <lagrange/solver/DirectSolver.h>
#include <lagrange/solver/eigen_solvers.h>

#include <cmath>
#include <vector>

namespace lagrange::polyddg {

using solver::SolverLDLT;

template <typename Scalar, typename Index>
AttributeId compute_smooth_direction_field(
    SurfaceMesh<Scalar, Index>& mesh,
    const DifferentialOperators<Scalar, Index>& ops,
    SmoothDirectionFieldOptions options)
{
    la_runtime_assert(options.nrosy >= 1, "compute_smooth_direction_field: nrosy must be >= 1.");

    const Index num_vertices = mesh.get_num_vertices();
    const Index n = static_cast<Index>(options.nrosy);
    const int n_int = static_cast<int>(options.nrosy);
    const Scalar lambda = static_cast<Scalar>(options.lambda);

    // Build the connection Laplacian L of size (#V*2) x (#V*2).
    auto L = ops.connection_laplacian_nrosy(n, lambda);

    // Build the mass matrix M of size (#V*2) x (#V*2) by expanding star0() (which is
    // #V x #V diagonal) into 2x2 identity blocks scaled by the per-vertex area.
    auto M0 = ops.star0();
    std::vector<Eigen::Triplet<Scalar>> M_triplets;
    M_triplets.reserve(num_vertices * 2);
    for (Index vid = 0; vid < num_vertices; ++vid) {
        const Scalar m = M0.coeff(static_cast<Eigen::Index>(vid), static_cast<Eigen::Index>(vid));
        M_triplets.emplace_back(
            static_cast<Eigen::Index>(vid * 2),
            static_cast<Eigen::Index>(vid * 2),
            m);
        M_triplets.emplace_back(
            static_cast<Eigen::Index>(vid * 2 + 1),
            static_cast<Eigen::Index>(vid * 2 + 1),
            m);
    }
    Eigen::SparseMatrix<Scalar> M(
        static_cast<Eigen::Index>(num_vertices * 2),
        static_cast<Eigen::Index>(num_vertices * 2));
    M.setFromTriplets(M_triplets.begin(), M_triplets.end());

    // --- Compute the smallest eigenvector/eigenvalue of L u = σ M u ---
    //
    // This is needed for both the unconstrained case (the result IS the smoothest field)
    // and the constrained case (σ_min sets the spectral shift for alignment).
    Eigen::Matrix<Scalar, Eigen::Dynamic, 1> x(static_cast<Eigen::Index>(num_vertices * 2));
    Scalar sigma_min = Scalar(0);
    {
        constexpr Scalar eps = Scalar(1e-8);
        Eigen::SparseMatrix<Scalar> L_reg = L + eps * M;
        bool solved = false;

        {
            auto result = solver::generalized_selfadjoint_eigen_smallest(L_reg, M, 1);
            if (result.is_successful() && result.num_converged >= 1) {
                x = result.eigenvectors.col(0);
                sigma_min = result.eigenvalues(0) - eps;
                solved = true;
            } else {
                logger().warn(
                    "compute_smooth_direction_field: Spectra eigen solver did not converge, "
                    "falling back to inverse power iteration.");
            }
        }

        if (!solved) {
            // Fallback: inverse power iteration.
            SolverLDLT<Eigen::SparseMatrix<Scalar>> solver(L_reg);
            la_runtime_assert(
                solver.info() == Eigen::Success,
                "compute_smooth_direction_field: Cholesky factorization of L + eps*M failed");

            x = Eigen::Matrix<Scalar, Eigen::Dynamic, 1>::Ones(
                static_cast<Eigen::Index>(num_vertices * 2));
            x.normalize();

            constexpr int max_iter = 20;
            for (int iter = 0; iter < max_iter; ++iter) {
                x = M * x;
                x = solver.solve(x);
                x.normalize();
            }
            // Estimate σ_min via Rayleigh quotient: σ = (x^T L x) / (x^T M x).
            sigma_min = x.dot(L * x) / x.dot(M * x);
        }
    }

    const bool has_constraints = !options.alignment_attribute.empty();

    if (has_constraints) {
        // --- Constrained solve: (L - α*M + ε*M) u = M*q  ---
        //
        // Following fieldgen (Knöppel et al. 2013), the spectral shift α = σ_min makes
        // (L - α*M) singular along the smoothest mode, maximally biasing the solution
        // toward the prescribed field q. The small ε*M regularization prevents exact
        // singularity. alignment_weight scales the shift (1.0 = full fieldgen shift).
        la_runtime_assert(
            options.alignment_weight > 0 && options.alignment_weight <= 1.0,
            "compute_smooth_direction_field: alignment_weight must be in (0, 1].");

        const Scalar alpha =
            static_cast<Scalar>(options.alignment_weight) * std::max(sigma_min, Scalar(0));
        constexpr Scalar eps = Scalar(1e-8);

        const auto alignment_id = internal::find_attribute<Scalar>(
            mesh,
            options.alignment_attribute,
            AttributeElement::Vertex,
            AttributeUsage::Vector,
            3);
        la_runtime_assert(
            alignment_id != invalid_attribute_id(),
            "compute_smooth_direction_field: alignment attribute not found or does not match "
            "expected properties (must be a Vector Vertex attribute with 3 channels).");

        auto align_data = attribute_matrix_view<Scalar>(mesh, alignment_id);

        Eigen::Matrix<Scalar, Eigen::Dynamic, 1> q(static_cast<Eigen::Index>(num_vertices * 2));
        q.setZero();

        for (Index vid = 0; vid < num_vertices; ++vid) {
            Eigen::Matrix<Scalar, 3, 1> v3 = align_data.row(vid).transpose();
            if (v3.squaredNorm() < Scalar(1e-20)) continue; // unconstrained vertex

            // Project into the local 2-D tangent frame (vertex_basis is orthonormal).
            Eigen::Matrix<Scalar, 3, 2> B = ops.vertex_basis(vid);
            Eigen::Matrix<Scalar, 2, 1> v2 = (B.transpose() * v3).stableNormalized();

            // Apply n-fold symmetry encoding: [cos(n*theta), sin(n*theta)].
            q.template segment<2>(static_cast<Eigen::Index>(vid * 2)) = nrosy_encode(v2, n_int);
        }

        // M-normalize q so ||q||_M = 1 (following fieldgen).
        Scalar norm_q = std::sqrt(q.dot(M * q));
        la_runtime_assert(
            norm_q > Scalar(1e-10),
            "compute_smooth_direction_field: all alignment vectors are zero or near-zero");
        q /= norm_q;

        // Assemble and factor the shifted system matrix: L - α*M + ε*M.
        Eigen::SparseMatrix<Scalar> L_shifted = L - (alpha - eps) * M;
        SolverLDLT<Eigen::SparseMatrix<Scalar>> solver(L_shifted);
        la_runtime_assert(
            solver.info() == Eigen::Success,
            "compute_smooth_direction_field: factorization of L - alpha*M failed.");

        Eigen::Matrix<Scalar, Eigen::Dynamic, 1> Mq = M * q;
        x = solver.solve(Mq);
        la_runtime_assert(
            solver.info() == Eigen::Success,
            "compute_smooth_direction_field: constrained solve failed");
    }
    // else: x already holds the unconstrained smallest eigenvector from above.

    // Create or reuse the output attribute (3-D vector per vertex).
    const auto direction_field_id = internal::find_or_create_attribute<Scalar>(
        mesh,
        options.direction_field_attribute,
        AttributeElement::Vertex,
        AttributeUsage::Vector,
        3,
        internal::ResetToDefault::No);

    auto direction_data = attribute_matrix_ref<Scalar>(mesh, direction_field_id);

    // Decode the n-fold representation and map to 3-D world-space tangent vectors.
    // The solution x lives in the n-rosy encoded space: each vertex's 2-D component is
    // (cos(n*θ), sin(n*θ)) where θ is the actual direction angle. Decode by dividing
    // the angle by n to recover one representative direction of the n-rosy field.
    for (Index vid = 0; vid < num_vertices; ++vid) {
        Eigen::Matrix<Scalar, 2, 1> u2 = x.template segment<2>(static_cast<Eigen::Index>(vid * 2));
        if (n > 1) {
            u2 = nrosy_decode(u2, n_int);
        }
        Eigen::Matrix<Scalar, 3, 2> B = ops.vertex_basis(vid);
        direction_data.row(vid) = (B * u2).stableNormalized().transpose();
    }

    return direction_field_id;
}

#define LA_X_compute_smooth_direction_field(_, Scalar, Index)                          \
    template LA_POLYDDG_API AttributeId compute_smooth_direction_field<Scalar, Index>( \
        SurfaceMesh<Scalar, Index>&,                                                   \
        const DifferentialOperators<Scalar, Index>&,                                   \
        SmoothDirectionFieldOptions);
LA_SURFACE_MESH_X(compute_smooth_direction_field, 0)

} // namespace lagrange::polyddg
