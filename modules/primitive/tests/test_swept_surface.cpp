/*
 * Copyright 2021 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/ExactPredicatesShewchuk.h>
#include <lagrange/IndexedAttribute.h>
#include <lagrange/common.h>
#include <lagrange/compute_euler.h>
#include <lagrange/compute_uv_charts.h>
#include <lagrange/create_mesh.h>
#include <lagrange/io/save_mesh.h>
#include <lagrange/mesh_cleanup/detect_degenerate_triangles.h>
#include <lagrange/primitive/SweepPath.h>
#include <lagrange/primitive/generate_swept_surface.h>
#include <lagrange/testing/common.h>
#include <lagrange/topology.h>
#include <lagrange/views.h>
#include <catch2/catch_approx.hpp>

#include "primitive_test_utils.h"

TEST_CASE("generate_swept_surface", "[primitive][surface]")
{
    using namespace lagrange;

    using Scalar = double;
    using Index = uint32_t;
    using Profile = Eigen::Matrix<Scalar, Eigen::Dynamic, 2, Eigen::RowMajor>;

    SECTION("simple segment sweep")
    {
        Profile profile(2, 2);
        profile << 0.0, 0.0, 1.0, 0.0;

        auto sweep_setting = primitive::SweepOptions<Scalar>::linear_sweep({0, 0, 0}, {0, 1, 0});

        auto mesh = primitive::generate_swept_surface<Scalar, Index>(
            {profile.data(), static_cast<size_t>(profile.size())},
            sweep_setting);
        REQUIRE(mesh.get_num_vertices() == 4);
        REQUIRE(mesh.get_num_facets() == 1);
        REQUIRE(compute_uv_charts(mesh) == 1);
    }

    SECTION("two segments sweep (smooth)")
    {
        Profile profile(3, 2);
        profile << 0.0, 0.0, 1.0, 0.0, 2.0, 0.1;

        auto sweep_setting = primitive::SweepOptions<Scalar>::linear_sweep({0, 0, 0}, {0, 0, 1});

        auto mesh = primitive::generate_swept_surface<Scalar, Index>(
            {profile.data(), static_cast<size_t>(profile.size())},
            sweep_setting);
        REQUIRE(mesh.get_num_vertices() == 6);
        REQUIRE(mesh.get_num_facets() == 2);
        REQUIRE(compute_uv_charts(mesh) == 1);
    }

    SECTION("two segments sweep (sharp)")
    {
        Profile profile(3, 2);
        profile << 0.0, 0.0, 1.0, 0.0, 0.0, 1.0;

        auto sweep_setting = primitive::SweepOptions<Scalar>::linear_sweep({0, 0, 0}, {0, 0, 1});
        primitive::SweptSurfaceOptions options;

        auto check_attributes = [&](auto& mesh) {
            REQUIRE(mesh.has_attribute(options.uv_attribute_name));
            REQUIRE(mesh.is_attribute_indexed(options.uv_attribute_name));

            REQUIRE(mesh.has_attribute(options.normal_attribute_name));
            REQUIRE(mesh.is_attribute_indexed(options.normal_attribute_name));

            REQUIRE(mesh.has_attribute(options.latitude_attribute_name));
            REQUIRE(mesh.is_attribute_indexed(options.latitude_attribute_name));

            REQUIRE(mesh.has_attribute(options.longitude_attribute_name));
            REQUIRE(mesh.is_attribute_indexed(options.longitude_attribute_name));
        };

        SECTION("default options")
        {
            options.fixed_uv = false;

            auto mesh = primitive::generate_swept_surface<Scalar, Index>(
                {profile.data(), static_cast<size_t>(profile.size())},
                sweep_setting,
                options);
            REQUIRE(mesh.get_num_vertices() == 6);
            REQUIRE(mesh.get_num_facets() == 2);
            REQUIRE(compute_uv_charts(mesh) == 2);

            check_attributes(mesh);
        }

        SECTION("fixed_uv = true")
        {
            options.fixed_uv = true;

            auto mesh = primitive::generate_swept_surface<Scalar, Index>(
                {profile.data(), static_cast<size_t>(profile.size())},
                sweep_setting,
                options);
            REQUIRE(mesh.get_num_vertices() == 6);
            REQUIRE(mesh.get_num_facets() == 2);
            REQUIRE(compute_uv_charts(mesh) == 1);

            check_attributes(mesh);
        }
    }

    SECTION("Triangle prism")
    {
        // Convert 3D profile to 2D profile (taking only x,y coordinates)
        Profile profile(4, 2);
        profile << 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 0.0;

        auto sweep_setting = primitive::SweepOptions<Scalar>::linear_sweep({0, 0, 0}, {0, 0, 10});

        auto mesh = primitive::generate_swept_surface<Scalar, Index>(
            {profile.data(), static_cast<size_t>(profile.size())},
            sweep_setting);

        // Note: vertex/facet counts may differ from legacy API due to different implementation
        REQUIRE(mesh.get_num_vertices() > 0);
        REQUIRE(mesh.get_num_facets() > 0);

        primitive_test_utils::validate_primitive(mesh, 2);
        primitive_test_utils::check_degeneracy(mesh);
        primitive_test_utils::check_UV(mesh);
    }

    SECTION("Torus")
    {
        constexpr Index N = 16;
        constexpr Scalar r = 1;
        constexpr Scalar R = 3;

        Profile profile(N + 1, 2);
        for (Index i = 0; i <= N; i++) {
            Scalar theta = (Scalar)i / (Scalar)N * 2 * lagrange::internal::pi;
            profile.row(i) << r * std::cos(theta), r * std::sin(theta);
        }

        // Create circular sweep path
        auto sweep_setting = primitive::SweepOptions<Scalar>::circular_sweep({R, 0, 0}, {0, 0, 1});
        sweep_setting.set_num_samples(64 + 1);

        auto mesh = primitive::generate_swept_surface<Scalar, Index>(
            {profile.data(), static_cast<size_t>(profile.size())},
            sweep_setting);

        primitive_test_utils::validate_primitive(mesh);
        primitive_test_utils::check_degeneracy(mesh);
        primitive_test_utils::check_UV(mesh);
    }

    SECTION("Profile translation")
    {
        Profile profile_1(4, 2);
        profile_1 << 0, 0, 1, 0, 1, 1, 0, 1;
        Profile profile_2 = profile_1.array() + 10;

        Eigen::Matrix<Scalar, 1, 2> profile_center_1 = profile_1.colwise().mean();
        Eigen::Matrix<Scalar, 1, 2> profile_center_2 = profile_2.colwise().mean();

        // Create circular sweep path
        auto sweep_setting_1 =
            primitive::SweepOptions<Scalar>::circular_sweep({3, 0, 0}, {0, 0, 1});
        sweep_setting_1.set_num_samples(8 + 1);
        sweep_setting_1.set_pivot({profile_center_1(0), profile_center_1(1), 0});

        auto sweep_setting_2 =
            primitive::SweepOptions<Scalar>::circular_sweep({3, 0, 0}, {0, 0, 1});
        sweep_setting_2.set_num_samples(8 + 1);
        sweep_setting_2.set_pivot({profile_center_2(0), profile_center_2(1), 0});

        auto mesh_1 = primitive::generate_swept_surface<Scalar, Index>(
            {profile_1.data(), static_cast<size_t>(profile_1.size())},
            sweep_setting_1);

        auto mesh_2 = primitive::generate_swept_surface<Scalar, Index>(
            {profile_2.data(), static_cast<size_t>(profile_2.size())},
            sweep_setting_2);

        // Compute bounding box diagonals
        auto compute_bbox_diag = [](const auto& mesh) {
            const auto& vertices = vertex_view(mesh);
            Eigen::Matrix<Scalar, 1, 3> bbox_min = vertices.colwise().minCoeff();
            Eigen::Matrix<Scalar, 1, 3> bbox_max = vertices.colwise().maxCoeff();
            return (bbox_max - bbox_min).norm();
        };

        Scalar bbox_diag_1 = compute_bbox_diag(mesh_1);
        Scalar bbox_diag_2 = compute_bbox_diag(mesh_2);

        REQUIRE(bbox_diag_1 == Catch::Approx(bbox_diag_2));
    }

    SECTION("twisted torus")
    {
        Profile profile(5, 2);
        profile << 0, 0, 1, 0, 1, 1, 0, 1, 0, 0;

        auto sweep_setting = primitive::SweepOptions<Scalar>::circular_sweep({5, 0, 0}, {0, 0, 1});
        sweep_setting.set_num_samples(33);
        sweep_setting.set_pivot({0.5, 0.5, 0});
        sweep_setting.set_twist_function([](Scalar t) { return 2 * lagrange::internal::pi * t; });

        auto mesh = primitive::generate_swept_surface<Scalar, Index>(
            {profile.data(), static_cast<size_t>(profile.size())},
            sweep_setting);

        auto euler = lagrange::compute_euler<Scalar, Index>(mesh);
        REQUIRE(euler == 0);
        primitive_test_utils::validate_primitive(mesh);
        primitive_test_utils::check_degeneracy(mesh);
        primitive_test_utils::check_UV(mesh);
    }

    SECTION("offset")
    {
        Profile profile(5, 2);
        profile << 0, 0, 1, 0, 1, 1, 0, 1, 0, 0;

        auto sweep_setting = primitive::SweepOptions<Scalar>::circular_sweep({5, 0, 0}, {0, 0, 1});
        sweep_setting.set_num_samples(65);
        sweep_setting.set_pivot({0.5, 0.5, 0});
        sweep_setting.set_twist_function([](Scalar t) { return 2 * lagrange::internal::pi * t; });
        sweep_setting.set_offset_function(
            [](Scalar t) { return 1 + 0.2 * std::sin(t * 8 * lagrange::internal::pi); });

        auto mesh = primitive::generate_swept_surface<Scalar, Index>(
            {profile.data(), static_cast<size_t>(profile.size())},
            sweep_setting);

        auto euler = lagrange::compute_euler<Scalar, Index>(mesh);
        REQUIRE(euler == 0);
        primitive_test_utils::validate_primitive(mesh);
        primitive_test_utils::check_degeneracy(mesh);
        primitive_test_utils::check_UV(mesh);
    }

    SECTION("Normalization")
    {
        Profile profile(5, 2);
        profile << 0, 0, 1, 0, 1, 1, 0, 1, 0, 0;

        using Transform = typename primitive::SweepOptions<Scalar>::Transform;
        Transform normalization;
        normalization.setIdentity();
        normalization.scale(2);

        auto sweep_setting = primitive::SweepOptions<Scalar>::linear_sweep({0, 0, 0}, {0, 0, 1});
        sweep_setting.set_normalization(normalization);

        auto mesh = primitive::generate_swept_surface<Scalar, Index>(
            {profile.data(), static_cast<size_t>(profile.size())},
            sweep_setting);

        const auto& vertices = vertex_view(mesh);
        REQUIRE(vertices.col(2).maxCoeff() == Catch::Approx(0.5).margin(1e-6));
        REQUIRE(vertices.col(2).minCoeff() == Catch::Approx(0).margin(1e-6));

        primitive_test_utils::check_degeneracy(mesh);
        primitive_test_utils::check_UV(mesh);
    }

    SECTION("Zero depth")
    {
        Profile profile(2, 2);
        profile << 0, 0, 1, 0;

        // Create a sweep setting that represents zero depth (single point)
        auto sweep_setting =
            primitive::SweepOptions<Scalar>::circular_sweep({1, 0, 0}, {0, 0, 1}, 0);
        sweep_setting.set_num_samples(2); // Need at least 2 samples for sweep generation
        // Create very small sweep arc to simulate zero depth
        sweep_setting.set_position_function([](Scalar /*t*/) {
            return typename primitive::SweepOptions<Scalar>::Point(1, 0, 0); // Fixed position
        });

        auto mesh = primitive::generate_swept_surface<Scalar, Index>(
            {profile.data(), static_cast<size_t>(profile.size())},
            sweep_setting);

        REQUIRE(mesh.get_num_vertices() > 0);
        REQUIRE(mesh.get_num_facets() > 0);

        auto euler = lagrange::compute_euler<Scalar, Index>(mesh);
        REQUIRE(euler == 1); // i.e. should be of disc topology.
        primitive_test_utils::validate_primitive(mesh, 1);
    }

    SECTION("Degenerate edge in profile")
    {
        Profile profile(3, 2);
        profile << 0, 0, 1, 0, 1, 0; // Contains a degenerate edge (last two points have same y)

        auto sweep_setting =
            primitive::SweepOptions<Scalar>::circular_sweep({1, 0, 0}, {0, 0, 1}, 1);
        sweep_setting.set_num_samples(16);

        primitive::SweptSurfaceOptions options;

        auto mesh = primitive::generate_swept_surface<Scalar, Index>(
            {profile.data(), static_cast<size_t>(profile.size())},
            sweep_setting,
            options);

        REQUIRE(mesh.get_num_vertices() > 0);
        REQUIRE(mesh.get_num_facets() > 0);

        REQUIRE(vertex_view(mesh).array().isFinite().all());

        auto euler = lagrange::compute_euler<Scalar, Index>(mesh);
        REQUIRE(euler == 1); // i.e. should be of disc topology.
        primitive_test_utils::validate_primitive(mesh, 1);

        // Check normals.
        if (mesh.has_attribute(options.normal_attribute_name)) {
            auto& normal_attr = mesh.get_indexed_attribute<Scalar>(options.normal_attribute_name);
            auto normals = matrix_view(normal_attr.values());
            REQUIRE(normals.array().isFinite().all());
        }

        // Check UVs.
        if (mesh.has_attribute(options.uv_attribute_name)) {
            auto& uv_attr = mesh.get_indexed_attribute<Scalar>(options.uv_attribute_name);
            auto uv_values = matrix_view(uv_attr.values());
            REQUIRE(uv_values.array().isFinite().all());
        }
    }
}

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
TEST_CASE("SweptSurface", "[primitive][swept_surface]")
{
    using namespace lagrange;

    using Scalar = double;
    using Index = uint32_t;
    using MeshType =
        Mesh<Eigen::Matrix<Scalar, Eigen::Dynamic, 3>, Eigen::Matrix<Index, Eigen::Dynamic, 3>>;
    using VertexArray = typename MeshType::VertexArray;

    SECTION("Triangle prism")
    {
        VertexArray profile(4, 3);
        profile << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0;

        VertexArray sweep_path(2, 3);
        sweep_path << 0.0, 0.0, 0.0, 0.0, 0.0, 10.0;

        auto mesh = lagrange::primitive::generate_swept_surface<MeshType>(profile, sweep_path);
        REQUIRE(mesh);
        REQUIRE(mesh->get_num_vertices() == 9);
        REQUIRE(mesh->get_num_facets() == 12);
        primitive_test_utils::validate_primitive(*mesh, 2);
        primitive_test_utils::check_degeneracy(*mesh);
        primitive_test_utils::check_semantic_labels(*mesh);
        primitive_test_utils::check_UV(*mesh);
    }

    SECTION("Torus")
    {
        constexpr Index N = 16;
        constexpr Index M = 64;
        constexpr Scalar r = 1;
        constexpr Scalar R = 3;

        VertexArray profile(N + 1, 3);
        for (Index i = 0; i <= N; i++) {
            Scalar theta = (Scalar)i / (Scalar)N * 2 * lagrange::internal::pi;
            profile.row(i) << std::cos(theta), std::sin(theta), 0;
        }
        profile *= r;

        VertexArray sweep_path(M + 1, 3);
        for (Index i = 0; i <= M; i++) {
            Scalar theta = (Scalar)i / (Scalar)M * 2 * lagrange::internal::pi;
            sweep_path.row(i) << 0, std::cos(theta), std::sin(theta);
        }
        sweep_path *= R;

        auto mesh = lagrange::primitive::generate_swept_surface<MeshType>(profile, sweep_path);
        REQUIRE(mesh);
        // io::save_mesh("debug.obj", *mesh);
        primitive_test_utils::validate_primitive(*mesh);
        primitive_test_utils::check_degeneracy(*mesh);
        primitive_test_utils::check_semantic_labels(*mesh);
        primitive_test_utils::check_UV(*mesh);
    }

    SECTION("Profile translaiton")
    {
        VertexArray profile_1(4, 3);
        profile_1 << 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0;
        VertexArray profile_2 = profile_1.array() + 10;

        constexpr Index M = 8;
        constexpr Scalar R = 3;
        VertexArray sweep_path(M + 1, 3);
        for (Index i = 0; i <= M; i++) {
            Scalar theta = (Scalar)i / (Scalar)M * 2 * lagrange::internal::pi;
            sweep_path.row(i) << 0, std::cos(theta), std::sin(theta);
        }
        sweep_path *= R;

        auto mesh_1 = lagrange::primitive::generate_swept_surface<MeshType>(profile_1, sweep_path);
        auto mesh_2 = lagrange::primitive::generate_swept_surface<MeshType>(profile_2, sweep_path);

        typename MeshType::VertexType bbox_min_1 = mesh_1->get_vertices().colwise().minCoeff();
        typename MeshType::VertexType bbox_max_1 = mesh_1->get_vertices().colwise().maxCoeff();
        typename MeshType::VertexType bbox_min_2 = mesh_2->get_vertices().colwise().minCoeff();
        typename MeshType::VertexType bbox_max_2 = mesh_2->get_vertices().colwise().maxCoeff();

        Scalar bbox_diag_1 = (bbox_max_1 - bbox_min_1).norm();
        Scalar bbox_diag_2 = (bbox_max_2 - bbox_min_2).norm();

        REQUIRE(bbox_diag_1 == Catch::Approx(bbox_diag_2));
    }

    SECTION("Transformed profile should in angle bisector plane")
    {
        VertexArray profile(5, 3);
        profile << 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0;

        VertexArray sweep_path(5, 3);
        sweep_path << 0, 0, 0, 1, 0, 1, 2, 0, 0, 1, 0, -1, 0, 0, 0;

        auto mesh = lagrange::primitive::generate_swept_surface<MeshType>(profile, sweep_path);
        const auto& vertices = mesh->get_vertices();

        REQUIRE(vertices.rows() == 32);
        REQUIRE(vertices.col(2).segment(0, 4).mean() == Catch::Approx(vertices(0, 2)).margin(1e-6));
        REQUIRE(vertices.col(0).segment(4, 4).mean() == Catch::Approx(vertices(4, 0)).margin(1e-6));
        REQUIRE(vertices.col(2).segment(8, 4).mean() == Catch::Approx(vertices(8, 2)).margin(1e-6));
        REQUIRE(
            vertices.col(0).segment(12, 4).mean() == Catch::Approx(vertices(12, 0)).margin(1e-6));
    }

    SECTION("Sweep path")
    {
        VertexArray profile(5, 3);
        profile << 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0;

        VertexArray polyline(5, 3);
        polyline << 0, 0, 0, 0, 1, 1, 0, 2, 0, 0, 1, -1, 0, 0, 0;
        polyline *= 3;

        lagrange::primitive::PolylineSweepPath<VertexArray> sweep_path(polyline);
        sweep_path.set_num_samples(33);
        sweep_path.set_twist_end(2 * lagrange::internal::pi);
        sweep_path.initialize();

        auto mesh = lagrange::primitive::generate_swept_surface<MeshType>(profile, sweep_path);
        auto euler = lagrange::compute_euler(*mesh);
        REQUIRE(euler == 0);
        primitive_test_utils::validate_primitive(*mesh);
        primitive_test_utils::check_UV(*mesh);
    }

    SECTION("twisted torus")
    {
        VertexArray profile(5, 3);
        profile << 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0;

        lagrange::primitive::CircularArcSweepPath<Scalar> sweep_path(5, 0);
        sweep_path.set_num_samples(33);
        sweep_path.set_twist_end(2 * lagrange::internal::pi);
        sweep_path.set_pivot({0.5, 0.5, 0});
        sweep_path.initialize();

        auto mesh = lagrange::primitive::generate_swept_surface<MeshType>(profile, sweep_path);
        // io::save_mesh("debug.obj", *mesh);
        auto euler = lagrange::compute_euler(*mesh);
        REQUIRE(euler == 0);
        primitive_test_utils::validate_primitive(*mesh);
        primitive_test_utils::check_UV(*mesh);
    }

    SECTION("offset")
    {
        VertexArray profile(5, 3);
        profile << 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0;

        lagrange::primitive::CircularArcSweepPath<Scalar> sweep_path(5, 0);
        sweep_path.set_num_samples(65);
        sweep_path.set_offset_fn(
            [](Scalar t) { return 1 + 0.2 * std::sin(t * 8 * lagrange::internal::pi); });
        sweep_path.set_twist_end(2 * lagrange::internal::pi);
        sweep_path.set_pivot({0.5, 0.5, 0});
        sweep_path.initialize();

        auto mesh = lagrange::primitive::generate_swept_surface<MeshType>(profile, sweep_path);
        // io::save_mesh("debug.obj", *mesh);
        auto euler = lagrange::compute_euler(*mesh);
        REQUIRE(euler == 0);
        primitive_test_utils::validate_primitive(*mesh);
        primitive_test_utils::check_UV(*mesh);
    }

    SECTION("Normalization")
    {
        VertexArray profile(5, 3);
        profile << 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0;

        Eigen::Transform<Scalar, 3, Eigen::AffineCompact> normalization;
        normalization.setIdentity();
        normalization.scale(2);

        lagrange::primitive::LinearSweepPath<Scalar> sweep_path({0, 0, 1});
        sweep_path.set_depth_end(1);
        sweep_path.set_normalization_transform(normalization);
        sweep_path.initialize();

        auto mesh = lagrange::primitive::generate_swept_surface<MeshType>(profile, sweep_path);
        const auto& vertices = mesh->get_vertices();
        REQUIRE(vertices.col(2).maxCoeff() == Catch::Approx(0.5).margin(1e-6));
        REQUIRE(vertices.col(2).minCoeff() == Catch::Approx(0).margin(1e-6));
        primitive_test_utils::check_UV(*mesh);
    }

    SECTION("Zero depth")
    {
        VertexArray profile(2, 3);
        profile << 0, 0, 0, 1, 0, 0;

        lagrange::primitive::CircularArcSweepPath<Scalar> sweep_path(1, 0);
        sweep_path.set_depth_begin(0);
        sweep_path.set_depth_end(0);
        sweep_path.initialize();

        REQUIRE(!sweep_path.is_closed());

        auto mesh = lagrange::primitive::generate_swept_surface<MeshType>(profile, sweep_path);
        REQUIRE(mesh->get_num_vertices() > 0);
        REQUIRE(mesh->get_num_facets() > 0);

        auto euler = lagrange::compute_euler(*mesh);
        REQUIRE(euler == 1); // i.e. should be of disc topology.
        primitive_test_utils::validate_primitive(*mesh, 1);
    }

    SECTION("Degenerate edge in profile")
    {
        VertexArray profile(3, 3);
        profile << 0, 0, 0, 1, 0, 0, 1, 0, 0;

        lagrange::primitive::CircularArcSweepPath<Scalar> sweep_path(1, 0);
        sweep_path.set_angle_begin(0);
        sweep_path.set_angle_end(lagrange::internal::pi);
        sweep_path.initialize();

        REQUIRE(!sweep_path.is_closed());

        auto mesh = lagrange::primitive::generate_swept_surface<MeshType>(profile, sweep_path);
        REQUIRE(mesh->get_num_vertices() > 0);
        REQUIRE(mesh->get_num_facets() > 0);

        REQUIRE(mesh->get_vertices().array().isFinite().all());

        auto euler = lagrange::compute_euler(*mesh);
        REQUIRE(euler == 1); // i.e. should be of disc topology.
        primitive_test_utils::validate_primitive(*mesh, 1);

        {
            // Check normals.
            REQUIRE(mesh->has_indexed_attribute("normal"));
            auto r = mesh->get_indexed_attribute("normal");
            auto& normal_values = std::get<0>(r);
            REQUIRE(normal_values.array().isFinite().all());
        }

        {
            // Check UVs.
            REQUIRE(mesh->has_indexed_attribute("uv"));
            auto r = mesh->get_indexed_attribute("uv");
            auto& uv_values = std::get<0>(r);
            REQUIRE(uv_values.array().isFinite().all());
        }
    }
}

TEST_CASE("SweptSurface/Issue1257", "[primitive][swept_surface][structure][issue1257]")
{
    using namespace lagrange;

    using Scalar = double;
    using Index = uint32_t;
    using MeshType =
        Mesh<Eigen::Matrix<Scalar, Eigen::Dynamic, 3>, Eigen::Matrix<Index, Eigen::Dynamic, 3>>;
    using VertexArray = typename MeshType::VertexArray;

    constexpr Index N = 32;
    VertexArray profile(N + 1, 3);
    for (auto i : range(N + 1)) {
        Scalar t = 2 * lagrange::internal::pi * i / (Scalar)(N);
        profile.row(i) << cos(t), sin(t), 0;
    }

    primitive::LinearSweepPath<Scalar> sweep_path({0, 0, 1});
    sweep_path.initialize();
    auto mesh = primitive::generate_swept_surface<MeshType>(profile, sweep_path);
    auto uv_mesh = mesh->get_uv_mesh();
    uv_mesh->initialize_components();
    REQUIRE(uv_mesh->get_num_components() == 1);
    primitive_test_utils::check_UV(*mesh);
}

TEST_CASE("SweptSurface/normal", "[primitive][swept_surface]")
{
    using namespace lagrange;

    using Scalar = double;
    using Index = uint32_t;
    using MeshType =
        Mesh<Eigen::Matrix<Scalar, Eigen::Dynamic, 3>, Eigen::Matrix<Index, Eigen::Dynamic, 3>>;
    using VertexArray = typename MeshType::VertexArray;

    constexpr Scalar EPS = std::numeric_limits<Scalar>::epsilon();
    constexpr Index N = 32;
    VertexArray profile(N + 1, 3);
    for (auto i : range(N + 1)) {
        Scalar t = 2 * lagrange::internal::pi * i / (Scalar)(N);
        profile.row(i) << cos(t), sin(t), 0;
    }

    primitive::LinearSweepPath<Scalar> sweep_path({0, 0, 1});
    sweep_path.initialize();
    auto mesh = primitive::generate_swept_surface<MeshType>(profile, sweep_path);

    mesh->initialize_edge_data();
    const auto num_vertices = mesh->get_num_vertices();
    const auto normal_attr = mesh->get_indexed_attribute("normal");
    const auto& normal_values = std::get<0>(normal_attr);
    const auto& normal_indices = std::get<1>(normal_attr);

    for (auto vi : range(num_vertices)) {
        Eigen::Matrix<Scalar, 1, 3> normal{0, 0, 0};
        mesh->foreach_corners_around_vertex(vi, [&](Index ci) {
            const Index fi = ci / 3;
            const Index li = ci % 3;
            const Index normal_id = normal_indices(fi, li);
            if (normal.norm() < EPS) {
                normal = normal_values.row(normal_id);
            } else {
                CHECK((normal - normal_values.row(normal_id)).norm() < EPS);
            }
        });
    }
}

TEST_CASE("SweptSurface/TwistingNormal", "[primitive][swept_surface]")
{
    using namespace lagrange;

    using Scalar = double;
    using Index = uint32_t;
    using MeshType =
        Mesh<Eigen::Matrix<Scalar, Eigen::Dynamic, 3>, Eigen::Matrix<Index, Eigen::Dynamic, 3>>;
    using VertexArray = typename MeshType::VertexArray;

    constexpr Index N = 32;
    VertexArray profile(N + 1, 3);
    for (auto i : range(N + 1)) {
        profile.row(i) << (Scalar)i / (Scalar)N, 0, 0;
    }

    primitive::LinearSweepPath<Scalar> sweep_path({0, 0, 1});
    sweep_path.set_twist_end(lagrange::internal::pi);
    sweep_path.set_depth_end(2);
    sweep_path.set_num_samples(18);
    sweep_path.set_pivot({0.5f, 0, 0});
    sweep_path.initialize();
    auto mesh = primitive::generate_swept_surface<MeshType>(profile, sweep_path);
    REQUIRE(mesh->has_indexed_attribute("normal"));

    mesh->initialize_edge_data();
    const auto num_facets = mesh->get_num_facets();
    const auto normal_attr = mesh->get_indexed_attribute("normal");
    const auto& normal_values = std::get<0>(normal_attr);
    const auto& normal_indices = std::get<1>(normal_attr);

    const auto& vertices = mesh->get_vertices();
    const auto& facets = mesh->get_facets();
    for (auto fi : range(num_facets)) {
        Index v0 = facets(fi, 0);
        Index v1 = facets(fi, 1);
        Index v2 = facets(fi, 2);

        Eigen::Matrix<Scalar, 1, 3> p0 = vertices.row(v0);
        Eigen::Matrix<Scalar, 1, 3> p1 = vertices.row(v1);
        Eigen::Matrix<Scalar, 1, 3> p2 = vertices.row(v2);

        Eigen::Matrix<Scalar, 1, 3> n0 = normal_values.row(normal_indices(fi, 0));
        Eigen::Matrix<Scalar, 1, 3> n1 = normal_values.row(normal_indices(fi, 1));
        Eigen::Matrix<Scalar, 1, 3> n2 = normal_values.row(normal_indices(fi, 2));

        Eigen::Matrix<Scalar, 1, 3> n = (p1 - p0).cross(p2 - p0).stableNormalized();

        REQUIRE(n.dot(n0) > 0.9);
        REQUIRE(n.dot(n1) > 0.9);
        REQUIRE(n.dot(n2) > 0.9);
    }
}

TEST_CASE("SweptSurface/TwistingNormal2", "[primitive][swept_surface]")
{
    using namespace lagrange;

    using Scalar = double;
    using Index = uint32_t;
    using MeshType =
        Mesh<Eigen::Matrix<Scalar, Eigen::Dynamic, 3>, Eigen::Matrix<Index, Eigen::Dynamic, 3>>;
    using VertexArray = typename MeshType::VertexArray;

    VertexArray profile(5, 3);
    // clang-format off
    profile << 0, 0, 0,
               1, 0, 0,
               1, 1, 0,
               0, 1, 0,
               0, 0, 0;
    // clang-format on

    primitive::LinearSweepPath<Scalar> sweep_path({0, 0, 1});
    sweep_path.set_twist_end(lagrange::internal::pi);
    sweep_path.set_depth_end(1);
    sweep_path.set_num_samples(18);
    sweep_path.set_pivot({-0.5f, -0.5f, 0});
    sweep_path.initialize();
    auto mesh = primitive::generate_swept_surface<MeshType>(profile, sweep_path);
    REQUIRE(mesh->has_indexed_attribute("normal"));

    mesh->initialize_edge_data();
    const auto num_facets = mesh->get_num_facets();
    const auto normal_attr = mesh->get_indexed_attribute("normal");
    const auto& normal_values = std::get<0>(normal_attr);
    const auto& normal_indices = std::get<1>(normal_attr);

    const auto& vertices = mesh->get_vertices();
    const auto& facets = mesh->get_facets();
    for (auto fi : range(num_facets)) {
        Index v0 = facets(fi, 0);
        Index v1 = facets(fi, 1);
        Index v2 = facets(fi, 2);

        Eigen::Matrix<Scalar, 1, 3> p0 = vertices.row(v0);
        Eigen::Matrix<Scalar, 1, 3> p1 = vertices.row(v1);
        Eigen::Matrix<Scalar, 1, 3> p2 = vertices.row(v2);

        Eigen::Matrix<Scalar, 1, 3> n0 = normal_values.row(normal_indices(fi, 0));
        Eigen::Matrix<Scalar, 1, 3> n1 = normal_values.row(normal_indices(fi, 1));
        Eigen::Matrix<Scalar, 1, 3> n2 = normal_values.row(normal_indices(fi, 2));

        Eigen::Matrix<Scalar, 1, 3> n = (p1 - p0).cross(p2 - p0).stableNormalized();

        REQUIRE(n.dot(n0) > 0.9);
        REQUIRE(n.dot(n1) > 0.9);
        REQUIRE(n.dot(n2) > 0.9);
    }
}

TEST_CASE("SweptSurface/latitude_and_longitude", "[primitive][swept_surface]")
{
    using VertexArray = Eigen::Matrix<double, Eigen::Dynamic, 3, Eigen::RowMajor>;
    SECTION("Triangle prism")
    {
        VertexArray profile(4, 3);
        profile << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0;

        VertexArray polyline(2, 3);
        polyline << 0.0, 0.0, 0.0, 0.0, 0.0, 10.0;

        lagrange::primitive::PolylineSweepPath<VertexArray> sweep_path(polyline);
        constexpr int N = 10;
        sweep_path.set_num_samples(N);
        sweep_path.initialize();

        auto latitudes = lagrange::primitive::generate_swept_surface_latitude(profile, sweep_path);
        auto longitudes =
            lagrange::primitive::generate_swept_surface_longitude(profile, sweep_path);
        REQUIRE(latitudes.size() == N);
        REQUIRE(longitudes.size() == 3);
        for (auto i : lagrange::range(N)) {
            for (auto j : lagrange::range(4)) {
                REQUIRE(latitudes[i](j, 0) == profile(j, 0));
                REQUIRE(latitudes[i](j, 1) == profile(j, 1));
                REQUIRE(latitudes[i](j, 2) == Catch::Approx(profile(j, 2) + 10 / 9.0 * i));
            }
        }

        for (auto i : lagrange::range(N)) {
            REQUIRE(longitudes[0](i, 0) == profile(0, 0));
            REQUIRE(longitudes[0](i, 1) == profile(0, 1));
            REQUIRE(longitudes[0](i, 2) == Catch::Approx(profile(0, 2) + 10 / 9.0 * i));

            REQUIRE(longitudes[1](i, 0) == profile(1, 0));
            REQUIRE(longitudes[1](i, 1) == profile(1, 1));
            REQUIRE(longitudes[1](i, 2) == Catch::Approx(profile(1, 2) + 10 / 9.0 * i));

            REQUIRE(longitudes[2](i, 0) == profile(2, 0));
            REQUIRE(longitudes[2](i, 1) == profile(2, 1));
            REQUIRE(longitudes[2](i, 2) == Catch::Approx(profile(2, 2) + 10 / 9.0 * i));
        }
    }
}

TEST_CASE("SweptSurface/longitude_distribution", "[primitive][swept_surface]")
{
    using Scalar = double;
    using Index = uint32_t;
    using MeshType = lagrange::
        Mesh<Eigen::Matrix<Scalar, Eigen::Dynamic, 3>, Eigen::Matrix<Index, Eigen::Dynamic, 3>>;
    using VertexArray = Eigen::Matrix<Scalar, Eigen::Dynamic, 3, Eigen::RowMajor>;

    SECTION("Straight line")
    {
        VertexArray profile(3, 3);
        profile << 0, 0, 0, 0.5, 0, 0, 1, 0, 0;

        VertexArray polyline(2, 3);
        polyline << 0, 0, 0, 0, 0, 10;

        lagrange::primitive::PolylineSweepPath<VertexArray> sweep_path(polyline);
        constexpr int N = 10;
        sweep_path.set_num_samples(N);
        sweep_path.initialize();

        auto mesh = lagrange::primitive::generate_swept_surface<MeshType>(profile, sweep_path);
        REQUIRE(mesh);
        REQUIRE(mesh->has_indexed_attribute("longitude"));

        const auto& vertices = mesh->get_vertices();
        const auto& facets = mesh->get_facets();

        {
            const auto& attr = mesh->get_indexed_attribute("longitude");
            const auto& values = std::get<0>(attr);
            const auto& indices = std::get<1>(attr);

            for (auto i : lagrange::range(mesh->get_num_facets())) {
                REQUIRE(values(indices(i, 0)) == Catch::Approx(vertices(facets(i, 0), 0)));
                REQUIRE(values(indices(i, 1)) == Catch::Approx(vertices(facets(i, 1), 0)));
                REQUIRE(values(indices(i, 2)) == Catch::Approx(vertices(facets(i, 2), 0)));
            }
        }

        {
            const auto& attr = mesh->get_indexed_attribute("latitude");
            const auto& values = std::get<0>(attr);
            const auto& indices = std::get<1>(attr);

            for (auto i : lagrange::range(mesh->get_num_facets())) {
                REQUIRE(values(indices(i, 0)) == Catch::Approx(0.1 * vertices(facets(i, 0), 2)));
                REQUIRE(values(indices(i, 1)) == Catch::Approx(0.1 * vertices(facets(i, 1), 2)));
                REQUIRE(values(indices(i, 2)) == Catch::Approx(0.1 * vertices(facets(i, 2), 2)));
            }
        }
    }

    SECTION("Circle")
    {
        constexpr size_t N = 32;
        VertexArray profile(N + 1, 3);
        for (auto i : lagrange::range(N + 1)) {
            Scalar theta = 2 * lagrange::internal::pi * i / (N);
            profile.row(i) << std::cos(theta), std::sin(theta), 0;
        }
        VertexArray polyline(2, 3);
        polyline << 0, 0, 0, 0, 0, 1;

        lagrange::primitive::PolylineSweepPath<VertexArray> sweep_path(polyline);
        sweep_path.set_num_samples(10);
        sweep_path.initialize();

        auto mesh = lagrange::primitive::generate_swept_surface<MeshType>(profile, sweep_path);
        REQUIRE(mesh);
        REQUIRE(mesh->has_indexed_attribute("longitude"));

        const auto& vertices = mesh->get_vertices();
        const auto& facets = mesh->get_facets();

        {
            const auto& attr = mesh->get_indexed_attribute("longitude");
            const auto& values = std::get<0>(attr);
            const auto& indices = std::get<1>(attr);

            for (auto i : lagrange::range(mesh->get_num_facets())) {
                Eigen::Vector3<Scalar> v0 = vertices.row(facets(i, 0));
                Eigen::Vector3<Scalar> v1 = vertices.row(facets(i, 1));
                Eigen::Vector3<Scalar> v2 = vertices.row(facets(i, 2));
                Scalar theta_0 = std::atan2(v0[1], v0[0]);
                Scalar theta_1 = std::atan2(v1[1], v1[0]);
                Scalar theta_2 = std::atan2(v2[1], v2[0]);
                if (theta_0 < 0) theta_0 += 2 * lagrange::internal::pi;
                if (theta_1 < 0) theta_1 += 2 * lagrange::internal::pi;
                if (theta_2 < 0) theta_2 += 2 * lagrange::internal::pi;
                Scalar value_0 = values(indices(i, 0));
                Scalar value_1 = values(indices(i, 1));
                Scalar value_2 = values(indices(i, 2));
                if (v0[1] != 0)
                    REQUIRE(value_0 == Catch::Approx(theta_0 / (2 * lagrange::internal::pi)));
                else
                    REQUIRE((value_0 == Catch::Approx(0) || value_0 == Catch::Approx(1)));
                if (v1[1] != 0)
                    REQUIRE(value_1 == Catch::Approx(theta_1 / (2 * lagrange::internal::pi)));
                else
                    REQUIRE((value_1 == Catch::Approx(0) || value_1 == Catch::Approx(1)));
                if (v2[1] != 0)
                    REQUIRE(value_2 == Catch::Approx(theta_2 / (2 * lagrange::internal::pi)));
                else
                    REQUIRE((value_2 == Catch::Approx(0) || value_2 == Catch::Approx(1)));
            }
        }

        {
            const auto& attr = mesh->get_indexed_attribute("latitude");
            const auto& values = std::get<0>(attr);
            const auto& indices = std::get<1>(attr);

            for (auto i : lagrange::range(mesh->get_num_facets())) {
                REQUIRE(values(indices(i, 0)) == Catch::Approx(vertices(facets(i, 0), 2)));
                REQUIRE(values(indices(i, 1)) == Catch::Approx(vertices(facets(i, 1), 2)));
                REQUIRE(values(indices(i, 2)) == Catch::Approx(vertices(facets(i, 2), 2)));
            }
        }
    }
}
#endif
