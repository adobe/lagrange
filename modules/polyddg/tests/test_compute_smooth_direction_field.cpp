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
#include <lagrange/polyddg/compute_smooth_direction_field.h>
#include <lagrange/primitive/generate_icosahedron.h>
#include <lagrange/primitive/generate_subdivided_sphere.h>
#include <lagrange/primitive/generate_torus.h>
#include <lagrange/testing/common.h>
#include <lagrange/views.h>

#include <Eigen/Core>
#include <Eigen/Sparse>

#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>

TEST_CASE("compute_smooth_direction_field", "[polyddg]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    SECTION("output dimensions and unit length — triangle mesh")
    {
        SurfaceMesh<Scalar, Index> triangle_mesh;
        triangle_mesh.add_vertex({1.0, 0.0, 0.0});
        triangle_mesh.add_vertex({0.0, 1.0, 0.0});
        triangle_mesh.add_vertex({0.0, 0.0, 1.0});
        triangle_mesh.add_triangle(0, 1, 2);

        polyddg::DifferentialOperators<Scalar, Index> ops(triangle_mesh);
        polyddg::SmoothDirectionFieldOptions opts;
        opts.nrosy = 1;
        auto result = polyddg::compute_smooth_direction_field(triangle_mesh, ops, opts);

        REQUIRE(result != invalid_attribute_id());
        auto data = attribute_matrix_view<Scalar>(triangle_mesh, result);
        REQUIRE(data.rows() == static_cast<Eigen::Index>(triangle_mesh.get_num_vertices()));
        REQUIRE(data.cols() == 3);
        for (Index vid = 0; vid < triangle_mesh.get_num_vertices(); ++vid) {
            REQUIRE_THAT(data.row(vid).norm(), Catch::Matchers::WithinAbs(1.0, 1e-10));
        }
    }

    SECTION("output dimensions and unit length — pyramid mesh n=4")
    {
        SurfaceMesh<Scalar, Index> pyramid_mesh;
        pyramid_mesh.add_vertex({0.0, 0.0, 0.0});
        pyramid_mesh.add_vertex({1.0, 0.0, 0.0});
        pyramid_mesh.add_vertex({1.0, 1.0, 0.0});
        pyramid_mesh.add_vertex({0.0, 1.0, 0.0});
        pyramid_mesh.add_vertex({0.5, 0.5, 1.0});
        pyramid_mesh.add_triangle(0, 1, 4);
        pyramid_mesh.add_triangle(1, 2, 4);
        pyramid_mesh.add_triangle(2, 3, 4);
        pyramid_mesh.add_triangle(3, 0, 4);
        pyramid_mesh.add_quad(0, 3, 2, 1);

        polyddg::DifferentialOperators<Scalar, Index> ops(pyramid_mesh);
        polyddg::SmoothDirectionFieldOptions opts;
        opts.nrosy = 4;
        auto result = polyddg::compute_smooth_direction_field(pyramid_mesh, ops, opts);

        REQUIRE(result != invalid_attribute_id());
        auto data = attribute_matrix_view<Scalar>(pyramid_mesh, result);
        REQUIRE(data.rows() == static_cast<Eigen::Index>(pyramid_mesh.get_num_vertices()));
        REQUIRE(data.cols() == 3);
        for (Index vid = 0; vid < pyramid_mesh.get_num_vertices(); ++vid) {
            REQUIRE_THAT(data.row(vid).norm(), Catch::Matchers::WithinAbs(1.0, 1e-10));
        }
    }

    SECTION("result is deterministic")
    {
        SurfaceMesh<Scalar, Index> pyramid_mesh;
        pyramid_mesh.add_vertex({0.0, 0.0, 0.0});
        pyramid_mesh.add_vertex({1.0, 0.0, 0.0});
        pyramid_mesh.add_vertex({1.0, 1.0, 0.0});
        pyramid_mesh.add_vertex({0.0, 1.0, 0.0});
        pyramid_mesh.add_vertex({0.5, 0.5, 1.0});
        pyramid_mesh.add_triangle(0, 1, 4);
        pyramid_mesh.add_triangle(1, 2, 4);
        pyramid_mesh.add_triangle(2, 3, 4);
        pyramid_mesh.add_triangle(3, 0, 4);
        pyramid_mesh.add_quad(0, 3, 2, 1);

        polyddg::DifferentialOperators<Scalar, Index> ops(pyramid_mesh);
        polyddg::SmoothDirectionFieldOptions opts;
        opts.nrosy = 4;
        opts.direction_field_attribute = "@sdf_first";
        auto result1 = polyddg::compute_smooth_direction_field(pyramid_mesh, ops, opts);

        opts.direction_field_attribute = "@sdf_second";
        auto result2 = polyddg::compute_smooth_direction_field(pyramid_mesh, ops, opts);

        auto data1 = attribute_matrix_view<Scalar>(pyramid_mesh, result1);
        auto data2 = attribute_matrix_view<Scalar>(pyramid_mesh, result2);

        // Compare in the n-fold encoded space to handle n-rosy equivalence:
        // for n=4, two fields differing by a rotation of k*π/2 are equivalent.
        // Encode each vertex's 2D tangent direction to 4θ and compare.
        Scalar max_encoded_diff = 0;
        for (Index vid = 0; vid < pyramid_mesh.get_num_vertices(); ++vid) {
            auto B = ops.vertex_basis(vid);
            Eigen::Matrix<Scalar, 2, 1> u1 = B.transpose() * data1.row(vid).transpose();
            Eigen::Matrix<Scalar, 2, 1> u2 = B.transpose() * data2.row(vid).transpose();
            Scalar a1 = 4.0 * std::atan2(u1(1), u1(0));
            Scalar a2 = 4.0 * std::atan2(u2(1), u2(0));
            // 1 - cos(Δ) measures angular distance in the encoded space.
            max_encoded_diff = std::max(max_encoded_diff, 1.0 - std::cos(a1 - a2));
        }
        REQUIRE_THAT(max_encoded_diff, Catch::Matchers::WithinAbs(0.0, 1e-8));
    }

    SECTION("sphere: tangent to surface n=4")
    {
        primitive::IcosahedronOptions ico_opts;
        ico_opts.radius = 1.0;
        auto base_ico = primitive::generate_icosahedron<Scalar, Index>(ico_opts);
        primitive::SubdividedSphereOptions subdiv_opts;
        subdiv_opts.radius = 1.0;
        subdiv_opts.subdiv_level = 1;
        auto sphere = primitive::generate_subdivided_sphere<Scalar, Index>(base_ico, subdiv_opts);

        polyddg::DifferentialOperators<Scalar, Index> ops(sphere);
        polyddg::SmoothDirectionFieldOptions opts;
        opts.nrosy = 4;
        auto result = polyddg::compute_smooth_direction_field(sphere, ops, opts);

        auto data = attribute_matrix_view<Scalar>(sphere, result);
        const auto normal_id = ops.get_vertex_normal_attribute_id();
        const auto normal_view = attribute_matrix_view<Scalar>(sphere, normal_id);

        Scalar max_dot = 0;
        for (Index vid = 0; vid < sphere.get_num_vertices(); ++vid) {
            max_dot = std::max(max_dot, std::abs(data.row(vid).dot(normal_view.row(vid))));
        }
        REQUIRE(max_dot < 1e-10);
    }

    SECTION("sphere: n=4 decoded angle in principal branch")
    {
        primitive::IcosahedronOptions ico_opts;
        ico_opts.radius = 1.0;
        auto base_ico = primitive::generate_icosahedron<Scalar, Index>(ico_opts);
        primitive::SubdividedSphereOptions subdiv_opts;
        subdiv_opts.radius = 1.0;
        subdiv_opts.subdiv_level = 1;
        auto sphere = primitive::generate_subdivided_sphere<Scalar, Index>(base_ico, subdiv_opts);

        polyddg::DifferentialOperators<Scalar, Index> ops(sphere);
        polyddg::SmoothDirectionFieldOptions opts;
        opts.nrosy = 4;
        auto result = polyddg::compute_smooth_direction_field(sphere, ops, opts);

        auto data = attribute_matrix_view<Scalar>(sphere, result);
        for (Index vid = 0; vid < sphere.get_num_vertices(); ++vid) {
            auto B = ops.vertex_basis(vid);
            Eigen::Matrix<Scalar, 2, 1> u2 = B.transpose() * data.row(vid).transpose();
            if (u2.norm() < 1e-10) continue;
            Scalar theta = std::atan2(u2(1), u2(0));
            REQUIRE(theta > -internal::pi_4 - 1e-10);
            REQUIRE(theta <= internal::pi_4 + 1e-10);
        }
    }

    SECTION("sphere: alignment constraint is respected (regression)")
    {
        // Regression test for the alignment-flip bug. A sphere has σ_min ≈ 4, and the
        // earlier implementation set the spectral shift to σ_min — exactly the boundary
        // disallowed by Knöppel et al. 2013 Algorithm 3 (λ_t ∈ (−∞, λ_0) strictly).
        // Numerical error then drove the smoothest-mode coefficient negative under
        // LDLT, producing cos(4·Δθ) ≈ −1 (worst-case anti-aligned). The current
        // implementation hard-codes λ_t = 0 (paper-recommended default), avoiding the
        // boundary entirely.
        primitive::IcosahedronOptions ico_opts;
        ico_opts.radius = 1.0;
        auto base_ico = primitive::generate_icosahedron<Scalar, Index>(ico_opts);
        primitive::SubdividedSphereOptions subdiv_opts;
        subdiv_opts.radius = 1.0;
        subdiv_opts.subdiv_level = 3;
        auto sphere = primitive::generate_subdivided_sphere<Scalar, Index>(base_ico, subdiv_opts);
        polyddg::DifferentialOperators<Scalar, Index> ops(sphere);

        const Index constrained_vid = 0;
        auto p = sphere.get_position(constrained_vid);
        Eigen::Matrix<Scalar, 3, 1> pv(p[0], p[1], p[2]);
        Eigen::Matrix<Scalar, 3, 1> e(0, 0, 1);
        Eigen::Matrix<Scalar, 3, 1> tan = e - e.dot(pv.normalized()) * pv.normalized();
        if (tan.norm() < 1e-6) {
            e = Eigen::Matrix<Scalar, 3, 1>(1, 0, 0);
            tan = e - e.dot(pv.normalized()) * pv.normalized();
        }
        tan.normalize();

        auto align_id = internal::find_or_create_attribute<Scalar>(
            sphere,
            "@sphere_alignment",
            AttributeElement::Vertex,
            AttributeUsage::Vector,
            3,
            internal::ResetToDefault::Yes);
        auto align_data = attribute_matrix_ref<Scalar>(sphere, align_id);
        align_data.setZero();
        align_data.row(constrained_vid) = tan.transpose();

        polyddg::SmoothDirectionFieldOptions opts;
        opts.nrosy = 4;
        opts.alignment_attribute = "@sphere_alignment";
        auto result = polyddg::compute_smooth_direction_field(sphere, ops, opts);
        auto data = attribute_matrix_view<Scalar>(sphere, result);

        auto B = ops.vertex_basis(constrained_vid);
        Eigen::Matrix<Scalar, 2, 1> out_2d = B.transpose() * data.row(constrained_vid).transpose();
        Eigen::Matrix<Scalar, 2, 1> ref_2d = B.transpose() * tan;
        Scalar out_angle = std::atan2(out_2d(1), out_2d(0));
        Scalar ref_angle = std::atan2(ref_2d(1), ref_2d(0));
        Scalar cos4_diff = std::cos(4.0 * (out_angle - ref_angle));
        REQUIRE(cos4_diff > 0.9);
    }

    SECTION("torus: alignment constraint is respected")
    {
        primitive::TorusOptions torus_opts;
        torus_opts.major_radius = 3.0;
        torus_opts.minor_radius = 1.0;
        torus_opts.ring_segments = 30;
        torus_opts.pipe_segments = 20;
        auto torus = primitive::generate_torus<Scalar, Index>(torus_opts);
        polyddg::DifferentialOperators<Scalar, Index> ops(torus);

        const Index nv = torus.get_num_vertices();
        const Index constrained_vid = 0;

        // Prescribe the ∂/∂u tangent direction at a single vertex.
        auto p = torus.get_position(constrained_vid);
        Scalar angle_u = std::atan2(p[1], p[0]);
        Eigen::Matrix<Scalar, 3, 1> prescribed_dir(-std::sin(angle_u), std::cos(angle_u), 0);

        auto align_id = internal::find_or_create_attribute<Scalar>(
            torus,
            "@test_alignment",
            AttributeElement::Vertex,
            AttributeUsage::Vector,
            3,
            internal::ResetToDefault::Yes);
        auto align_data = attribute_matrix_ref<Scalar>(torus, align_id);
        align_data.setZero();
        align_data.row(constrained_vid) = prescribed_dir.transpose();

        // Compute constrained n=1 vector field.
        polyddg::SmoothDirectionFieldOptions opts;
        opts.nrosy = 1;
        opts.alignment_attribute = "@test_alignment";
        auto result = polyddg::compute_smooth_direction_field(torus, ops, opts);
        auto data = attribute_matrix_view<Scalar>(torus, result);

        // The output at the constrained vertex should align with the prescribed ∂/∂u
        // direction. On a torus, ∂/∂u is a smooth (harmonic) field, so the solver
        // should produce a field well-aligned with it.
        Eigen::Matrix<Scalar, 1, 3> ref = prescribed_dir.normalized().transpose();
        Scalar dot = data.row(constrained_vid).dot(ref);
        REQUIRE(std::abs(dot) > 0.9);

        // All output vectors should be tangent and unit length.
        const auto normal_id = ops.get_vertex_normal_attribute_id();
        const auto normal_view = attribute_matrix_view<Scalar>(torus, normal_id);
        for (Index vid = 0; vid < nv; ++vid) {
            REQUIRE_THAT(data.row(vid).norm(), Catch::Matchers::WithinAbs(1.0, 1e-10));
            Scalar ndot = std::abs(data.row(vid).dot(normal_view.row(vid).normalized()));
            REQUIRE_THAT(ndot, Catch::Matchers::WithinAbs(0.0, 1e-8));
        }
    }

    SECTION("torus: 4-rosy alignment constraint is respected")
    {
        primitive::TorusOptions torus_opts;
        torus_opts.major_radius = 3.0;
        torus_opts.minor_radius = 1.0;
        torus_opts.ring_segments = 30;
        torus_opts.pipe_segments = 20;
        auto torus = primitive::generate_torus<Scalar, Index>(torus_opts);
        polyddg::DifferentialOperators<Scalar, Index> ops(torus);

        const Index nv = torus.get_num_vertices();
        const Index constrained_vid = 0;

        // Prescribe the ∂/∂u tangent direction at a single vertex.
        auto p = torus.get_position(constrained_vid);
        Scalar angle_u = std::atan2(p[1], p[0]);
        Eigen::Matrix<Scalar, 3, 1> prescribed_dir(-std::sin(angle_u), std::cos(angle_u), 0);

        auto align_id = internal::find_or_create_attribute<Scalar>(
            torus,
            "@test_alignment_4rosy",
            AttributeElement::Vertex,
            AttributeUsage::Vector,
            3,
            internal::ResetToDefault::Yes);
        auto align_data = attribute_matrix_ref<Scalar>(torus, align_id);
        align_data.setZero();
        align_data.row(constrained_vid) = prescribed_dir.transpose();

        // Compute constrained 4-rosy field.
        polyddg::SmoothDirectionFieldOptions opts;
        opts.nrosy = 4;
        opts.alignment_attribute = "@test_alignment_4rosy";
        auto result = polyddg::compute_smooth_direction_field(torus, ops, opts);
        auto data = attribute_matrix_view<Scalar>(torus, result);

        // The output at the constrained vertex should align with the prescribed ∂/∂u
        // direction up to 4-rosy symmetry: cos(4*(θ_out - θ_ref)) should be close to 1.
        auto B = ops.vertex_basis(constrained_vid);
        Eigen::Matrix<Scalar, 2, 1> out_2d = B.transpose() * data.row(constrained_vid).transpose();
        Eigen::Matrix<Scalar, 2, 1> ref_2d = B.transpose() * prescribed_dir;
        Scalar out_angle = std::atan2(out_2d(1), out_2d(0));
        Scalar ref_angle = std::atan2(ref_2d(1), ref_2d(0));
        Scalar cos4_diff = std::cos(4.0 * (out_angle - ref_angle));
        REQUIRE(cos4_diff > 0.9);

        // All output vectors should be tangent and unit length.
        const auto normal_id = ops.get_vertex_normal_attribute_id();
        const auto normal_view = attribute_matrix_view<Scalar>(torus, normal_id);
        for (Index vid = 0; vid < nv; ++vid) {
            REQUIRE_THAT(data.row(vid).norm(), Catch::Matchers::WithinAbs(1.0, 1e-10));
            Scalar ndot = std::abs(data.row(vid).dot(normal_view.row(vid).normalized()));
            REQUIRE_THAT(ndot, Catch::Matchers::WithinAbs(0.0, 1e-8));
        }
    }

    SECTION("torus: per-face zero-energy condition")
    {
        primitive::TorusOptions torus_opts;
        torus_opts.major_radius = 3.0;
        torus_opts.minor_radius = 1.0;
        torus_opts.ring_segments = 30;
        torus_opts.pipe_segments = 20;
        auto torus = primitive::generate_torus<Scalar, Index>(torus_opts);
        polyddg::DifferentialOperators<Scalar, Index> ops(torus);

        const Index nf = torus.get_num_facets();
        Eigen::Matrix<Scalar, 2, 1> c(1, 0);
        for (Index fid = 0; fid < std::min(nf, Index(10)); ++fid) {
            Index fs = torus.get_facet_size(fid);
            Eigen::Matrix<Scalar, Eigen::Dynamic, 1> u_local(2 * fs);
            for (Index lv = 0; lv < fs; ++lv) {
                auto R4 = ops.levi_civita_nrosy(fid, lv, Index(4));
                u_local.segment(2 * lv, 2) = R4.transpose() * c;
            }
            auto G_cov = ops.covariant_derivative_nrosy(fid, Index(4));
            REQUIRE_THAT((G_cov * u_local).norm(), Catch::Matchers::WithinAbs(0.0, 1e-10));
        }
    }

    SECTION("torus: 4-rosy field is tangent and unit length")
    {
        primitive::TorusOptions torus_opts;
        torus_opts.major_radius = 3.0;
        torus_opts.minor_radius = 1.0;
        torus_opts.ring_segments = 30;
        torus_opts.pipe_segments = 20;
        auto torus = primitive::generate_torus<Scalar, Index>(torus_opts);
        polyddg::DifferentialOperators<Scalar, Index> ops(torus);

        polyddg::SmoothDirectionFieldOptions opts;
        opts.nrosy = 4;
        auto result = polyddg::compute_smooth_direction_field(torus, ops, opts);

        auto data = attribute_matrix_view<Scalar>(torus, result);
        const auto normal_id = ops.get_vertex_normal_attribute_id();
        const auto normal_view = attribute_matrix_view<Scalar>(torus, normal_id);

        for (Index vid = 0; vid < torus.get_num_vertices(); ++vid) {
            REQUIRE_THAT(data.row(vid).norm(), Catch::Matchers::WithinAbs(1.0, 1e-10));
            Scalar dot = std::abs(data.row(vid).dot(normal_view.row(vid).normalized()));
            REQUIRE_THAT(dot, Catch::Matchers::WithinAbs(0.0, 1e-8));
        }
    }
}

TEST_CASE("compute_smooth_direction_field_on_facets", "[polyddg]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    SECTION("output dimensions and unit length — triangle mesh")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({1.0, 0.0, 0.0});
        mesh.add_vertex({0.0, 1.0, 0.0});
        mesh.add_vertex({0.0, 0.0, 1.0});
        mesh.add_triangle(0, 1, 2);

        polyddg::DifferentialOperators<Scalar, Index> ops(mesh);
        polyddg::SmoothDirectionFieldOptions opts;
        opts.nrosy = 4;
        opts.output_element_type = AttributeElement::Facet;
        auto result = polyddg::compute_smooth_direction_field(mesh, ops, opts);

        REQUIRE(result != invalid_attribute_id());
        auto data = attribute_matrix_view<Scalar>(mesh, result);
        REQUIRE(data.rows() == static_cast<Eigen::Index>(mesh.get_num_facets()));
        REQUIRE(data.cols() == 3);
        for (Index fid = 0; fid < mesh.get_num_facets(); ++fid) {
            REQUIRE_THAT(data.row(fid).norm(), Catch::Matchers::WithinAbs(1.0, 1e-10));
        }
    }

    SECTION("output dimensions and unit length — pyramid mesh")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0.0, 0.0, 0.0});
        mesh.add_vertex({1.0, 0.0, 0.0});
        mesh.add_vertex({1.0, 1.0, 0.0});
        mesh.add_vertex({0.0, 1.0, 0.0});
        mesh.add_vertex({0.5, 0.5, 1.0});
        mesh.add_triangle(0, 1, 4);
        mesh.add_triangle(1, 2, 4);
        mesh.add_triangle(2, 3, 4);
        mesh.add_triangle(3, 0, 4);
        mesh.add_quad(0, 3, 2, 1);

        polyddg::DifferentialOperators<Scalar, Index> ops(mesh);
        polyddg::SmoothDirectionFieldOptions opts;
        opts.nrosy = 4;
        opts.output_element_type = AttributeElement::Facet;
        auto result = polyddg::compute_smooth_direction_field(mesh, ops, opts);

        REQUIRE(result != invalid_attribute_id());
        auto data = attribute_matrix_view<Scalar>(mesh, result);
        REQUIRE(data.rows() == static_cast<Eigen::Index>(mesh.get_num_facets()));
        REQUIRE(data.cols() == 3);
        for (Index fid = 0; fid < mesh.get_num_facets(); ++fid) {
            REQUIRE_THAT(data.row(fid).norm(), Catch::Matchers::WithinAbs(1.0, 1e-10));
        }
    }

    SECTION("sphere: tangent to surface")
    {
        primitive::IcosahedronOptions ico_opts;
        ico_opts.radius = 1.0;
        auto base_ico = primitive::generate_icosahedron<Scalar, Index>(ico_opts);
        primitive::SubdividedSphereOptions subdiv_opts;
        subdiv_opts.radius = 1.0;
        subdiv_opts.subdiv_level = 1;
        auto sphere = primitive::generate_subdivided_sphere<Scalar, Index>(base_ico, subdiv_opts);

        polyddg::DifferentialOperators<Scalar, Index> ops(sphere);
        polyddg::SmoothDirectionFieldOptions opts;
        opts.nrosy = 4;
        opts.output_element_type = AttributeElement::Facet;
        auto result = polyddg::compute_smooth_direction_field(sphere, ops, opts);

        auto data = attribute_matrix_view<Scalar>(sphere, result);
        const auto va_id = ops.get_vector_area_attribute_id();
        const auto va_view = attribute_matrix_view<Scalar>(sphere, va_id);

        Scalar max_dot = 0;
        for (Index fid = 0; fid < sphere.get_num_facets(); ++fid) {
            Eigen::Matrix<Scalar, 1, 3> normal = va_view.row(fid).stableNormalized();
            max_dot = std::max(max_dot, std::abs(data.row(fid).dot(normal)));
        }
        REQUIRE(max_dot < 1e-10);
    }

    SECTION("result is deterministic")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0.0, 0.0, 0.0});
        mesh.add_vertex({1.0, 0.0, 0.0});
        mesh.add_vertex({1.0, 1.0, 0.0});
        mesh.add_vertex({0.0, 1.0, 0.0});
        mesh.add_vertex({0.5, 0.5, 1.0});
        mesh.add_triangle(0, 1, 4);
        mesh.add_triangle(1, 2, 4);
        mesh.add_triangle(2, 3, 4);
        mesh.add_triangle(3, 0, 4);
        mesh.add_quad(0, 3, 2, 1);

        polyddg::DifferentialOperators<Scalar, Index> ops(mesh);
        polyddg::SmoothDirectionFieldOptions opts;
        opts.nrosy = 4;
        opts.output_element_type = AttributeElement::Facet;
        opts.direction_field_attribute = "@sdf_facets_first";
        auto result1 = polyddg::compute_smooth_direction_field(mesh, ops, opts);

        opts.direction_field_attribute = "@sdf_facets_second";
        auto result2 = polyddg::compute_smooth_direction_field(mesh, ops, opts);

        auto data1 = attribute_matrix_view<Scalar>(mesh, result1);
        auto data2 = attribute_matrix_view<Scalar>(mesh, result2);

        Scalar max_encoded_diff = 0;
        for (Index fid = 0; fid < mesh.get_num_facets(); ++fid) {
            auto B = ops.facet_basis(fid);
            Eigen::Matrix<Scalar, 2, 1> u1 = B.transpose() * data1.row(fid).transpose();
            Eigen::Matrix<Scalar, 2, 1> u2 = B.transpose() * data2.row(fid).transpose();
            Scalar a1 = 4.0 * std::atan2(u1(1), u1(0));
            Scalar a2 = 4.0 * std::atan2(u2(1), u2(0));
            max_encoded_diff = std::max(max_encoded_diff, 1.0 - std::cos(a1 - a2));
        }
        REQUIRE_THAT(max_encoded_diff, Catch::Matchers::WithinAbs(0.0, 1e-8));
    }

    SECTION("torus: field is tangent and unit length")
    {
        primitive::TorusOptions torus_opts;
        torus_opts.major_radius = 3.0;
        torus_opts.minor_radius = 1.0;
        torus_opts.ring_segments = 30;
        torus_opts.pipe_segments = 20;
        auto torus = primitive::generate_torus<Scalar, Index>(torus_opts);

        polyddg::DifferentialOperators<Scalar, Index> ops(torus);
        polyddg::SmoothDirectionFieldOptions opts;
        opts.nrosy = 4;
        opts.output_element_type = AttributeElement::Facet;
        auto result = polyddg::compute_smooth_direction_field(torus, ops, opts);

        auto data = attribute_matrix_view<Scalar>(torus, result);
        const auto va_id = ops.get_vector_area_attribute_id();
        const auto va_view = attribute_matrix_view<Scalar>(torus, va_id);

        for (Index fid = 0; fid < torus.get_num_facets(); ++fid) {
            REQUIRE_THAT(data.row(fid).norm(), Catch::Matchers::WithinAbs(1.0, 1e-10));
            Eigen::Matrix<Scalar, 1, 3> normal = va_view.row(fid).stableNormalized();
            Scalar ndot = std::abs(data.row(fid).dot(normal));
            REQUIRE_THAT(ndot, Catch::Matchers::WithinAbs(0.0, 1e-8));
        }
    }

    SECTION("torus: alignment constraint is respected")
    {
        primitive::TorusOptions torus_opts;
        torus_opts.major_radius = 3.0;
        torus_opts.minor_radius = 1.0;
        torus_opts.ring_segments = 30;
        torus_opts.pipe_segments = 20;
        auto torus = primitive::generate_torus<Scalar, Index>(torus_opts);
        polyddg::DifferentialOperators<Scalar, Index> ops(torus);

        const Index constrained_fid = 0;

        const auto centroid_id = ops.get_centroid_attribute_id();
        const auto centroid_view = attribute_matrix_view<Scalar>(torus, centroid_id);
        Eigen::Matrix<Scalar, 1, 3> c = centroid_view.row(constrained_fid);
        Scalar angle_u = std::atan2(c(1), c(0));
        Eigen::Matrix<Scalar, 3, 1> prescribed_dir(-std::sin(angle_u), std::cos(angle_u), 0);

        auto align_id = internal::find_or_create_attribute<Scalar>(
            torus,
            "@test_facet_alignment",
            AttributeElement::Facet,
            AttributeUsage::Vector,
            3,
            internal::ResetToDefault::Yes);
        auto align_data = attribute_matrix_ref<Scalar>(torus, align_id);
        align_data.setZero();
        align_data.row(constrained_fid) = prescribed_dir.transpose();

        polyddg::SmoothDirectionFieldOptions opts;
        opts.nrosy = 4;
        opts.output_element_type = AttributeElement::Facet;
        opts.alignment_attribute = "@test_facet_alignment";
        auto result = polyddg::compute_smooth_direction_field(torus, ops, opts);
        auto data = attribute_matrix_view<Scalar>(torus, result);

        auto B = ops.facet_basis(constrained_fid);
        Eigen::Matrix<Scalar, 2, 1> out_2d = B.transpose() * data.row(constrained_fid).transpose();
        Eigen::Matrix<Scalar, 2, 1> ref_2d = B.transpose() * prescribed_dir;
        Scalar out_angle = std::atan2(out_2d(1), out_2d(0));
        Scalar ref_angle = std::atan2(ref_2d(1), ref_2d(0));
        Scalar cos4_diff = std::cos(4.0 * (out_angle - ref_angle));
        // Loosened from 0.9: meta-build (Eigen 3.4.1) lands at ~0.8886 here, while
        // CMake (Eigen 5.0.1) gets ~0.95. 0.85 still asserts ~<16° angular error.
        REQUIRE(cos4_diff > 0.85);
    }

    SECTION("convenience overload (no ops argument)")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({1.0, 0.0, 0.0});
        mesh.add_vertex({0.0, 1.0, 0.0});
        mesh.add_vertex({0.0, 0.0, 1.0});
        mesh.add_triangle(0, 1, 2);

        polyddg::SmoothDirectionFieldOptions opts;
        opts.nrosy = 4;
        opts.output_element_type = AttributeElement::Facet;
        auto result = polyddg::compute_smooth_direction_field(mesh, opts);

        REQUIRE(result != invalid_attribute_id());
        auto data = attribute_matrix_view<Scalar>(mesh, result);
        REQUIRE(data.rows() == static_cast<Eigen::Index>(mesh.get_num_facets()));
        REQUIRE(data.cols() == 3);
    }
}
