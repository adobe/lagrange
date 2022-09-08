/*
 * Copyright 2022 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/io/load_mesh.h>
#include <lagrange/raycasting/project_particles_directional.h>

#include <lagrange/testing/common.h>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <atomic>
#include <random>

TEST_CASE("project_particle_directional", "[raycasting]")
{
    const auto cube = lagrange::to_shared_ptr(lagrange::create_cube());
    REQUIRE(cube);

    using MeshType = decltype(cube)::element_type;
    using Scalar = typename MeshType::Scalar;
    using Vector3 = Eigen::Matrix<Scalar, 3, 1>;
    using Matrix4 = Eigen::Matrix<Scalar, 4, 4>;
    using Affine3 = Eigen::Transform<Scalar, 3, Eigen::Affine>;
    using Translation3 = Eigen::Translation<Scalar, 3>;
    using Rotation3 = Eigen::AngleAxis<Scalar>;
    using Scaling = Eigen::UniformScaling<Scalar>;
    using RayCasterType = lagrange::raycasting::EmbreeRayCaster<Scalar>;
    using Point = typename RayCasterType::Point;
    using Direction = typename RayCasterType::Direction;

    using ParticleDataType = std::vector<Vector3>;
    ParticleDataType test_origins = {
        Vector3(0., -1., -1.),
        Vector3(0., -1., 1.),
        Vector3(0., 1., -1.),
        Vector3(0., 1., 1.)};

    ParticleDataType test_origins_output;
    ParticleDataType test_normals_output;

    Matrix4 parent_transform = Matrix4::Identity();

    SECTION("empty output")
    {
        Vector3 project_dir = -Vector3::UnitX();
        auto dynamic_ray_caster = lagrange::raycasting::create_ray_caster<Scalar>(
            lagrange::raycasting::EMBREE_ROBUST,
            lagrange::raycasting::BUILD_QUALITY_MEDIUM);

        Matrix4 trans = Affine3(Translation3(10.0, 0.0, 0.0) * Scaling(1.5)).matrix();

        dynamic_ray_caster->add_mesh(cube, trans);
        dynamic_ray_caster->cast(Point(0, 0, 0), Direction(0, 0, 1));

        lagrange::raycasting::project_particles_directional(
            test_origins,
            *cube,
            project_dir,
            test_origins_output,
            test_normals_output,
            parent_transform,
            dynamic_ray_caster.get());

        REQUIRE(test_origins_output.size() == 0);
        REQUIRE(test_normals_output.size() == 0);
    }

    SECTION("non-empty output without normal filled")
    {
        Vector3 project_dir = Vector3::UnitX();
        auto dynamic_ray_caster = lagrange::raycasting::create_ray_caster<Scalar>(
            lagrange::raycasting::EMBREE_ROBUST,
            lagrange::raycasting::BUILD_QUALITY_MEDIUM);

        Scalar box_center_x = 10.0;
        Scalar box_size = 1.5;
        Matrix4 trans = Affine3(Translation3(box_center_x, 0.0, 0.0) * Scaling(box_size)).matrix();

        dynamic_ray_caster->add_mesh(cube, trans);
        dynamic_ray_caster->cast(Point(0, 0, 0), Direction(0, 0, 1));

        lagrange::raycasting::project_particles_directional(
            test_origins,
            *cube,
            project_dir,
            test_origins_output,
            test_normals_output,
            parent_transform,
            dynamic_ray_caster.get(),
            false);

        REQUIRE(test_origins_output.size() == 4);
        REQUIRE(test_normals_output.size() == 0);
        for (int i = 0; i < 4; ++i) {
            REQUIRE(fabs(test_origins_output[i][0] - (box_center_x - box_size)) < 1e-6);
            REQUIRE(fabs(test_origins_output[i][1] - test_origins[i][1]) < 1e-6);
            REQUIRE(fabs(test_origins_output[i][2] - test_origins[i][2]) < 1e-6);
        }
    }

    SECTION("non-empty output with normal filled")
    {
        Vector3 project_dir = Vector3::UnitX();
        auto dynamic_ray_caster = lagrange::raycasting::create_ray_caster<Scalar>(
            lagrange::raycasting::EMBREE_ROBUST,
            lagrange::raycasting::BUILD_QUALITY_MEDIUM);

        Scalar box_center_x = 10.0;
        Scalar box_size = sqrt(2.0);
        Matrix4 trans =
            Affine3(
                Translation3(box_center_x, 0.0, 0.0) *
                Rotation3(static_cast<Scalar>(M_PI_4), Vector3::UnitY()) * Scaling(box_size))
                .matrix();

        dynamic_ray_caster->add_mesh(cube, trans);
        dynamic_ray_caster->cast(Point(0, 0, 0), Direction(0, 0, 1));

        lagrange::raycasting::project_particles_directional(
            test_origins,
            *cube,
            project_dir,
            test_origins_output,
            test_normals_output,
            parent_transform,
            dynamic_ray_caster.get());

        REQUIRE(test_origins_output.size() == 4);
        REQUIRE(test_normals_output.size() == 4);

        auto rot_posi_45 =
            Rotation3(static_cast<Scalar>(M_PI_4), -Vector3::UnitY()).toRotationMatrix();
        auto rot_neg_45 = rot_posi_45.transpose();

        ParticleDataType expected_normals = {
            -rot_posi_45 * Vector3::UnitX(),
            -rot_neg_45 * Vector3::UnitX(),
            -rot_posi_45 * Vector3::UnitX(),
            -rot_neg_45 * Vector3::UnitX()};

        for (int i = 0; i < 4; ++i) {
            REQUIRE(fabs(test_origins_output[i][0] - 9.0) < 1e-6);
            REQUIRE(fabs(test_origins_output[i][1] - test_origins[i][1]) < 1e-6);
            REQUIRE(fabs(test_origins_output[i][2] - test_origins[i][2]) < 1e-6);
            // normal of the mesh surface
            REQUIRE((test_normals_output[i] - expected_normals[i]).norm() < 1e-6);
        }
    }
}

TEST_CASE("Projection Speed", "[raycasting][!benchmark]")
{
    const auto cube = lagrange::to_shared_ptr(lagrange::create_cube());
    REQUIRE(cube);

    using MeshType = decltype(cube)::element_type;
    using Scalar = typename MeshType::Scalar;
    using Vector3 = Eigen::Matrix<Scalar, 3, 1>;
    using Matrix4 = Eigen::Matrix<Scalar, 4, 4>;
    using Affine3 = Eigen::Transform<Scalar, 3, Eigen::Affine>;
    using Translation3 = Eigen::Translation<Scalar, 3>;
    using Scaling = Eigen::UniformScaling<Scalar>;
    using RayCasterType = lagrange::raycasting::EmbreeRayCaster<Scalar>;
    using Point = typename RayCasterType::Point;
    using Direction = typename RayCasterType::Direction;

    using ParticleDataType = std::vector<Vector3>;
    const int num_samples_per_dim = 256;
    ParticleDataType test_origins;
    test_origins.reserve(num_samples_per_dim * num_samples_per_dim * 4);

    std::mt19937 rg;
    Scalar dx = 2.0 / static_cast<Scalar>(num_samples_per_dim);

    // stratified random sampling
    for (int i = 0; i < num_samples_per_dim; ++i) {
        for (int j = 0; j < num_samples_per_dim; ++j) {
            Scalar iy = static_cast<Scalar>(i) * dx - 1.0;
            Scalar iz = static_cast<Scalar>(j) * dx - 1.0;

            for (int r = -1; r <= 1; r = r + 2) {
                for (int s = -1; s <= 1; s = s + 2) {
                    Vector3 origin = Vector3::Zero();
                    origin(1) = iy + (static_cast<Scalar>(r) +
                                      static_cast<Scalar>(rg()) / static_cast<Scalar>(rg.max()) *
                                          static_cast<Scalar>(2.0) -
                                      static_cast<Scalar>(1.0)) *
                                         static_cast<Scalar>(0.25) * dx;
                    origin(2) = iz + (static_cast<Scalar>(s) +
                                      static_cast<Scalar>(rg()) / static_cast<Scalar>(rg.max()) *
                                          static_cast<Scalar>(2.0) -
                                      static_cast<Scalar>(1.0)) *
                                         static_cast<Scalar>(0.25) * dx;
                    test_origins.emplace_back(std::move(origin));
                }
            }
        }
    }

    Vector3 project_dir = Vector3::UnitX();
    auto dynamic_ray_caster = lagrange::raycasting::create_ray_caster<Scalar>(
        lagrange::raycasting::EMBREE_ROBUST,
        lagrange::raycasting::BUILD_QUALITY_MEDIUM);

    Scalar box_center_x = 10.0;
    Scalar box_size = 1.5;
    Matrix4 trans = Affine3(Translation3(box_center_x, 0.0, 0.0) * Scaling(box_size)).matrix();
    Matrix4 parent_transform = Matrix4::Identity();

    dynamic_ray_caster->add_mesh(cube, trans);
    dynamic_ray_caster->cast(Point(0, 0, 0), Direction(0, 0, 1));

    ParticleDataType test_origins_output;
    ParticleDataType test_normals_output;

    lagrange::raycasting::project_particles_directional(
        test_origins,
        *cube,
        project_dir,
        test_origins_output,
        test_normals_output,
        parent_transform,
        dynamic_ray_caster.get());

    REQUIRE(test_origins_output.size() == test_origins.size());
    REQUIRE(test_normals_output.size() == test_origins.size());

    BENCHMARK("project particles directional")
    {
        test_origins_output.clear();
        test_normals_output.clear();

        lagrange::raycasting::project_particles_directional(
            test_origins,
            *cube,
            project_dir,
            test_origins_output,
            test_normals_output,
            parent_transform,
            dynamic_ray_caster.get());
    };
}
