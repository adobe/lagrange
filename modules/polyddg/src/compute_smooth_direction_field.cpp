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
#include <lagrange/utils/fmt/format.h>
#include <lagrange/utils/invalid.h>
#include <lagrange/views.h>

#include <lagrange/solver/DirectSolver.h>
#include <lagrange/solver/eigen_solvers.h>

#include <cmath>
#include <vector>

namespace lagrange::polyddg {

using solver::SolverLDLT;

// =============================================================================
// solve_connection_laplacian — shared solve kernel for both vertex and facet paths
// =============================================================================

// Solves for the smoothest (or alignment-constrained) n-rosy field given a
// regularized Laplacian L_reg = L + εM and mass matrix M.
//
// When has_constraints is true, solves (L_reg) x = M q and normalizes in M-norm.
// When has_constraints is false, finds the smallest generalized eigenvector of
// L_reg x = σ M x via Spectra, falling back to inverse power iteration.
//
// fn_name is used only in runtime-assert/warning messages.
template <typename Scalar>
static void solve_connection_laplacian(
    const Eigen::SparseMatrix<Scalar>& L_reg,
    const Eigen::SparseMatrix<Scalar>& M,
    const Eigen::Matrix<Scalar, Eigen::Dynamic, 1>& q,
    bool has_constraints,
    Eigen::Index size,
    std::string_view fn_name,
    Eigen::Matrix<Scalar, Eigen::Dynamic, 1>& x)
{
    if (has_constraints) {
        SolverLDLT<Eigen::SparseMatrix<Scalar>> slv(L_reg);
        la_runtime_assert(
            slv.info() == Eigen::Success,
            lagrange::format("{}: factorization of L + eps*M failed", fn_name));

        x = slv.solve((M * q).eval());
        la_runtime_assert(
            slv.info() == Eigen::Success,
            lagrange::format("{}: constrained solve failed", fn_name));

        const Eigen::Matrix<Scalar, Eigen::Dynamic, 1> Mx = M * x;
        const Scalar x_norm_M = std::sqrt(x.dot(Mx));
        la_runtime_assert(x_norm_M > Scalar(0), lagrange::format("{}: solution is zero", fn_name));
        x /= x_norm_M;
    } else {
        bool solved = false;
        {
            auto result = solver::generalized_selfadjoint_eigen_smallest(L_reg, M, 1);
            if (result.is_successful() && result.num_converged >= 1) {
                x = result.eigenvectors.col(0);
                solved = true;
            } else {
                logger().warn(
                    "{}: Spectra eigen solver did not converge, falling back to inverse power "
                    "iteration.",
                    fn_name);
            }
        }

        if (!solved) {
            SolverLDLT<Eigen::SparseMatrix<Scalar>> slv(L_reg);
            la_runtime_assert(
                slv.info() == Eigen::Success,
                lagrange::format("{}: Cholesky factorization of L + eps*M failed", fn_name));

            x = Eigen::Matrix<Scalar, Eigen::Dynamic, 1>::Ones(size);
            x.normalize();
            constexpr int max_iter = 20;
            for (int iter = 0; iter < max_iter; ++iter) {
                x = M * x;
                x = slv.solve(x);
                x.normalize();
            }
        }
    }
}

// =============================================================================
// compute_smooth_direction_field_on_facets  (face-based connection Laplacian)
// =============================================================================

template <typename Scalar, typename Index>
AttributeId compute_smooth_direction_field_on_facets(
    SurfaceMesh<Scalar, Index>& mesh,
    const DifferentialOperators<Scalar, Index>& ops,
    SmoothDirectionFieldOptions options)
{
    la_runtime_assert(
        options.nrosy >= 1,
        "compute_smooth_direction_field_on_facets: nrosy must be >= 1.");

    const Index num_facets = mesh.get_num_facets();
    const Index num_edges = mesh.get_num_edges();
    const Index n = static_cast<Index>(options.nrosy);
    const int n_int = static_cast<int>(options.nrosy);

    // ---- 1. Edge-to-face mapping ----
    // For each edge store the local vertex index of edge.v0 in each adjacent face.
    // edge_faces[eid][0/1] = {fid, lv_v0} where lv_v0 is the local index of the canonical
    // edge endpoint v0 inside face fid.  Slot fid == kInvalid means no face assigned yet.
    const Index kInvalid = invalid<Index>();
    struct FaceSlot
    {
        Index fid;
        Index lv_v0;
    };
    std::vector<std::array<FaceSlot, 2>> edge_faces(
        num_edges,
        {FaceSlot{kInvalid, 0}, FaceSlot{kInvalid, 0}});

    for (Index fid = 0; fid < num_facets; ++fid) {
        const Index ns = mesh.get_facet_size(fid);
        for (Index lv = 0; lv < ns; ++lv) {
            const Index eid = mesh.get_edge(fid, lv);
            auto [v0, v1] = mesh.get_edge_vertices(eid);
            const Index vid_at_lv = mesh.get_facet_vertex(fid, lv);
            // Edge lv runs from vid_at_lv → next vertex; find which is v0.
            const Index lv_v0 = (v0 == vid_at_lv) ? lv : (lv + 1) % ns;

            auto& slot = edge_faces[eid];
            if (slot[0].fid == kInvalid) {
                slot[0] = {fid, lv_v0};
            } else {
                la_runtime_assert(
                    slot[1].fid == kInvalid,
                    "compute_smooth_direction_field_on_facets: non-manifold edge detected (more "
                    "than 2 incident faces). Only edge-manifold meshes are supported.");
                slot[1] = {fid, lv_v0};
            }
        }
    }

    // ---- 2. Face-based n-fold connection Laplacian L_n (2F × 2F) ----
    // For interior edge e = (f0, f1):
    //   R_{f0→f1}^n = levi_civita_nrosy(f1, lv0_in_f1, n) · levi_civita_nrosy(f0, lv0_in_f0,
    //   n)^T
    //
    // Energy contribution: w_e · ||R_{f0→f1}^n u_{f0} − u_{f1}||²
    // gives diagonal blocks +w_e I₂ and off-diagonal blocks ∓w_e R_{f0→f1}^{n,T} / R_{f0→f1}^n.
    std::vector<Eigen::Triplet<Scalar>> L_triplets;
    L_triplets.reserve(8 * num_edges + 4 * num_facets);

    for (Index eid = 0; eid < num_edges; ++eid) {
        const auto& s0 = edge_faces[eid][0];
        const auto& s1 = edge_faces[eid][1];
        if (s0.fid == kInvalid || s1.fid == kInvalid) continue; // boundary edge

        const Index f0 = s0.fid, lv0_in_f0 = s0.lv_v0;
        const Index f1 = s1.fid, lv0_in_f1 = s1.lv_v0;

        // n-fold Levi-Civita transport: f0's frame → f1's frame, via shared vertex v0.
        // Eager-evaluate to a concrete matrix: levi_civita_nrosy returns by value, so the
        // product expression would otherwise hold references to destroyed temporaries.
        const Eigen::Matrix<Scalar, 2, 2> R01 = ops.levi_civita_nrosy(f1, lv0_in_f1, n) *
                                                ops.levi_civita_nrosy(f0, lv0_in_f0, n).transpose();

        // Weight = primal edge length.
        auto [v0, v1] = mesh.get_edge_vertices(eid);
        auto p0 = mesh.get_position(v0);
        auto p1 = mesh.get_position(v1);
        const Scalar w_e = std::sqrt(
            (p1[0] - p0[0]) * (p1[0] - p0[0]) + (p1[1] - p0[1]) * (p1[1] - p0[1]) +
            (p1[2] - p0[2]) * (p1[2] - p0[2]));

        const Eigen::Index ef0 = static_cast<Eigen::Index>(f0);
        const Eigen::Index ef1 = static_cast<Eigen::Index>(f1);

        // Diagonal blocks: +w_e I₂
        for (Eigen::Index d = 0; d < 2; ++d) {
            L_triplets.emplace_back(2 * ef0 + d, 2 * ef0 + d, w_e);
            L_triplets.emplace_back(2 * ef1 + d, 2 * ef1 + d, w_e);
        }

        // Off-diagonal blocks:
        //   L[f0, f1] -= w_e · R01^T    (R01^T[i,j] = R01[j,i])
        //   L[f1, f0] -= w_e · R01
        for (Eigen::Index i = 0; i < 2; ++i) {
            for (Eigen::Index j = 0; j < 2; ++j) {
                L_triplets.emplace_back(2 * ef0 + i, 2 * ef1 + j, -w_e * R01(j, i)); // R01^T
                L_triplets.emplace_back(2 * ef1 + i, 2 * ef0 + j, -w_e * R01(i, j));
            }
        }
    }

    Eigen::SparseMatrix<Scalar> Ln(
        static_cast<Eigen::Index>(2 * num_facets),
        static_cast<Eigen::Index>(2 * num_facets));
    Ln.setFromTriplets(L_triplets.begin(), L_triplets.end());

    // ---- 3. Mass matrix M (2F × 2F, face areas on diagonal) ----
    auto M2 = ops.star2();
    std::vector<Eigen::Triplet<Scalar>> M_triplets;
    M_triplets.reserve(2 * num_facets);
    for (Eigen::Index fid = 0; fid < static_cast<Eigen::Index>(num_facets); ++fid) {
        const Scalar a = M2.coeff(fid, fid);
        M_triplets.emplace_back(2 * fid, 2 * fid, a);
        M_triplets.emplace_back(2 * fid + 1, 2 * fid + 1, a);
    }
    Eigen::SparseMatrix<Scalar> M(
        static_cast<Eigen::Index>(2 * num_facets),
        static_cast<Eigen::Index>(2 * num_facets));
    M.setFromTriplets(M_triplets.begin(), M_triplets.end());

    constexpr Scalar eps = Scalar(1e-8);
    Eigen::SparseMatrix<Scalar> L_reg = Ln + eps * M;

    Eigen::Matrix<Scalar, Eigen::Dynamic, 1> x(static_cast<Eigen::Index>(2 * num_facets));
    bool has_constraints = !options.alignment_attribute.empty();
    Eigen::Matrix<Scalar, Eigen::Dynamic, 1> q;

    if (has_constraints) {
        // --- Build alignment rhs q ---
        // Reads per-facet prescribed 3-D tangent vectors (zero = unconstrained),
        // encodes them in n-fold space for the rhs M q.
        const auto alignment_id = internal::find_attribute<Scalar>(
            mesh,
            options.alignment_attribute,
            AttributeElement::Facet,
            AttributeUsage::Vector,
            3);
        la_runtime_assert(
            alignment_id != invalid_attribute_id(),
            "compute_smooth_direction_field_on_facets: alignment attribute not found or does "
            "not "
            "match expected properties (must be a Vector Facet attribute with 3 channels).");

        auto align_data = attribute_matrix_view<Scalar>(mesh, alignment_id);

        q.resize(static_cast<Eigen::Index>(2 * num_facets));
        q.setZero();

        for (Index fid = 0; fid < num_facets; ++fid) {
            Eigen::Matrix<Scalar, 3, 1> v3 = align_data.row(fid).transpose();
            if (v3.squaredNorm() < Scalar(1e-20)) continue;

            Eigen::Matrix<Scalar, 3, 2> B = ops.facet_basis(fid);
            Eigen::Matrix<Scalar, 2, 1> v2 = (B.transpose() * v3).stableNormalized();
            q.template segment<2>(static_cast<Eigen::Index>(2 * fid)) = nrosy_encode(v2, n_int);
        }

        if (q.squaredNorm() == Scalar(0)) {
            logger().warn(
                "compute_smooth_direction_field_on_facets: all alignment vectors are zero or "
                "near-zero; falling back to unconstrained solve.");
            has_constraints = false;
        }
    }

    solve_connection_laplacian(
        L_reg,
        M,
        q,
        has_constraints,
        static_cast<Eigen::Index>(2 * num_facets),
        "compute_smooth_direction_field_on_facets",
        x);

    // ---- 4. Decode and store as per-facet 3-D tangent vector ----
    const std::string_view out_name =
        (options.direction_field_attribute && !options.direction_field_attribute->empty())
            ? *options.direction_field_attribute
            : "@smooth_direction_field_facets";
    const auto attr_id = internal::find_or_create_attribute<Scalar>(
        mesh,
        out_name,
        AttributeElement::Facet,
        AttributeUsage::Vector,
        3,
        internal::ResetToDefault::No);

    auto out_data = attribute_matrix_ref<Scalar>(mesh, attr_id);

    for (Index fid = 0; fid < num_facets; ++fid) {
        Eigen::Matrix<Scalar, 2, 1> u2 = x.template segment<2>(static_cast<Eigen::Index>(2 * fid));
        if (n > 1) u2 = nrosy_decode(u2, n_int);
        Eigen::Matrix<Scalar, 3, 2> B = ops.facet_basis(fid);
        out_data.row(fid) = (B * u2).stableNormalized().transpose();
    }

    return attr_id;
}


// =============================================================================
// compute_smooth_direction_field  (vertex-based, Knöppel et al. 2013)
// =============================================================================

template <typename Scalar, typename Index>
AttributeId compute_smooth_direction_field(
    SurfaceMesh<Scalar, Index>& mesh,
    const DifferentialOperators<Scalar, Index>& ops,
    SmoothDirectionFieldOptions options)
{
    la_runtime_assert(options.nrosy >= 1, "compute_smooth_direction_field: nrosy must be >= 1.");

    if (options.output_element_type == AttributeElement::Facet) {
        if (options.lambda != SmoothDirectionFieldOptions{}.lambda) {
            logger().warn(
                "compute_smooth_direction_field: lambda is ignored for Facet output (vertex-based "
                "path only).");
        }
        return compute_smooth_direction_field_on_facets(mesh, ops, std::move(options));
    }
    la_runtime_assert(
        options.output_element_type == AttributeElement::Vertex,
        "compute_smooth_direction_field: output_element_type must be Vertex or Facet.");

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
    for (Eigen::Index vid = 0; vid < static_cast<Eigen::Index>(num_vertices); ++vid) {
        const Scalar m = M0.coeff(vid, vid);
        M_triplets.emplace_back(2 * vid, 2 * vid, m);
        M_triplets.emplace_back(2 * vid + 1, 2 * vid + 1, m);
    }
    Eigen::SparseMatrix<Scalar> M(
        static_cast<Eigen::Index>(num_vertices * 2),
        static_cast<Eigen::Index>(num_vertices * 2));
    M.setFromTriplets(M_triplets.begin(), M_triplets.end());

    // The system matrix L is positive semi-definite. Add a small multiple of M to make
    // it strictly positive definite for Cholesky / LDLT (Knöppel et al. 2013, Algorithm 1
    // setup: A ← A + εM with ε = 1e-8).
    constexpr Scalar eps = Scalar(1e-8);
    Eigen::SparseMatrix<Scalar> L_reg = L + eps * M;

    Eigen::Matrix<Scalar, Eigen::Dynamic, 1> x(static_cast<Eigen::Index>(num_vertices * 2));
    bool has_constraints = !options.alignment_attribute.empty();
    Eigen::Matrix<Scalar, Eigen::Dynamic, 1> q;

    if (has_constraints) {
        // --- Build alignment rhs q (Knöppel et al. 2013, Eq. 16 / Algorithm 3) ---
        //
        // Prescribed unit tangents at constrained vertices in n-rosy encoded space,
        // zero elsewhere.

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

        q.resize(static_cast<Eigen::Index>(num_vertices * 2));
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

        if (q.squaredNorm() == Scalar(0)) {
            logger().warn(
                "compute_smooth_direction_field: all alignment vectors are zero or near-zero; "
                "falling back to unconstrained solve.");
            has_constraints = false;
        }
    }

    solve_connection_laplacian(
        L_reg,
        M,
        q,
        has_constraints,
        static_cast<Eigen::Index>(num_vertices * 2),
        "compute_smooth_direction_field",
        x);

    // Create or reuse the output attribute (3-D vector per vertex).
    const std::string_view out_name =
        (options.direction_field_attribute && !options.direction_field_attribute->empty())
            ? *options.direction_field_attribute
            : "@smooth_direction_field";
    const auto direction_field_id = internal::find_or_create_attribute<Scalar>(
        mesh,
        out_name,
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

template <typename Scalar, typename Index>
AttributeId compute_smooth_direction_field(
    SurfaceMesh<Scalar, Index>& mesh,
    SmoothDirectionFieldOptions options)
{
    DifferentialOperators<Scalar, Index> ops(mesh);
    return compute_smooth_direction_field(mesh, ops, std::move(options));
}

#define LA_X_compute_smooth_direction_field(_, Scalar, Index)                          \
    template LA_POLYDDG_API AttributeId compute_smooth_direction_field<Scalar, Index>( \
        SurfaceMesh<Scalar, Index>&,                                                   \
        const DifferentialOperators<Scalar, Index>&,                                   \
        SmoothDirectionFieldOptions);                                                  \
    template LA_POLYDDG_API AttributeId compute_smooth_direction_field<Scalar, Index>( \
        SurfaceMesh<Scalar, Index>&,                                                   \
        SmoothDirectionFieldOptions);
LA_SURFACE_MESH_X(compute_smooth_direction_field, 0)


} // namespace lagrange::polyddg
