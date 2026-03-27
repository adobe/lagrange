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
#include <lagrange/polyddg/hodge_decomposition.h>

#include "nrosy_utils.h"

#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/compute_components.h>
#include <lagrange/internal/find_attribute_utils.h>
#include <lagrange/topology.h>
#include <lagrange/utils/assert.h>
#include <lagrange/views.h>

#include <lagrange/solver/DirectSolver.h>

#include <Eigen/SparseLU>

#include <cmath>
#include <utility>
#include <vector>

namespace lagrange::polyddg {

using solver::SolverLDLT;

// =============================================================================
// 1-form level
// =============================================================================

template <typename Scalar, typename Index>
HodgeDecompositionResult hodge_decomposition_1_form(
    SurfaceMesh<Scalar, Index>& mesh,
    const DifferentialOperators<Scalar, Index>& ops,
    HodgeDecompositionOptions options)
{
    using VectorX = Eigen::Matrix<Scalar, Eigen::Dynamic, 1>;
    using SpMat = Eigen::SparseMatrix<Scalar>;

    const auto lambda = static_cast<Scalar>(options.lambda);

    // Read input 1-form (per-edge scalar).
    const auto input_id = internal::find_attribute<Scalar>(
        mesh,
        options.input_attribute,
        AttributeElement::Edge,
        AttributeUsage::Scalar,
        1);
    la_runtime_assert(
        input_id != invalid_attribute_id(),
        "hodge_decomposition_1_form: input attribute not found or does not match expected "
        "properties (must be a Scalar Edge attribute with 1 channel).");

    const auto input_view = attribute_matrix_view<Scalar>(mesh, input_id);
    const VectorX omega = input_view.col(0);

    la_runtime_assert(
        is_closed(mesh),
        "hodge_decomposition_1_form: mesh must be closed (no boundary edges).");

    la_runtime_assert(
        compute_components(mesh) == 1,
        "hodge_decomposition_1_form: mesh must have a single connected component.");

    // Discrete operators.
    const SpMat D0 = ops.d0(); // #E × #V
    const SpMat D1 = ops.d1(); // #F × #E

    const Eigen::Index nv = static_cast<Eigen::Index>(mesh.get_num_vertices());
    const Eigen::Index ne = static_cast<Eigen::Index>(mesh.get_num_edges());
    const Eigen::Index nf = static_cast<Eigen::Index>(mesh.get_num_facets());

    // ---- Step 1: Exact part ----
    // Solve L₀ α = δ₁ ω,  L₀ = d0ᵀ M₁ d0  (#V × #V, SPSD)
    // ops.laplacian(λ) and ops.delta1(λ) both use M₁ = star1(λ), so the systems are
    // consistent with the 1-form inner product.
    //
    // Uniqueness is enforced by requiring the average of α to be zero.  This is
    // encoded as a Lagrange multiplier augmenting L₀ to a (nv+1)×(nv+1) system:
    //
    //   [ L₀   1 ] [ α ]   [ δ₁ω ]
    //   [ 1ᵀ   0 ] [ λ ] = [  0  ]
    //
    // The matrix is symmetric indefinite (one negative pivot from the constraint)
    // and is solved with SimplicialLDLT, which factors as LDLᵀ and handles
    // negative diagonal entries in D.
    SpMat L0 = ops.laplacian(lambda);
    const VectorX rhs0 = ops.delta1(lambda) * omega;

    SpMat L0_aug(nv + 1, nv + 1);
    {
        std::vector<Eigen::Triplet<Scalar>> triplets;
        triplets.reserve(L0.nonZeros() + 2 * nv);
        // L0 block (#V × #V)
        for (int k = 0; k < L0.outerSize(); ++k)
            for (typename SpMat::InnerIterator it(L0, k); it; ++it)
                triplets.emplace_back(it.row(), it.col(), it.value());
        // Zero-mean constraint: sum(alpha) = 0
        // Add row and column of 1s
        for (Eigen::Index i = 0; i < nv; ++i) {
            triplets.emplace_back(nv, i, Scalar(1));
            triplets.emplace_back(i, nv, Scalar(1));
        }
        L0_aug.setFromTriplets(triplets.begin(), triplets.end());
    }
    VectorX rhs0_aug(nv + 1);
    rhs0_aug.head(nv) = rhs0;
    rhs0_aug(nv) = Scalar(0);

    SolverLDLT<SpMat> solver0(L0_aug);
    la_runtime_assert(
        solver0.info() == Eigen::Success,
        "hodge_decomposition_1_form: scalar Laplacian factorization failed.");
    const VectorX alpha = solver0.solve(rhs0_aug).head(nv);
    la_runtime_assert(
        solver0.info() == Eigen::Success,
        "hodge_decomposition_1_form: scalar Laplacian solve failed.");

    const VectorX omega_exact = D0 * alpha;

    // ---- Step 2: Co-exact part ----
    // M₁ = star1(λ) is SPD but non-diagonal for polygonal meshes, so we cannot
    // invert it element-wise. Instead solve the symmetric saddle-point system:
    //
    //   [ M₁   D1ᵀ ] [ ω_coexact ]   [  0   ]
    //   [ D1    0  ] [    ψ      ] = [ D1 ω ]
    //
    // Row 1: M₁ ω_coexact + D1ᵀ ψ = 0  ⟹  ω_coexact = −M₁⁻¹ D1ᵀ ψ
    // Row 2: D1 ω_coexact = D1 ω
    //
    // The matrix is symmetric indefinite (SPD upper-left block, zero lower-right block)
    // and is solved with SparseLU (LU with partial pivoting), which handles non-SPD
    // sparse systems correctly.  Gauge: pin ψ[0] = 0 to remove the constant null space
    // of D1ᵀ on closed meshes.  This is done by skipping the first column of D1ᵀ and
    // the first row of D1 during assembly, then inserting a 1 on the diagonal at (ne, ne).
    const SpMat M1 = ops.star1(lambda);
    const SpMat D1T = D1.transpose();
    const Eigen::Index total = ne + nf;

    SpMat A(total, total);
    {
        std::vector<Eigen::Triplet<Scalar>> triplets;
        triplets.reserve(M1.nonZeros() + 2 * D1.nonZeros() + 1);
        // M1 block (#E × #E)
        for (int k = 0; k < M1.outerSize(); ++k)
            for (typename SpMat::InnerIterator it(M1, k); it; ++it)
                triplets.emplace_back(it.row(), it.col(), it.value());
        // D1^T block (#E × #F), skip column 0 (gauge fix: lambda[0] = 0)
        for (int k = 0; k < D1T.outerSize(); ++k)
            for (typename SpMat::InnerIterator it(D1T, k); it; ++it)
                if (it.col() != 0) triplets.emplace_back(it.row(), ne + it.col(), it.value());
        // D1 block (#F × #E), skip row 0 (corresponding to lambda[0])
        for (int k = 0; k < D1.outerSize(); ++k)
            for (typename SpMat::InnerIterator it(D1, k); it; ++it)
                if (it.row() != 0) triplets.emplace_back(ne + it.row(), it.col(), it.value());
        // Dummy equation at (ne, ne) to fix lambda[0] = 0
        triplets.emplace_back(ne, ne, Scalar(1));
        A.setFromTriplets(triplets.begin(), triplets.end());
    }

    VectorX rhs_aug = VectorX::Zero(total);
    rhs_aug.tail(nf) = D1 * omega;
    rhs_aug(ne) = Scalar(0); // lambda[0] = 0

    Eigen::SparseLU<SpMat> solver1;
    solver1.compute(A);
    la_runtime_assert(
        solver1.info() == Eigen::Success,
        "hodge_decomposition_1_form: saddle-point factorization failed.");
    const VectorX sol = solver1.solve(rhs_aug);
    la_runtime_assert(
        solver1.info() == Eigen::Success,
        "hodge_decomposition_1_form: saddle-point solve failed.");

    const VectorX omega_coexact = sol.head(ne);

    // ---- Step 3: Harmonic part ----
    // The harmonic component is the residual after removing exact and co-exact parts.
    // For genus-0 closed surfaces, this should be nearly zero (within numerical precision).
    // For higher genus surfaces, this captures the non-trivial harmonic 1-forms.
    const VectorX omega_harmonic = omega - omega_exact - omega_coexact;

    // ---- Write output per-edge scalar attributes ----
    auto write_edge_attr = [&](std::string_view name, const VectorX& values) -> AttributeId {
        const auto id = internal::find_or_create_attribute<Scalar>(
            mesh,
            name,
            AttributeElement::Edge,
            AttributeUsage::Scalar,
            1,
            internal::ResetToDefault::No);
        auto ref = attribute_matrix_ref<Scalar>(mesh, id);
        ref.col(0) = values;
        return id;
    };

    HodgeDecompositionResult result;
    result.exact_id = write_edge_attr(options.exact_attribute, omega_exact);
    result.coexact_id = write_edge_attr(options.coexact_attribute, omega_coexact);
    result.harmonic_id = write_edge_attr(options.harmonic_attribute, omega_harmonic);
    return result;
}

template <typename Scalar, typename Index>
HodgeDecompositionResult hodge_decomposition_1_form(
    SurfaceMesh<Scalar, Index>& mesh,
    HodgeDecompositionOptions options)
{
    DifferentialOperators<Scalar, Index> ops(mesh);
    return hodge_decomposition_1_form(mesh, ops, std::move(options));
}

// =============================================================================
// Per-vertex vector field level
// =============================================================================

template <typename Scalar, typename Index>
HodgeDecompositionResult hodge_decomposition_vector_field(
    SurfaceMesh<Scalar, Index>& mesh,
    const DifferentialOperators<Scalar, Index>& ops,
    HodgeDecompositionOptions options)
{
    using VectorX = Eigen::Matrix<Scalar, Eigen::Dynamic, 1>;
    using Vector2 = Eigen::Matrix<Scalar, 2, 1>;
    using Vector3 = Eigen::Matrix<Scalar, 3, 1>;
    using MatrixX3 = Eigen::Matrix<Scalar, Eigen::Dynamic, 3>;
    using SpMat = Eigen::SparseMatrix<Scalar>;

    const int nrosy = static_cast<int>(options.nrosy);

    la_runtime_assert(nrosy >= 1, "hodge_decomposition_vector_field: nrosy must be >= 1.");

    // Read input per-vertex vector field (3D vector per vertex in global coordinates).
    const auto input_id = internal::find_attribute<Scalar>(
        mesh,
        options.input_attribute,
        AttributeElement::Vertex,
        AttributeUsage::Vector,
        3);
    la_runtime_assert(
        input_id != invalid_attribute_id(),
        "hodge_decomposition_vector_field: input attribute not found or does not match expected "
        "properties (must be a Vector Vertex attribute with 3 channels).");

    const auto input_view = attribute_matrix_view<Scalar>(mesh, input_id);

    // Discrete operators.
    const SpMat D0 = ops.d0(); // #E × #V

    const Eigen::Index nv = static_cast<Eigen::Index>(mesh.get_num_vertices());
    const Eigen::Index ne = static_cast<Eigen::Index>(mesh.get_num_edges());
    const Eigen::Index nf = static_cast<Eigen::Index>(mesh.get_num_facets());

    // Precompute face areas from vector area attribute.
    const auto va_id = ops.get_vector_area_attribute_id();
    const auto va_view = attribute_matrix_view<Scalar>(mesh, va_id);

    // Precompute the n-rosy Levi-Civita transport matrix (reused for both forward and
    // inverse transport in the n-rosy path).
    SpMat LC_n;
    if (nrosy > 1) {
        LC_n = ops.levi_civita_nrosy(static_cast<Index>(nrosy));
    }

    // ---- Convert per-vertex vector field to 1-form ----
    VectorX omega(ne);

    if (nrosy == 1) {
        // Simple midpoint rule: ω_e = (V_i + V_j)/2 · (x_j - x_i).
        // No tangent-plane projection needed: the normal component vanishes in the
        // dot product with the edge vector (which lies on the surface).
        MatrixX3 positions(nv, 3);
        for (Index v = 0; v < static_cast<Index>(nv); ++v) {
            auto p = mesh.get_position(v);
            for (int d = 0; d < 3; ++d) positions(v, d) = p[d];
        }
        MatrixX3 edge_vecs = D0 * positions; // #E × 3

        for (Index e = 0; e < static_cast<Index>(ne); ++e) {
            auto ev = mesh.get_edge_vertices(e);
            Eigen::Matrix<Scalar, 1, 3> avg_vec =
                (input_view.row(ev[0]) + input_view.row(ev[1])) / Scalar(2);
            omega(e) = avg_vec.dot(edge_vecs.row(e));
        }
    } else {
        // N-rosy path: encode in vertex tangent planes, parallel transport to face
        // frames via the n-rosy Levi-Civita connection, then apply the flat operator.

        // 1. Encode per-vertex vectors to 2D tangent vectors.
        VectorX u_encoded(2 * nv);
        for (Index v = 0; v < static_cast<Index>(nv); ++v) {
            auto B = ops.vertex_basis(v); // 3 × 2
            Vector2 u2 = B.transpose() * Vector3(input_view.row(v).transpose());
            u2 = nrosy_encode(u2, nrosy);
            u_encoded.segment(2 * v, 2) = u2;
        }

        // 2. Transport vertex tangent vectors to corners using n-rosy connection.
        //    LC_n is (#C * 2) × (#V * 2).
        VectorX u_corners = LC_n * u_encoded;

        // 3. Average corner vectors per face → per-face 2D → per-face 3D.
        VectorX face_vecs_3d(3 * nf);
        Index corner_offset = 0;
        for (Index f = 0; f < static_cast<Index>(nf); ++f) {
            Index fs = mesh.get_facet_size(f);
            Vector2 avg_u = Vector2::Zero();
            for (Index lv = 0; lv < fs; ++lv) {
                avg_u += u_corners.segment(2 * (corner_offset + lv), 2);
            }
            avg_u /= static_cast<Scalar>(fs);

            auto Bf = ops.facet_basis(f); // 3 × 2
            Vector3 v3 = Bf * avg_u;
            face_vecs_3d.segment(3 * f, 3) = v3;

            corner_offset += fs;
        }

        // 4. Apply global flat operator: per-face 3D vectors → 1-form.
        SpMat Flat = ops.flat(); // #E × (#F * 3)
        omega = Flat * face_vecs_3d;
    }

    // ---- Write 1-form to edge attribute and decompose ----
    {
        const auto omega_id = internal::find_or_create_attribute<Scalar>(
            mesh,
            "@_hodge_vf_omega",
            AttributeElement::Edge,
            AttributeUsage::Scalar,
            1,
            internal::ResetToDefault::No);
        attribute_matrix_ref<Scalar>(mesh, omega_id).col(0) = omega;
    }

    HodgeDecompositionOptions opts_1form;
    opts_1form.lambda = options.lambda;
    opts_1form.input_attribute = "@_hodge_vf_omega";
    opts_1form.exact_attribute = "@_hodge_vf_1form_exact";
    opts_1form.coexact_attribute = "@_hodge_vf_1form_coexact";
    opts_1form.harmonic_attribute = "@_hodge_vf_1form_harmonic";

    auto result_1form = hodge_decomposition_1_form(mesh, ops, opts_1form);

    // ---- Read 1-form results ----
    const VectorX omega_exact = attribute_matrix_view<Scalar>(mesh, result_1form.exact_id).col(0);
    const VectorX omega_coexact =
        attribute_matrix_view<Scalar>(mesh, result_1form.coexact_id).col(0);
    const VectorX omega_harmonic =
        attribute_matrix_view<Scalar>(mesh, result_1form.harmonic_id).col(0);

    // ---- Convert 1-form components back to per-vertex vector fields ----
    const SpMat Sharp = ops.sharp(); // #F*3 × #E

    MatrixX3 vertex_exact, vertex_coexact, vertex_harmonic;

    // Precompute face areas (reused across all components).
    VectorX face_areas(nf);
    for (Index f = 0; f < static_cast<Index>(nf); ++f) {
        face_areas(f) = va_view.row(f).norm();
    }

    if (nrosy == 1) {
        // Simple sharp + area-weighted averaging to vertices.
        auto oneform_to_vertex = [&](const VectorX& omega_1form) -> MatrixX3 {
            VectorX fv = Sharp * omega_1form;

            MatrixX3 result = MatrixX3::Zero(nv, 3);
            VectorX weights = VectorX::Zero(nv);

            for (Index f = 0; f < static_cast<Index>(nf); ++f) {
                Eigen::Matrix<Scalar, 1, 3> face_vec(fv(3 * f), fv(3 * f + 1), fv(3 * f + 2));

                Index facet_size = mesh.get_facet_size(f);
                for (Index lv = 0; lv < facet_size; ++lv) {
                    Index v = mesh.get_facet_vertex(f, lv);
                    result.row(v) += face_areas(f) * face_vec;
                    weights(v) += face_areas(f);
                }
            }

            for (Index v = 0; v < static_cast<Index>(nv); ++v) {
                if (weights(v) > Scalar(0)) result.row(v) /= weights(v);
            }

            return result;
        };

        vertex_exact = oneform_to_vertex(omega_exact);
        vertex_coexact = oneform_to_vertex(omega_coexact);
        // Compute harmonic as residual so that V_exact + V_coexact + V_harmonic = V_input
        // exactly. Converting ω_harmonic independently would only recover the roundtripped
        // input (sharp ∘ flat ≠ identity on vertex fields).
        vertex_harmonic = input_view - vertex_exact - vertex_coexact;
    } else {
        // N-rosy path: sharp → per-face 2D → inverse transport → decode.
        //
        // For each 1-form component:
        //   1. sharp → per-face 3D vectors
        //   2. Project to face 2D tangent, replicate to corners (area-weighted)
        //   3. Inverse transport via LC_n^T (adjoint of n-rosy Levi-Civita)
        //   4. Normalize by area weights at vertices
        //   5. Decode n-rosy and convert to 3D
        SpMat LC_nT = LC_n.transpose(); // (#V * 2) × (#C * 2)

        // Precompute per-vertex area sums (reused across all 3 components).
        Index total_corners = 0;
        VectorX vertex_area = VectorX::Zero(nv);
        for (Index f = 0; f < static_cast<Index>(nf); ++f) {
            Index fs = mesh.get_facet_size(f);
            total_corners += fs;
            for (Index lv = 0; lv < fs; ++lv) {
                Index v = mesh.get_facet_vertex(f, lv);
                vertex_area(v) += face_areas(f);
            }
        }

        auto oneform_to_vertex_nrosy = [&](const VectorX& omega_1form) -> MatrixX3 {
            // 1. Per-face 3D vectors via sharp.
            VectorX fv = Sharp * omega_1form;

            // 2. Project to face 2D, replicate to corners with area weighting.
            VectorX u_corners(2 * total_corners);
            Index corner_offset = 0;
            for (Index f = 0; f < static_cast<Index>(nf); ++f) {
                Vector3 fv3(fv(3 * f), fv(3 * f + 1), fv(3 * f + 2));
                auto Bf = ops.facet_basis(f); // 3 × 2
                Vector2 u_f = Bf.transpose() * fv3;

                Index fs = mesh.get_facet_size(f);
                for (Index lv = 0; lv < fs; ++lv) {
                    u_corners.segment(2 * (corner_offset + lv), 2) = face_areas(f) * u_f;
                }
                corner_offset += fs;
            }

            // 3. Inverse transport: LC_n^T sums R_{f,v}^{n,T} * (area_f * u_f)
            //    at each vertex.
            VectorX u_vertices = LC_nT * u_corners;

            // 4. Normalize by total incident face area, decode, convert to 3D.
            MatrixX3 result(nv, 3);
            for (Index v = 0; v < static_cast<Index>(nv); ++v) {
                Vector2 u2 = u_vertices.segment(2 * v, 2);
                if (vertex_area(v) > Scalar(0)) u2 /= vertex_area(v);
                u2 = nrosy_decode(u2, nrosy);
                auto B = ops.vertex_basis(v);
                result.row(v) = (B * u2).transpose();
            }

            return result;
        };

        vertex_exact = oneform_to_vertex_nrosy(omega_exact);
        vertex_coexact = oneform_to_vertex_nrosy(omega_coexact);
        vertex_harmonic = oneform_to_vertex_nrosy(omega_harmonic);
    }

    // ---- Write output per-vertex vector attributes ----
    auto write_attr = [&](std::string_view name, const MatrixX3& values) -> AttributeId {
        const auto id = internal::find_or_create_attribute<Scalar>(
            mesh,
            name,
            AttributeElement::Vertex,
            AttributeUsage::Vector,
            3,
            internal::ResetToDefault::No);
        attribute_matrix_ref<Scalar>(mesh, id) = values;
        return id;
    };

    HodgeDecompositionResult result;
    result.exact_id = write_attr(options.exact_attribute, vertex_exact);
    result.coexact_id = write_attr(options.coexact_attribute, vertex_coexact);
    result.harmonic_id = write_attr(options.harmonic_attribute, vertex_harmonic);
    return result;
}

template <typename Scalar, typename Index>
HodgeDecompositionResult hodge_decomposition_vector_field(
    SurfaceMesh<Scalar, Index>& mesh,
    HodgeDecompositionOptions options)
{
    DifferentialOperators<Scalar, Index> ops(mesh);
    return hodge_decomposition_vector_field(mesh, ops, std::move(options));
}

// =============================================================================
// Explicit template instantiations
// =============================================================================

#define LA_X_hodge_decomposition_1_form(_, Scalar, Index)                                       \
    template LA_POLYDDG_API HodgeDecompositionResult hodge_decomposition_1_form<Scalar, Index>( \
        SurfaceMesh<Scalar, Index>&,                                                            \
        const DifferentialOperators<Scalar, Index>&,                                            \
        HodgeDecompositionOptions);                                                             \
    template LA_POLYDDG_API HodgeDecompositionResult hodge_decomposition_1_form<Scalar, Index>( \
        SurfaceMesh<Scalar, Index>&,                                                            \
        HodgeDecompositionOptions);
LA_SURFACE_MESH_X(hodge_decomposition_1_form, 0)

#define LA_X_hodge_decomposition_vector_field(_, Scalar, Index) \
    template LA_POLYDDG_API HodgeDecompositionResult            \
    hodge_decomposition_vector_field<Scalar, Index>(            \
        SurfaceMesh<Scalar, Index>&,                            \
        const DifferentialOperators<Scalar, Index>&,            \
        HodgeDecompositionOptions);                             \
    template LA_POLYDDG_API HodgeDecompositionResult            \
    hodge_decomposition_vector_field<Scalar, Index>(            \
        SurfaceMesh<Scalar, Index>&,                            \
        HodgeDecompositionOptions);
LA_SURFACE_MESH_X(hodge_decomposition_vector_field, 0)

} // namespace lagrange::polyddg
