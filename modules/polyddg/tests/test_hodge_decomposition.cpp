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

#include <lagrange/internal/constants.h>
#include <lagrange/internal/find_attribute_utils.h>
#include <lagrange/polyddg/DifferentialOperators.h>
#include <lagrange/polyddg/hodge_decomposition.h>
#include <lagrange/primitive/generate_torus.h>
#include <lagrange/testing/common.h>
#include <lagrange/testing/create_test_mesh.h>
#include <lagrange/views.h>

#include <Eigen/Core>
#include <Eigen/Sparse>

#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <limits>

// ---- helpers ----------------------------------------------------------------

namespace {

using Scalar = double;
using Index = uint32_t;
using VectorX = Eigen::Matrix<Scalar, Eigen::Dynamic, 1>;
using MatrixX3 = Eigen::Matrix<Scalar, Eigen::Dynamic, 3>;
using SpMat = Eigen::SparseMatrix<Scalar>;

/// Run hodge_decomposition_vector_field and return (V_exact, V_coexact, V_harmonic).
std::tuple<MatrixX3, MatrixX3, MatrixX3> run_decomp(
    lagrange::SurfaceMesh<Scalar, Index>& mesh,
    const lagrange::polyddg::DifferentialOperators<Scalar, Index>& ops,
    const MatrixX3& V,
    std::string_view name = "@hd_test_input",
    uint8_t nrosy = 1)
{
    using namespace lagrange;

    const auto input_id = internal::find_or_create_attribute<Scalar>(
        mesh,
        name,
        AttributeElement::Vertex,
        AttributeUsage::Vector,
        3,
        internal::ResetToDefault::No);
    attribute_matrix_ref<Scalar>(mesh, input_id) = V;

    polyddg::HodgeDecompositionOptions opts;
    opts.input_attribute = name;
    opts.nrosy = nrosy;
    auto r = polyddg::hodge_decomposition_vector_field(mesh, ops, opts);

    auto read = [&](AttributeId id) -> MatrixX3 { return attribute_matrix_view<Scalar>(mesh, id); };
    return {read(r.exact_id), read(r.coexact_id), read(r.harmonic_id)};
}

/// Run hodge_decomposition_1_form and return (omega_exact, omega_coexact, omega_harmonic).
std::tuple<VectorX, VectorX, VectorX> run_decomp_1form(
    lagrange::SurfaceMesh<Scalar, Index>& mesh,
    const lagrange::polyddg::DifferentialOperators<Scalar, Index>& ops,
    const VectorX& omega,
    std::string_view name = "@hd_test_1form_input")
{
    using namespace lagrange;

    const auto input_id = internal::find_or_create_attribute<Scalar>(
        mesh,
        name,
        AttributeElement::Edge,
        AttributeUsage::Scalar,
        1,
        internal::ResetToDefault::No);
    attribute_matrix_ref<Scalar>(mesh, input_id).col(0) = omega;

    polyddg::HodgeDecompositionOptions opts;
    opts.input_attribute = name;
    auto r = polyddg::hodge_decomposition_1_form(mesh, ops, opts);

    auto read = [&](AttributeId id) -> VectorX {
        return attribute_matrix_view<Scalar>(mesh, id).col(0);
    };
    return {read(r.exact_id), read(r.coexact_id), read(r.harmonic_id)};
}

/// Build a per-vertex gradient vector field from a scalar function f.
MatrixX3 make_gradient_field(
    lagrange::SurfaceMesh<Scalar, Index>& mesh,
    const lagrange::polyddg::DifferentialOperators<Scalar, Index>& ops,
    const VectorX& f)
{
    using namespace lagrange;

    const Eigen::Index nv = static_cast<Eigen::Index>(mesh.get_num_vertices());
    const Eigen::Index nf = static_cast<Eigen::Index>(mesh.get_num_facets());

    SpMat D0 = ops.d0();
    VectorX omega = D0 * f;

    SpMat Sharp = ops.sharp();
    VectorX fv = Sharp * omega;

    auto va_id = ops.get_vector_area_attribute_id();
    auto va_view = attribute_matrix_view<Scalar>(mesh, va_id);

    MatrixX3 result = MatrixX3::Zero(nv, 3);
    VectorX weights = VectorX::Zero(nv);

    for (Index fi = 0; fi < static_cast<Index>(nf); ++fi) {
        Scalar area = va_view.row(fi).norm();
        Eigen::Matrix<Scalar, 1, 3> face_vec(fv(3 * fi), fv(3 * fi + 1), fv(3 * fi + 2));

        Index facet_size = mesh.get_facet_size(fi);
        for (Index lv = 0; lv < facet_size; ++lv) {
            Index v = mesh.get_facet_vertex(fi, lv);
            result.row(v) += area * face_vec;
            weights(v) += area;
        }
    }

    for (Index v = 0; v < static_cast<Index>(nv); ++v) {
        if (weights(v) > 0) result.row(v) /= weights(v);
    }

    return result;
}

/// Replicate the internal vertex→1-form conversion for n-rosy fields.
VectorX vertex_to_1form_nrosy(
    lagrange::SurfaceMesh<Scalar, Index>& mesh,
    const lagrange::polyddg::DifferentialOperators<Scalar, Index>& ops,
    const MatrixX3& V,
    uint8_t nrosy)
{
    using namespace lagrange;
    const Eigen::Index nv = static_cast<Eigen::Index>(mesh.get_num_vertices());
    const Eigen::Index nf = static_cast<Eigen::Index>(mesh.get_num_facets());
    const Eigen::Index ne = static_cast<Eigen::Index>(mesh.get_num_edges());

    if (nrosy == 1) {
        SpMat D0 = ops.d0();
        MatrixX3 positions(nv, 3);
        for (Index v = 0; v < static_cast<Index>(nv); ++v) {
            auto p = mesh.get_position(v);
            for (int d = 0; d < 3; ++d) positions(v, d) = p[d];
        }
        MatrixX3 edge_vecs = D0 * positions;

        VectorX omega(ne);
        for (Index e = 0; e < static_cast<Index>(ne); ++e) {
            auto ev = mesh.get_edge_vertices(e);
            Eigen::Matrix<Scalar, 1, 3> avg_vec = (V.row(ev[0]) + V.row(ev[1])) / 2.0;
            omega(e) = avg_vec.dot(edge_vecs.row(e));
        }
        return omega;
    }

    VectorX u_encoded(2 * nv);
    for (Index v = 0; v < static_cast<Index>(nv); ++v) {
        auto B = ops.vertex_basis(v);
        Eigen::Matrix<Scalar, 2, 1> u2 = B.transpose() * V.row(v).transpose();
        Scalar r = u2.norm();
        if (r > std::numeric_limits<Scalar>::epsilon()) {
            Scalar c = u2(0) / r, s = u2(1) / r;
            Scalar re = 1.0, im = 0.0;
            for (int k = 0; k < nrosy; ++k) {
                Scalar nr = re * c - im * s;
                Scalar ni = re * s + im * c;
                re = nr;
                im = ni;
            }
            u2 = Eigen::Matrix<Scalar, 2, 1>(r * re, r * im);
        }
        u_encoded.segment(2 * v, 2) = u2;
    }

    SpMat LC_n = ops.levi_civita_nrosy(static_cast<Index>(nrosy));
    VectorX u_corners = LC_n * u_encoded;

    VectorX face_vecs_3d(3 * nf);
    Index corner_offset = 0;
    for (Index f = 0; f < static_cast<Index>(nf); ++f) {
        Index fs = mesh.get_facet_size(f);
        Eigen::Matrix<Scalar, 2, 1> avg_u = Eigen::Matrix<Scalar, 2, 1>::Zero();
        for (Index lv = 0; lv < fs; ++lv) {
            avg_u += u_corners.segment(2 * (corner_offset + lv), 2);
        }
        avg_u /= static_cast<Scalar>(fs);

        auto Bf = ops.facet_basis(f);
        Eigen::Matrix<Scalar, 3, 1> v3 = Bf * avg_u;
        face_vecs_3d.segment(3 * f, 3) = v3;

        corner_offset += fs;
    }

    SpMat Flat = ops.flat();
    return Flat * face_vecs_3d;
}

} // namespace

// ---- tests ------------------------------------------------------------------

TEST_CASE("HodgeDecomposition1Form", "[polyddg]")
{
    using namespace lagrange;

    // Octahedron: 6 vertices, 12 edges, 8 triangles — genus-0 closed surface.
    SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_vertex({-1, 0, 0});
    mesh.add_vertex({0, -1, 0});
    mesh.add_vertex({0, 0, 1});
    mesh.add_vertex({0, 0, -1});
    mesh.add_triangle(0, 1, 4);
    mesh.add_triangle(1, 2, 4);
    mesh.add_triangle(2, 3, 4);
    mesh.add_triangle(3, 0, 4);
    mesh.add_triangle(0, 5, 3);
    mesh.add_triangle(1, 5, 0);
    mesh.add_triangle(2, 5, 1);
    mesh.add_triangle(3, 5, 2);

    polyddg::DifferentialOperators<Scalar, Index> ops(mesh);
    const Eigen::Index ne = static_cast<Eigen::Index>(mesh.get_num_edges());

    SECTION("1-form reconstruction")
    {
        VectorX omega = VectorX::Random(ne);
        const auto [w_exact, w_coexact, w_harmonic] = run_decomp_1form(mesh, ops, omega);

        const VectorX residual = w_exact + w_coexact + w_harmonic - omega;
        REQUIRE_THAT(residual.norm(), Catch::Matchers::WithinAbs(0.0, 1e-12));
    }

    SECTION("exact 1-form has vanishing curl")
    {
        VectorX omega = VectorX::Random(ne);
        const auto [w_exact, w_coexact, w_harmonic] = run_decomp_1form(mesh, ops, omega);

        SpMat D1 = ops.d1();
        REQUIRE_THAT((D1 * w_exact).norm(), Catch::Matchers::WithinAbs(0.0, 1e-10));
    }

    SECTION("gradient 1-form has vanishing co-exact part")
    {
        // Build a gradient 1-form: omega = D0 * f.
        const Eigen::Index nv = static_cast<Eigen::Index>(mesh.get_num_vertices());
        VectorX f(nv);
        for (Index v = 0; v < static_cast<Index>(nv); v++) f(v) = mesh.get_position(v)[0];
        SpMat D0 = ops.d0();
        VectorX omega = D0 * f;

        const auto [w_exact, w_coexact, w_harmonic] = run_decomp_1form(mesh, ops, omega);

        REQUIRE_THAT(w_coexact.norm(), Catch::Matchers::WithinAbs(0.0, 1e-8));
    }

    SECTION("harmonic vanishes on genus-0")
    {
        VectorX omega = VectorX::Random(ne);
        const auto [w_exact, w_coexact, w_harmonic] = run_decomp_1form(mesh, ops, omega);

        REQUIRE_THAT(w_harmonic.norm(), Catch::Matchers::WithinAbs(0.0, 1e-10));
    }
}

TEST_CASE("HodgeDecompositionVectorField", "[polyddg]")
{
    using namespace lagrange;

    // Octahedron: 6 vertices, 12 edges, 8 triangles — genus-0 closed surface.
    SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_vertex({-1, 0, 0});
    mesh.add_vertex({0, -1, 0});
    mesh.add_vertex({0, 0, 1});
    mesh.add_vertex({0, 0, -1});
    mesh.add_triangle(0, 1, 4);
    mesh.add_triangle(1, 2, 4);
    mesh.add_triangle(2, 3, 4);
    mesh.add_triangle(3, 0, 4);
    mesh.add_triangle(0, 5, 3);
    mesh.add_triangle(1, 5, 0);
    mesh.add_triangle(2, 5, 1);
    mesh.add_triangle(3, 5, 2);

    polyddg::DifferentialOperators<Scalar, Index> ops(mesh);
    const Eigen::Index num_vertices = mesh.get_num_vertices();

    SECTION("reconstruction")
    {
        MatrixX3 V = MatrixX3::Random(num_vertices, 3);
        const auto [V_exact, V_coexact, V_harmonic] = run_decomp(mesh, ops, V);

        const MatrixX3 residual = V_exact + V_coexact + V_harmonic - V;
        REQUIRE_THAT(residual.norm(), Catch::Matchers::WithinAbs(0.0, 1e-12));
    }

    SECTION("gradient input has vanishing co-exact part")
    {
        VectorX f(num_vertices);
        for (Index v = 0; v < static_cast<Index>(num_vertices); v++) f(v) = mesh.get_position(v)[0];
        MatrixX3 V = make_gradient_field(mesh, ops, f);

        const auto [V_exact, V_coexact, V_harmonic] = run_decomp(mesh, ops, V);

        REQUIRE_THAT(V_coexact.norm(), Catch::Matchers::WithinAbs(0.0, 1e-8));
        REQUIRE_THAT(
            (V_exact + V_coexact + V_harmonic - V).norm(),
            Catch::Matchers::WithinAbs(0.0, 1e-12));
    }

    SECTION("sphere reconstruction")
    {
        auto sphere = lagrange::testing::create_test_sphere<Scalar, Index>(
            lagrange::testing::CreateOptions{false, false});
        polyddg::DifferentialOperators<Scalar, Index> sphere_ops(sphere);

        const MatrixX3 V = MatrixX3::Random(sphere.get_num_vertices(), 3);
        const auto [V_exact, V_coexact, V_harmonic] = run_decomp(sphere, sphere_ops, V);

        const MatrixX3 residual = V_exact + V_coexact + V_harmonic - V;
        REQUIRE_THAT(residual.norm(), Catch::Matchers::WithinAbs(0.0, 1e-12));
    }

    SECTION("nrosy=4 harmonic vanishes on genus-0")
    {
        MatrixX3 V = MatrixX3::Random(num_vertices, 3);
        const auto [V_exact, V_coexact, V_harmonic] = run_decomp(mesh, ops, V, "@hd_4", 4);

        REQUIRE_THAT(V_harmonic.norm(), Catch::Matchers::WithinAbs(0.0, 1e-6));
    }

    SECTION("nrosy=4 output vectors lie in tangent plane")
    {
        MatrixX3 V = MatrixX3::Random(num_vertices, 3);
        const auto [V_exact, V_coexact, V_harmonic] = run_decomp(mesh, ops, V, "@hd_4tp", 4);

        auto vn_id = ops.get_vertex_normal_attribute_id();
        auto vn_view = attribute_matrix_view<Scalar>(mesh, vn_id);

        for (Index v = 0; v < static_cast<Index>(num_vertices); ++v) {
            Eigen::Matrix<Scalar, 1, 3> n = vn_view.row(v).normalized();
            if (V_exact.row(v).norm() > 1e-10)
                REQUIRE_THAT(
                    std::abs(V_exact.row(v).dot(n)),
                    Catch::Matchers::WithinAbs(0.0, 1e-6));
            if (V_coexact.row(v).norm() > 1e-10)
                REQUIRE_THAT(
                    std::abs(V_coexact.row(v).dot(n)),
                    Catch::Matchers::WithinAbs(0.0, 1e-6));
        }
    }

    SECTION("torus 4-rosy: nontrivial harmonic on genus-1")
    {
        primitive::TorusOptions torus_opts;
        torus_opts.major_radius = 3.0;
        torus_opts.minor_radius = 1.0;
        torus_opts.ring_segments = 60;
        torus_opts.pipe_segments = 40;
        auto torus = primitive::generate_torus<Scalar, Index>(torus_opts);
        polyddg::DifferentialOperators<Scalar, Index> torus_ops(torus);

        const Eigen::Index nv = static_cast<Eigen::Index>(torus.get_num_vertices());
        const SpMat D1 = torus_ops.d1();

        // Use ∂/∂u tangent direction as input.
        MatrixX3 V_input(nv, 3);
        for (Index vid = 0; vid < static_cast<Index>(nv); ++vid) {
            auto p = torus.get_position(vid);
            Scalar angle_u = std::atan2(p[1], p[0]);
            Eigen::Matrix<Scalar, 3, 1> e_u(-std::sin(angle_u), std::cos(angle_u), 0);
            auto B = torus_ops.vertex_basis(vid);
            Eigen::Matrix<Scalar, 2, 1> u2 = B.transpose() * e_u;
            if (u2.norm() < 1e-10) u2 = Eigen::Matrix<Scalar, 2, 1>(1, 0);
            V_input.row(vid) = (B * u2.normalized()).transpose();
        }

        const auto [V_exact, V_coexact, V_harmonic] =
            run_decomp(torus, torus_ops, V_input, "@hd_torus_du", 4);

        // All three components should be nonzero on genus-1.
        REQUIRE(V_exact.norm() > 1e-6);
        REQUIRE(V_coexact.norm() > 1e-6);
        REQUIRE(V_harmonic.norm() > 1e-6);

        // 1-form properties (approximate due to encode/decode roundtrip).
        VectorX omega = vertex_to_1form_nrosy(torus, torus_ops, V_input, 4);
        VectorX omega_exact = vertex_to_1form_nrosy(torus, torus_ops, V_exact, 4);
        VectorX omega_coexact = vertex_to_1form_nrosy(torus, torus_ops, V_coexact, 4);
        VectorX omega_harmonic = vertex_to_1form_nrosy(torus, torus_ops, V_harmonic, 4);

        REQUIRE((omega_exact + omega_coexact + omega_harmonic - omega).norm() / omega.norm() < 0.5);
        REQUIRE((D1 * omega_exact).norm() / omega_exact.norm() < 0.5);
        REQUIRE((torus_ops.delta1() * omega_coexact).norm() / omega_coexact.norm() < 0.5);
    }

    SECTION("torus 4-rosy: two inputs produce independent harmonic parts")
    {
        primitive::TorusOptions torus_opts;
        torus_opts.major_radius = 3.0;
        torus_opts.minor_radius = 1.0;
        torus_opts.ring_segments = 60;
        torus_opts.pipe_segments = 40;
        auto torus = primitive::generate_torus<Scalar, Index>(torus_opts);
        polyddg::DifferentialOperators<Scalar, Index> torus_ops(torus);

        const Eigen::Index nv = static_cast<Eigen::Index>(torus.get_num_vertices());

        // Input 1: ∂/∂u tangent.
        MatrixX3 V1(nv, 3);
        for (Index vid = 0; vid < static_cast<Index>(nv); ++vid) {
            auto p = torus.get_position(vid);
            Scalar angle_u = std::atan2(p[1], p[0]);
            Eigen::Matrix<Scalar, 3, 1> e_u(-std::sin(angle_u), std::cos(angle_u), 0);
            auto B = torus_ops.vertex_basis(vid);
            Eigen::Matrix<Scalar, 2, 1> u2 = B.transpose() * e_u;
            if (u2.norm() < 1e-10) u2 = Eigen::Matrix<Scalar, 2, 1>(1, 0);
            V1.row(vid) = (B * u2.normalized()).transpose();
        }

        // Input 2: ∂/∂u rotated by π/3.
        MatrixX3 V2(nv, 3);
        for (Index vid = 0; vid < static_cast<Index>(nv); ++vid) {
            auto p = torus.get_position(vid);
            Scalar angle_u = std::atan2(p[1], p[0]);
            Eigen::Matrix<Scalar, 3, 1> e_u(-std::sin(angle_u), std::cos(angle_u), 0);
            auto B = torus_ops.vertex_basis(vid);
            Eigen::Matrix<Scalar, 2, 1> u2 = B.transpose() * e_u;
            Eigen::Matrix<Scalar, 2, 1> u2_rot(
                u2(0) * std::cos(internal::pi / 3) - u2(1) * std::sin(internal::pi / 3),
                u2(0) * std::sin(internal::pi / 3) + u2(1) * std::cos(internal::pi / 3));
            if (u2_rot.norm() < 1e-10) u2_rot = Eigen::Matrix<Scalar, 2, 1>(1, 0);
            V2.row(vid) = (B * u2_rot.normalized()).transpose();
        }

        const auto [exact1, coexact1, harmonic1] =
            run_decomp(torus, torus_ops, V1, "@hd_torus_v1", 4);
        const auto [exact2, coexact2, harmonic2] =
            run_decomp(torus, torus_ops, V2, "@hd_torus_v2", 4);

        REQUIRE(harmonic1.norm() > 1e-6);
        REQUIRE(harmonic2.norm() > 1e-6);

        // M₁-linear independence via Gram determinant.
        VectorX h1 = vertex_to_1form_nrosy(torus, torus_ops, harmonic1, 4);
        VectorX h2 = vertex_to_1form_nrosy(torus, torus_ops, harmonic2, 4);
        SpMat M1 = torus_ops.star1();
        Scalar g11 = h1.dot(M1 * h1);
        Scalar g12 = h1.dot(M1 * h2);
        Scalar g22 = h2.dot(M1 * h2);
        Scalar normalized_det = (g11 * g22 - g12 * g12) / (g11 * g22);
        INFO("Gram det / (||h1||² ||h2||²) = " << normalized_det);
        REQUIRE(normalized_det > 0.01);
    }
}
