/*
 * Copyright 2017 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/Mesh.h>
#include <lagrange/common.h>
#include <lagrange/create_mesh.h>
#include <lagrange/raycasting/create_ray_caster.h>
#include <lagrange/utils/range.h>

#include <lagrange/testing/common.h>
#include <catch2/catch_approx.hpp>
#include <Eigen/Core>
#include <Eigen/Geometry>

TEST_CASE("EmbreeDynamicRayCasterTest", "[ray_caster][dynamic][embree][triangle_mesh]")
{
    const auto cube = lagrange::to_shared_ptr(lagrange::create_cube());
    REQUIRE(cube);
    const auto& vertices = cube->get_vertices();
    const auto& facets = cube->get_facets();

    using MeshType = decltype(cube)::element_type;
    using Scalar = typename MeshType::Scalar;
    using Index = size_t;
    using Vector3 = typename MeshType::VertexType;

    auto ray_caster =
        lagrange::raycasting::create_ray_caster<Scalar>(lagrange::raycasting::EMBREE_DYNAMIC);


    REQUIRE(ray_caster);

    auto mesh_id = ray_caster->add_mesh(cube, Eigen::Matrix<Scalar, 4, 4>::Identity());

    {
        auto mesh_read_back = ray_caster->get_mesh<MeshType>(mesh_id);
        REQUIRE(mesh_read_back == cube);
    }

    const int order = 20;
    Index success_count = 0;
    Index num_rays = 0;
    const Vector3 axis(1, 1, 1);
    for (Index k = 0; k <= order; k++) {
        const Scalar angle = Scalar(k) / Scalar(order) * 2 * M_PI;

        Eigen::Affine3d t(Eigen::AngleAxisd(angle, axis));

        /*Eigen::Matrix<Scalar, 4, 4> transformation;
        transformation.setIdentity();
        transformation.block(0, 0, 3, 3) =
            Eigen::AngleAxis<Scalar>(angle, axis).toRotationMatrix();*/
        ray_caster->update_transformation(mesh_id, 0, t.matrix().template cast<Scalar>());
        for (Index i = 0; i <= order; i++) {
            const Scalar theta = Scalar(i) / Scalar(order) * 2 * M_PI;
            for (Index j = 0; j <= order; j++) {
                const Scalar phi = Scalar(j) / Scalar(order) * M_PI - M_PI * 0.5;
                const Vector3 from(0.0, 0.0, 0.0);
                const Vector3 dir({cos(phi) * cos(theta), cos(phi) * sin(theta), sin(phi)});

                size_t mesh_index = std::numeric_limits<size_t>::max();
                size_t instance_index = std::numeric_limits<size_t>::max();
                size_t facet_index = std::numeric_limits<size_t>::max();
                Scalar ray_depth = 0.0;
                Eigen::Matrix<Scalar, 3, 1> bc; // barycentric coordinates
                Eigen::Matrix<Scalar, 3, 1> normal;

                bool hit = ray_caster->cast(
                    from,
                    dir,
                    mesh_index,
                    instance_index,
                    facet_index,
                    ray_depth,
                    bc,
                    normal);
                if (hit) {
                    const Vector3 p = from + dir * ray_depth;
                    const auto f = facets.row(facet_index);
                    const auto v0 = vertices.row(f[0]);
                    const auto v1 = vertices.row(f[1]);
                    const auto v2 = vertices.row(f[2]);

                    const Vector3 q = bc[0] * v0 + bc[1] * v1 + bc[2] * v2;
                    const Vector3 qt = t * q.transpose();

                    REQUIRE((p - qt).norm() == Catch::Approx(0.0).margin(1e-5));
                    success_count++;
                }
                num_rays++;
            }
        }
    }

    lagrange::logger().info("Embree (dynamic scene) ray hits: {}/{}", success_count, num_rays);
    REQUIRE(success_count > num_rays * 0.9);
}


TEST_CASE("EmbreeDynamicRayCaster4PackedTest", "[ray_caster][dynamic][embree][triangle_mesh]")
{
    const auto cube = lagrange::to_shared_ptr(lagrange::create_cube());
    REQUIRE(cube);
    const auto& vertices = cube->get_vertices();
    const auto& facets = cube->get_facets();

    using MeshType = decltype(cube)::element_type;
    using Scalar = typename MeshType::Scalar;
    using Vector3 = typename MeshType::VertexType;
    using RayCasterPtr = std::unique_ptr<lagrange::raycasting::EmbreeRayCaster<Scalar>>;
    using RayCaster = typename RayCasterPtr::element_type;
    using Index = RayCaster::Index;
    using Scalar4 = typename RayCaster::Scalar4;
    using Index4 = typename RayCaster::Index4;
    using Point4 = typename RayCaster::Point4;
    using Direction4 = typename RayCaster::Direction4;
    using Mask4 = typename RayCaster::Mask4;

    auto ray_caster =
        lagrange::raycasting::create_ray_caster<Scalar>(lagrange::raycasting::EMBREE_DYNAMIC);


    REQUIRE(ray_caster);

    auto mesh_id = ray_caster->add_mesh(cube, Eigen::Matrix<Scalar, 4, 4>::Identity());

    {
        auto mesh_read_back = ray_caster->get_mesh<MeshType>(mesh_id);
        REQUIRE(mesh_read_back == cube);
    }

    const int order = 20;
    Index success_count = 0;
    Index num_rays = 0;
    const Vector3 axis(1, 1, 1);

    Point4 from;
    from.setZero();

    auto bitcount4 = [](uint32_t i) {
        i = i - ((i >> 1) & 0x5);
        return (i & 0x3) + ((i >> 2) & 0x3);
    };

    for (int k = 0; k <= order; k++) {
        const Scalar angle = Scalar(k) / Scalar(order) * 2 * M_PI;

        Eigen::Affine3d t(Eigen::AngleAxisd(angle, axis));

        ray_caster->update_transformation(mesh_id, 0, t.matrix().template cast<Scalar>());
        for (int i = 0; i <= order; i++) {
            const Scalar theta = Scalar(i) / Scalar(order) * 2 * M_PI;
            Mask4 mask = Mask4(-1, -1, -1, -1);

            for (int j = 0; j <= order; j += 4) {
                int batchsize = std::min(order + 1 - j, 4);
                Direction4 dir;

                for (int b = 0; b < batchsize; ++b) {
                    const Scalar phi = Scalar(j + b) / Scalar(order) * M_PI - M_PI * 0.5;
                    dir(b, 0) = cos(phi) * cos(theta);
                    dir(b, 1) = cos(phi) * sin(theta);
                    dir(b, 2) = sin(phi);
                }

                for (int b = batchsize; b < 4; ++b) {
                    mask(b) = 0;
                }

                Index4 mesh_index = Index4::Constant(std::numeric_limits<Index>::max());
                Index4 instance_index = Index4::Constant(std::numeric_limits<Index>::max());
                Index4 facet_index = Index4::Constant(std::numeric_limits<Index>::max());
                Scalar4 ray_depth = Scalar4::Zero();
                Point4 bc;
                Point4 normal;

                uint32_t hit4 = ray_caster->cast4(
                    batchsize,
                    from,
                    dir,
                    mask,
                    mesh_index,
                    instance_index,
                    facet_index,
                    ray_depth,
                    bc,
                    normal);

                if (hit4) {
                    REQUIRE(bitcount4(hit4) >= static_cast<uint32_t>(batchsize));
                    for (int b = 0; b < batchsize; ++b) {
                        if (hit4 & (1 << b)) {
                            const Vector3 p =
                                from.row(b).transpose() + dir.row(b).transpose() * ray_depth[b];
                            const auto f = facets.row(facet_index[b]);
                            const auto v0 = vertices.row(f[0]);
                            const auto v1 = vertices.row(f[1]);
                            const auto v2 = vertices.row(f[2]);

                            const Vector3 q = bc(b, 0) * v0 + bc(b, 1) * v1 + bc(b, 2) * v2;
                            const Vector3 qt = t * q.transpose();

                            REQUIRE((p - qt).norm() == Catch::Approx(0.0).margin(1e-5));
                            success_count++;
                        }
                    }
                }
                num_rays += batchsize;
            }
        }
    }

    lagrange::logger().info("Embree (dynamic scene) ray hits: {}/{}", success_count, num_rays);
    REQUIRE(success_count > num_rays * 0.9);
}

TEST_CASE(
    "EmbreeDynamicRayCaster4PackedTest_multiple_meshes",
    "[ray_caster][dynamic][embree][triangle_mesh]")
{
    const auto cube = lagrange::to_shared_ptr(lagrange::create_cube());
    REQUIRE(cube);
    const auto& vertices = cube->get_vertices();
    const auto& facets = cube->get_facets();

    using MeshType = decltype(cube)::element_type;
    using Scalar = typename MeshType::Scalar;
    using Vector3 = typename MeshType::VertexType;
    using RayCasterPtr = std::unique_ptr<lagrange::raycasting::EmbreeRayCaster<Scalar>>;
    using RayCaster = typename RayCasterPtr::element_type;
    using Index = RayCaster::Index;
    using Scalar4 = typename RayCaster::Scalar4;
    using Index4 = typename RayCaster::Index4;
    using Point4 = typename RayCaster::Point4;
    using Direction4 = typename RayCaster::Direction4;
    using Mask4 = typename RayCaster::Mask4;

    auto dynamic_ray_caster =
        lagrange::raycasting::create_ray_caster<Scalar>(lagrange::raycasting::EMBREE_DYNAMIC);
    REQUIRE(dynamic_ray_caster);

    const int K = 10;
    for (int i = 0; i < K; i++) {
        const auto mesh_id =
            dynamic_ray_caster->add_mesh(cube, Eigen::Matrix<Scalar, 4, 4>::Identity());
        REQUIRE(i == static_cast<int>(mesh_id));
    }

    auto explode_pack4 = [&vertices, &facets, &dynamic_ray_caster](
                             const int order,
                             const std::vector<Eigen::Affine3d>& trans,
                             size_t& num_rays,
                             size_t& success_count) {
        Point4 from;
        from.setZero();

        for (int i = 0; i <= order; i++) {
            const Scalar theta = Scalar(i) / Scalar(order) * 2 * M_PI;
            Mask4 mask = Mask4(-1, -1, -1, -1);

            for (int j = 0; j <= order; j += 4) {
                int batchsize = std::min(order + 1 - j, 4);
                Direction4 dir;

                for (int b = 0; b < batchsize; ++b) {
                    const Scalar phi = Scalar(j + b) / Scalar(order) * M_PI - M_PI * 0.5;
                    dir(b, 0) = cos(phi) * cos(theta);
                    dir(b, 1) = cos(phi) * sin(theta);
                    dir(b, 2) = sin(phi);
                }

                for (int b = batchsize; b < 4; ++b) {
                    mask(b) = 0;
                }

                Index4 mesh_index = Index4::Constant(std::numeric_limits<Index>::max());
                Index4 instance_index = Index4::Constant(std::numeric_limits<Index>::max());
                Index4 facet_index = Index4::Constant(std::numeric_limits<Index>::max());
                Scalar4 ray_depth = Scalar4::Zero();
                Point4 bc;
                Point4 normal;

                uint32_t hit4 = dynamic_ray_caster->cast4(
                    batchsize,
                    from,
                    dir,
                    mask,
                    mesh_index,
                    instance_index,
                    facet_index,
                    ray_depth,
                    bc,
                    normal);

                if (hit4) {
                    for (int b = 0; b < batchsize; ++b) {
                        if (hit4 & (1 << b)) {
                            const Vector3 p =
                                from.row(b).transpose() + dir.row(b).transpose() * ray_depth[b];
                            auto f = facets.row(facet_index[b]);
                            auto v0 = vertices.row(f[0]);
                            auto v1 = vertices.row(f[1]);
                            auto v2 = vertices.row(f[2]);

                            const Vector3 q = bc(b, 0) * v0 + bc(b, 1) * v1 + bc(b, 2) * v2;
                            const Vector3 qt = trans[mesh_index[b]] * q.transpose();

                            REQUIRE((p - qt).norm() == Catch::Approx(0.0).margin(1e-5));
                            success_count++;
                        }
                    }
                }
                num_rays += batchsize;
            }
        }
    };

    auto explode = [&vertices, &facets, &dynamic_ray_caster](
                       const size_t order,
                       const std::vector<Eigen::Affine3d>& trans,
                       size_t& num_rays,
                       size_t& success_count) {
        for (size_t i = 0; i <= order; i++) {
            const Scalar theta = Scalar(i) / Scalar(order) * 2 * M_PI;
            for (size_t j = 0; j <= order; j++) {
                const Scalar phi = Scalar(j) / Scalar(order) * M_PI - M_PI * 0.5;
                const Vector3 from(0.0, 0.0, 0.0);
                const Vector3 dir({cos(phi) * cos(theta), cos(phi) * sin(theta), sin(phi)});

                size_t mesh_index = std::numeric_limits<size_t>::max();
                size_t instance_index = std::numeric_limits<size_t>::max();
                size_t facet_index = std::numeric_limits<size_t>::max();
                Scalar ray_depth = 0.0;
                Eigen::Matrix<Scalar, 3, 1> bc; // barycentric coordinates
                Eigen::Matrix<Scalar, 3, 1> normal;

                bool hit = dynamic_ray_caster->cast(
                    from,
                    dir,
                    mesh_index,
                    instance_index,
                    facet_index,
                    ray_depth,
                    bc,
                    normal);
                if (hit) {
                    const Vector3 p = from + dir * ray_depth;
                    auto f = facets.row(facet_index);
                    auto v0 = vertices.row(f[0]);
                    auto v1 = vertices.row(f[1]);
                    auto v2 = vertices.row(f[2]);

                    const Vector3 q = bc[0] * v0 + bc[1] * v1 + bc[2] * v2;
                    const Vector3 qt = trans[mesh_index] * q.transpose();

                    REQUIRE((p - qt).norm() == Catch::Approx(0.0).margin(1e-5));
                    success_count++;
                }
                num_rays++;
            }
        }
    };

    const int order = 20;
    size_t dynamic_success_count_pack4 = 0;
    size_t dynamic_num_rays_pack4 = 0;
    size_t dynamic_success_count = 0;
    size_t dynamic_num_rays = 0;
    for (size_t i = 0; i <= order; i++) {
        std::vector<Eigen::Affine3d> trans(K);
        for (auto k : lagrange::range(K)) {
            trans[k] = Eigen::Affine3d(Eigen::Translation3d(
                (k - K * 0.5) * 2 * (i + 1),
                k * 0.5 * (i + 1),
                (k - K * 0.5) * 5 * (i + 1)));
            dynamic_ray_caster->update_transformation(k, 0, trans[k].matrix());
        }
        explode(order, trans, dynamic_num_rays, dynamic_success_count);
        explode_pack4(order, trans, dynamic_num_rays_pack4, dynamic_success_count_pack4);

        REQUIRE(dynamic_num_rays == dynamic_num_rays_pack4);
        REQUIRE(dynamic_success_count == dynamic_success_count_pack4);
    }

    lagrange::logger().info(
        "Embree (dynamic scene) ray hits (single ray): {}/{}",
        dynamic_success_count,
        dynamic_num_rays);
    lagrange::logger().info(
        "Embree (dynamic scene) ray hits (packed-4 rays): {}/{}",
        dynamic_success_count_pack4,
        dynamic_num_rays_pack4);
}

TEST_CASE(
    "EmbreeDynamicRayCasterTest_multiple_meshes",
    "[ray_caster][dynamic][embree][triangle_mesh]")
{
    const auto cube = lagrange::to_shared_ptr(lagrange::create_cube());
    REQUIRE(cube);
    const auto& vertices = cube->get_vertices();
    const auto& facets = cube->get_facets();

    using MeshType = decltype(cube)::element_type;
    using Scalar = typename MeshType::Scalar;
    using Vector3 = typename MeshType::VertexType;

    auto dynamic_ray_caster =
        lagrange::raycasting::create_ray_caster<Scalar>(lagrange::raycasting::EMBREE_DYNAMIC);
    REQUIRE(dynamic_ray_caster);

    const size_t K = 10;
    for (size_t i = 0; i < K; i++) {
        const auto mesh_id =
            dynamic_ray_caster->add_mesh(cube, Eigen::Matrix<Scalar, 4, 4>::Identity());
        REQUIRE(i == mesh_id);
    }

    auto explode = [&vertices, &facets, &dynamic_ray_caster](
                       const size_t order,
                       const std::vector<Eigen::Affine3d>& trans,
                       size_t& num_rays,
                       size_t& success_count) {
        for (size_t i = 0; i <= order; i++) {
            const Scalar theta = Scalar(i) / Scalar(order) * 2 * M_PI;
            for (size_t j = 0; j <= order; j++) {
                const Scalar phi = Scalar(j) / Scalar(order) * M_PI - M_PI * 0.5;
                const Vector3 from(0.0, 0.0, 0.0);
                const Vector3 dir({cos(phi) * cos(theta), cos(phi) * sin(theta), sin(phi)});

                size_t mesh_index = std::numeric_limits<size_t>::max();
                size_t instance_index = std::numeric_limits<size_t>::max();
                size_t facet_index = std::numeric_limits<size_t>::max();
                Scalar ray_depth = 0.0;
                Eigen::Matrix<Scalar, 3, 1> bc; // barycentric coordinates
                Eigen::Matrix<Scalar, 3, 1> normal;

                bool hit = dynamic_ray_caster->cast(
                    from,
                    dir,
                    mesh_index,
                    instance_index,
                    facet_index,
                    ray_depth,
                    bc,
                    normal);
                if (hit) {
                    const Vector3 p = from + dir * ray_depth;
                    auto f = facets.row(facet_index);
                    auto v0 = vertices.row(f[0]);
                    auto v1 = vertices.row(f[1]);
                    auto v2 = vertices.row(f[2]);

                    const Vector3 q = bc[0] * v0 + bc[1] * v1 + bc[2] * v2;
                    const Vector3 qt = trans[mesh_index] * q.transpose();

                    REQUIRE((p - qt).norm() == Catch::Approx(0.0).margin(1e-5));
                    success_count++;
                }
                num_rays++;
            }
        }
    };

    const int order = 20;
    size_t dynamic_success_count = 0;
    size_t dynamic_num_rays = 0;
    for (size_t i = 0; i <= order; i++) {
        std::vector<Eigen::Affine3d> trans(K);
        for (auto k : lagrange::range(K)) {
            trans[k] = Eigen::Affine3d(Eigen::Translation3d(
                (k - K * 0.5) * 2 * (i + 1),
                k * 0.5 * (i + 1),
                (k - K * 0.5) * 5 * (i + 1)));
            dynamic_ray_caster->update_transformation(k, 0, trans[k].matrix());
        }
        explode(order, trans, dynamic_num_rays, dynamic_success_count);
    }

    lagrange::logger().info(
        "Embree (dynamic scene) ray hits: {}/{}",
        dynamic_success_count,
        dynamic_num_rays);
}

TEST_CASE(
    "EmbreeDynamicRayCasterTest_multiple_instances",
    "[ray_caster][dynamic][embree][triangle_mesh]")
{
    const auto cube = lagrange::to_shared_ptr(lagrange::create_cube());
    REQUIRE(cube);
    const auto& vertices = cube->get_vertices();
    const auto& facets = cube->get_facets();

    using MeshType = decltype(cube)::element_type;
    using Scalar = typename MeshType::Scalar;
    using Vector3 = typename MeshType::VertexType;

    auto dynamic_ray_caster =
        lagrange::raycasting::create_ray_caster<Scalar>(lagrange::raycasting::EMBREE_DYNAMIC);
    REQUIRE(dynamic_ray_caster);

    const size_t K = 10;
    dynamic_ray_caster->add_meshes(
        cube,
        std::vector<Eigen::Matrix<Scalar, 4, 4>>(K, Eigen::Matrix<Scalar, 4, 4>::Identity()));

    auto explode = [&vertices, &facets, &dynamic_ray_caster](
                       const size_t order,
                       const std::vector<Eigen::Affine3d>& trans,
                       size_t& num_rays,
                       size_t& success_count) {
        for (size_t i = 0; i <= order; i++) {
            const Scalar theta = Scalar(i) / Scalar(order) * 2 * M_PI;
            for (size_t j = 0; j <= order; j++) {
                const Scalar phi = Scalar(j) / Scalar(order) * M_PI - M_PI * 0.5;
                const Vector3 from(0.0, 0.0, 0.0);
                const Vector3 dir({cos(phi) * cos(theta), cos(phi) * sin(theta), sin(phi)});

                size_t mesh_index = std::numeric_limits<size_t>::max();
                size_t instance_index = std::numeric_limits<size_t>::max();
                size_t facet_index = std::numeric_limits<size_t>::max();
                Scalar ray_depth = 0.0;
                Eigen::Matrix<Scalar, 3, 1> bc; // barycentric coordinates
                Eigen::Matrix<Scalar, 3, 1> normal;

                bool hit = dynamic_ray_caster->cast(
                    from,
                    dir,
                    mesh_index,
                    instance_index,
                    facet_index,
                    ray_depth,
                    bc,
                    normal);
                if (hit) {
                    const Vector3 p = from + dir * ray_depth;
                    auto f = facets.row(facet_index);
                    auto v0 = vertices.row(f[0]);
                    auto v1 = vertices.row(f[1]);
                    auto v2 = vertices.row(f[2]);

                    const Vector3 q = bc[0] * v0 + bc[1] * v1 + bc[2] * v2;
                    const Vector3 qt = trans[instance_index] * q.transpose();

                    REQUIRE((p - qt).norm() == Catch::Approx(0.0).margin(1e-5));
                    success_count++;
                }
                num_rays++;
            }
        }
    };

    const int order = 20;
    size_t dynamic_success_count = 0;
    size_t dynamic_num_rays = 0;
    for (size_t i = 0; i <= order; i++) {
        std::vector<Eigen::Affine3d> trans(K);
        for (auto k : lagrange::range(K)) {
            trans[k] = Eigen::Affine3d(Eigen::Translation3d(
                (k - K * 0.5) * 2 * (i + 1),
                k * 0.5 * (i + 1),
                (k - K * 0.5) * 5 * (i + 1)));
            dynamic_ray_caster->update_transformation(0, k, trans[k].matrix());
        }
        explode(order, trans, dynamic_num_rays, dynamic_success_count);
    }

    lagrange::logger().info(
        "Embree (dynamic scene) ray hits: {}/{}",
        dynamic_success_count,
        dynamic_num_rays);
}

TEST_CASE("EmbreeDynamicRayCaster_visibility", "[embree][ray_caster][dynamic][visibility]")
{
    auto cube = lagrange::to_shared_ptr(lagrange::create_cube());
    REQUIRE(cube);
    const auto& facets = cube->get_facets();

    using MeshType = decltype(cube)::element_type;
    using Scalar = typename MeshType::Scalar;
    using Vector3 = typename MeshType::VertexType;

    auto ray_caster =
        lagrange::raycasting::create_ray_caster<Scalar>(lagrange::raycasting::EMBREE_DYNAMIC);
    REQUIRE(ray_caster);

    const auto mesh_id_1 = ray_caster->add_mesh(cube, Eigen::Matrix<Scalar, 4, 4>::Identity());
    const auto mesh_id_2 = ray_caster->add_mesh(cube, Eigen::Matrix<Scalar, 4, 4>::Identity());

    const Vector3 from(0.0, 0.0, 0.0);
    const Vector3 dir(1.0, 0.0, 0.0);

    size_t mesh_index = std::numeric_limits<int>::max();
    size_t instance_index = std::numeric_limits<int>::max();
    size_t facet_index = std::numeric_limits<int>::max();
    Scalar ray_depth = 0.0;
    Eigen::Matrix<Scalar, 3, 1> bc;
    Eigen::Matrix<Scalar, 3, 1> normal;

    bool hit = false;

    hit =
        ray_caster->cast(from, dir, mesh_index, instance_index, facet_index, ray_depth, bc, normal);

    REQUIRE(hit);
    REQUIRE(facet_index < lagrange::safe_cast<size_t>(facets.rows()));
    REQUIRE(ray_depth == Catch::Approx(1.0));

    ray_caster->update_visibility(mesh_id_1, 0, false);
    mesh_index = std::numeric_limits<size_t>::max();
    instance_index = std::numeric_limits<size_t>::max();
    facet_index = std::numeric_limits<size_t>::max();
    ray_depth = 0.0;
    hit =
        ray_caster->cast(from, dir, mesh_index, instance_index, facet_index, ray_depth, bc, normal);
    REQUIRE(hit);
    REQUIRE(mesh_index == 1);
    REQUIRE(facet_index < lagrange::safe_cast<size_t>(facets.rows()));
    REQUIRE(ray_depth == Catch::Approx(1.0));

    ray_caster->update_visibility(mesh_id_2, 0, false);
    mesh_index = std::numeric_limits<size_t>::max();
    instance_index = std::numeric_limits<size_t>::max();
    facet_index = std::numeric_limits<size_t>::max();
    ray_depth = 0.0;
    hit =
        ray_caster->cast(from, dir, mesh_index, instance_index, facet_index, ray_depth, bc, normal);
    REQUIRE(!hit);
    REQUIRE(mesh_index == lagrange::invalid<size_t>());
    REQUIRE(facet_index == lagrange::invalid<size_t>());
}

TEST_CASE("EmbreeDynamicRayCaster_updates", "[embree][ray_caster][dynamic][updates]")
{
    auto cube = lagrange::to_shared_ptr(lagrange::create_cube());
    REQUIRE(cube);

    using MeshType = decltype(cube)::element_type;
    using Scalar = typename MeshType::Scalar;
    using Vector3 = typename MeshType::VertexType;
    using VertexArray = typename MeshType::VertexArray;

    auto dynamic_ray_caster =
        lagrange::raycasting::create_ray_caster<Scalar>(lagrange::raycasting::EMBREE_DYNAMIC);
    REQUIRE(dynamic_ray_caster);

    dynamic_ray_caster->add_mesh(cube, Eigen::Matrix<Scalar, 4, 4>::Identity());

    // Functor to shoot a grid of rays in [-2, 2]^2 parallel to the Z axis through the cube.
    size_t num_rays, success_count;
    auto raygrid = [&dynamic_ray_caster, &num_rays, &success_count]() {
        const Scalar EYE_DIST = 100.0;
        const int NUM_STEPS = 100;
        Vector3 from(0.0, 0.0, EYE_DIST);
        const Vector3 dir(0.0, 0.0, -1.0);
        num_rays = 0;
        success_count = 0;
        for (int i = 0; i < NUM_STEPS; ++i) {
            from[0] = 4 * (i + 0.5) / (Scalar)NUM_STEPS - 2;
            for (int j = 0; j < NUM_STEPS; ++j) {
                from[1] = 4 * (j + 0.5) / (Scalar)NUM_STEPS - 2;

                if (dynamic_ray_caster->cast(from, dir)) {
                    success_count++;
                }
                num_rays++;
            }
        }
    };

    // Get the success rate for the original cube orientation (should be ~25%)
    {
        raygrid();
        lagrange::logger().info(
            "Embree (dynamic scene) ray hits (original orientation): {}/{}",
            success_count,
            num_rays);
        double success_rate = (double)success_count / num_rays;
        REQUIRE(success_rate == Catch::Approx(0.25).epsilon(0.01));
    }

    // Get the success rate after rotating the cube by 90% around the Y-axis (should be ~25%)
    {
        const Eigen::Matrix<Scalar, 3, 3> rot =
            Eigen::AngleAxisd(0.5 * M_PI, Eigen::Vector3d(0, 1, 0).normalized())
                .toRotationMatrix()
                .cast<Scalar>();
        const auto& old_verts = cube->get_vertices();
        VertexArray new_verts(old_verts.rows(), old_verts.cols());
        for (Eigen::Index i = 0; i < new_verts.rows(); ++i) {
            new_verts.row(i) = old_verts.row(i) * rot.transpose();
        }
        cube->import_vertices(new_verts);
        dynamic_ray_caster->update_mesh_vertices(0);
        raygrid();
        lagrange::logger().info(
            "Embree (dynamic scene) ray hits (rotated 90 degrees around Y axis): {}/{}",
            success_count,
            num_rays);
        double success_rate = (double)success_count / num_rays;
        REQUIRE(success_rate == Catch::Approx(0.25).epsilon(0.01));
    }

    // Get the success rate after rotating the cube by an addition 45% around the Y-axis (should be
    // ~sqrt(2) * 25%)
    {
        const Eigen::Matrix<Scalar, 3, 3> rot =
            Eigen::AngleAxisd(0.25 * M_PI, Eigen::Vector3d(0, 1, 0).normalized())
                .toRotationMatrix()
                .cast<Scalar>();
        const auto& old_verts = cube->get_vertices();
        VertexArray new_verts(old_verts.rows(), old_verts.cols());
        for (Eigen::Index i = 0; i < new_verts.rows(); ++i) {
            new_verts.row(i) = old_verts.row(i) * rot.transpose();
        }
        cube->import_vertices(new_verts);
        dynamic_ray_caster->update_mesh_vertices(0);
        raygrid();
        lagrange::logger().info(
            "Embree (dynamic scene) ray hits (rotated 45 more degrees around Y axis): {}/{}",
            success_count,
            num_rays);
        double success_rate = (double)success_count / num_rays;
        REQUIRE(success_rate == Catch::Approx(std::sqrt(2.0) * 0.25).epsilon(0.025));
    }
}

TEST_CASE("EmbreeDefaultRayCaster_filters", "[embree][ray_caster][default][filters]")
{
    auto cube = lagrange::to_shared_ptr(lagrange::create_cube());
    REQUIRE(cube);

    using MeshType = decltype(cube)::element_type;
    using Scalar = typename MeshType::Scalar;
    using Vector3 = typename MeshType::VertexType;
    using RayCaster = lagrange::raycasting::EmbreeRayCaster<Scalar>;
    using Index = typename RayCaster::Index;
    using Point4 = typename RayCaster::Point4;
    using Direction4 = typename RayCaster::Direction4;
    using Mask4 = typename RayCaster::Mask4;

    auto ray_caster =
        lagrange::raycasting::create_ray_caster<Scalar>(lagrange::raycasting::EMBREE_DEFAULT);
    REQUIRE(ray_caster);

    ray_caster->add_mesh(cube, Eigen::Matrix<Scalar, 4, 4>::Identity());

    const Scalar EYE_DIST = 100.0;
    double hit_count = 0;
    auto filter = [&hit_count, &ray_caster, EYE_DIST](
                      const RayCaster* obj,
                      const Index* mesh_index,
                      const Index* instance_index,
                      const RTCFilterFunctionNArguments* args) {
        for (unsigned int i = 0; i < args->N; ++i) {
            if (args->valid[i] == 0) {
                continue;
            }

            REQUIRE(obj == ray_caster.get());
            REQUIRE(mesh_index[i] == 0);
            REQUIRE(instance_index[i] == 0);

            float tfar = RTCRayN_tfar(args->ray, args->N, i);
            REQUIRE((tfar == Catch::Approx(EYE_DIST - 1) || tfar == Catch::Approx(EYE_DIST + 1)));

            hit_count += 1;
            args->valid[i] = 0; // reject the hit so that we get all hits
        }
    };

    ray_caster->set_intersection_filter(0, filter);
    ray_caster->set_occlusion_filter(0, filter);

    // Single ray tests.
    // Shoot a grid of rays parallel to the Z axis through the cube.
    const int NUM_STEPS = 100;
    Vector3 from(0.0, 0.0, EYE_DIST);
    const Vector3 dir(0.0, 0.0, -1.0);
    size_t num_rays = 0;
    hit_count = 0;
    for (int i = 0; i < NUM_STEPS; ++i) {
        from[0] = 2 * (i + 0.5) / (Scalar)NUM_STEPS - 1;
        for (int j = 0; j < NUM_STEPS; ++j) {
            from[1] = 2 * (j + 0.5) / (Scalar)NUM_STEPS - 1;

            ray_caster->cast(from, dir);
            num_rays++;
        }
    }

    double avg_hit_count = hit_count / (double)num_rays;
    lagrange::logger().info(
        "Embree (default scene) average hits per ray traversal (single rays): {} ({} rays)",
        avg_hit_count,
        num_rays);

    // We expect mostly 2 hits per ray (front and back of cube), but rays through the diagonal can
    // get 4 hits if the cube is triangulated.
    const double MAX_AVG_HITS =
        ((NUM_STEPS * (NUM_STEPS - 1)) * 2 + NUM_STEPS * 4) / (double)(NUM_STEPS * NUM_STEPS);
    REQUIRE(avg_hit_count >= Catch::Approx(2));
    REQUIRE(avg_hit_count <= Catch::Approx(MAX_AVG_HITS));

    // Ray packet tests.
    // Shoot a grid of rays parallel to the Z axis through the cube.
    num_rays = 0;
    hit_count = 0;
    Point4 from4;
    from4.col(2).fill(EYE_DIST);
    Direction4 dir4 = Direction4::Zero();
    dir4.col(2).fill(-1.0);
    const Mask4 mask4 = Mask4(-1, -1, -1, -1);
    for (int i = 0; i < NUM_STEPS; ++i) {
        Scalar base_x = (i + 0.5) / (Scalar)NUM_STEPS;
        for (int j = 0; j < NUM_STEPS; ++j) {
            Scalar base_y = (j + 0.5) / (Scalar)NUM_STEPS;
            int ray_id = 0;
            for (int k = -1; k <= 1; k += 2)
                for (int h = -1; h <= 1; h += 2, ++ray_id) {
                    from4(ray_id, 0) = 2 * (base_x + k * 0.25 / (Scalar)NUM_STEPS) - 1;
                    from4(ray_id, 1) = 2 * (base_y + h * 0.25 / (Scalar)NUM_STEPS) - 1;
                }

            ray_caster->cast4(4, from4, dir4, mask4);
            num_rays += 4;
        }
    }

    avg_hit_count = hit_count / (double)num_rays;
    lagrange::logger().info(
        "Embree (default scene) average hits per ray traversal (packed-4 rays): {} ({} rays)",
        avg_hit_count,
        num_rays);

    const int NUM_STEPSx2 = 2 * NUM_STEPS;
    const double MAX_AVG_HITS4 = ((NUM_STEPSx2 * (NUM_STEPSx2 - 1)) * 2 + NUM_STEPSx2 * 4) /
                                 (double)(NUM_STEPSx2 * NUM_STEPSx2);
    REQUIRE(avg_hit_count >= Catch::Approx(2));
    REQUIRE(avg_hit_count <= Catch::Approx(MAX_AVG_HITS4));
}
