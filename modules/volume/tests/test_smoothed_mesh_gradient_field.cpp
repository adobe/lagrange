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
#include <lagrange/primitive/generate_sphere.h>
#include <lagrange/testing/common.h>
#include <lagrange/volume/SmoothedMeshGradientField.h>

#include <Eigen/Core>
#include <Eigen/Geometry>

#include <algorithm>
#include <array>
#include <cmath>
#include <thread>
#include <vector>

namespace {

lagrange::SurfaceMesh<double, uint32_t> make_sheet()
{
    lagrange::SurfaceMesh<double, uint32_t> mesh;
    mesh.add_vertex({-1, -1, 0});
    mesh.add_vertex({1, -1, 0});
    mesh.add_vertex({1, 1, 0});
    mesh.add_vertex({-1, 1, 0});
    mesh.add_triangle(0, 1, 2);
    mesh.add_triangle(0, 2, 3);
    return mesh;
}

lagrange::SurfaceMesh<double, uint32_t> make_box()
{
    lagrange::SurfaceMesh<double, uint32_t> mesh;
    for (const auto& p : std::vector<Eigen::Vector3d>{
             {-1, -1, -1},
             {1, -1, -1},
             {1, 1, -1},
             {-1, 1, -1},
             {-1, -1, 1},
             {1, -1, 1},
             {1, 1, 1},
             {-1, 1, 1}}) {
        mesh.add_vertex({p.x(), p.y(), p.z()});
    }
    for (const auto& f : std::vector<Eigen::Vector3i>{
             {0, 2, 1},
             {0, 3, 2},
             {4, 5, 6},
             {4, 6, 7},
             {0, 1, 5},
             {0, 5, 4},
             {3, 7, 6},
             {3, 6, 2},
             {0, 4, 7},
             {0, 7, 3},
             {1, 2, 6},
             {1, 6, 5}}) {
        mesh.add_triangle(f.x(), f.y(), f.z());
    }
    return mesh;
}

lagrange::SurfaceMesh<double, uint32_t> make_oriented_box(
    const Eigen::Vector3d& normal,
    const Eigen::Vector3d& translation,
    double half_extent)
{
    auto mesh = make_box();
    const Eigen::Vector3d x = normal.normalized();
    const Eigen::Vector3d y = x.cross(Eigen::Vector3d::UnitZ()).normalized();
    const Eigen::Vector3d z = x.cross(y);
    Eigen::Matrix3d rotation;
    rotation.col(0) = x;
    rotation.col(1) = y;
    rotation.col(2) = z;
    for (uint32_t v = 0; v < mesh.get_num_vertices(); ++v) {
        const auto p = mesh.get_position(v);
        const Eigen::Vector3d local(p[0], p[1], p[2]);
        const Eigen::Vector3d world = translation + half_extent * rotation * local;
        auto output = mesh.ref_position(v);
        std::copy(world.data(), world.data() + 3, output.begin());
    }
    return mesh;
}

} // namespace

TEST_CASE("smoothed mesh gradient: accurate at query reach", "[volume][smoothed_gradient]")
{
    auto sheet = make_sheet();
    constexpr double query_reach = 0.2;
    lagrange::volume::SmoothedMeshGradientField field(sheet, 0.05, 0.2, query_reach);
    Eigen::Vector3d direction;
    REQUIRE(field.grid());
    REQUIRE(field.sample_direction({0, 0, query_reach}, direction));
    REQUIRE(direction.dot(Eigen::Vector3d::UnitZ()) > 0.99);
}

TEST_CASE(
    "smoothed mesh gradient: accurate directions at query reach",
    "[volume][smoothed_gradient]")
{
    constexpr double voxel_size = 0.1;
    constexpr double epsilon = 0.2;
    constexpr double query_reach = 0.2;
    constexpr double half_extent = 2.0;
    constexpr int filter_width = 2;
    const double reference_reach =
        query_reach + std::sqrt(3.0) * (4.0 * filter_width + 2.0) * voxel_size;

    const std::array<Eigen::Vector3d, 2> normals = {
        Eigen::Vector3d(1, 2, 3).normalized(),
        Eigen::Vector3d(-2, 3, 1).normalized()};
    const std::array<Eigen::Vector3d, 2> translations = {
        Eigen::Vector3d(0.137, -0.219, 0.083),
        Eigen::Vector3d(-0.173, 0.091, 0.227)};

    for (size_t i = 0; i < normals.size(); ++i) {
        const Eigen::Vector3d& normal = normals[i];
        CAPTURE(i, normal.x(), normal.y(), normal.z());
        REQUIRE(normal.cwiseAbs().minCoeff() > 0.25);
        auto box = make_oriented_box(normal, translations[i], half_extent);
        lagrange::volume::SmoothedMeshGradientField subject(box, voxel_size, epsilon, query_reach);
        lagrange::volume::SmoothedMeshGradientField reference(
            box,
            voxel_size,
            epsilon,
            reference_reach);
        const Eigen::Vector3d face_center = translations[i] + half_extent * normal;

        for (double side : {-1.0, 1.0}) {
            const Eigen::Vector3d position = face_center + side * query_reach * normal;
            Eigen::Vector3d subject_direction;
            Eigen::Vector3d reference_direction;
            CAPTURE(side, position.x(), position.y(), position.z());
            REQUIRE(subject.sample_direction(position, subject_direction));
            REQUIRE(reference.sample_direction(position, reference_direction));
            REQUIRE(std::abs(subject_direction.norm() - 1.0) < 1e-6);
            REQUIRE(std::abs(reference_direction.norm() - 1.0) < 1e-6);
            REQUIRE(subject_direction.dot(reference_direction) > 0.99999);
            REQUIRE(subject_direction.dot(normal) > 0.995);
        }
    }
}

TEST_CASE("smoothed mesh gradient: concurrent sampling", "[volume][smoothed_gradient]")
{
    auto box = make_box();
    lagrange::volume::SmoothedMeshGradientField field(box, 0.05, 0.2, 0.2);
    Eigen::Vector3d expected;
    REQUIRE(field.sample_direction({0, 0, 1.05}, expected));

    constexpr size_t num_threads = 8;
    std::array<bool, num_threads> passed{};
    std::array<std::thread, num_threads> threads;
    for (size_t i = 0; i < num_threads; ++i) {
        threads[i] = std::thread([&, i] {
            Eigen::Vector3d actual;
            passed[i] =
                field.sample_direction({0, 0, 1.05}, actual) && (actual - expected).norm() < 1e-12;
        });
    }
    for (auto& thread : threads) thread.join();
    for (bool result : passed) REQUIRE(result);
}

TEST_CASE("smoothed mesh gradient: flat and sphere orientation", "[volume][smoothed_gradient]")
{
    auto box = make_box();
    lagrange::volume::SmoothedMeshGradientField box_field(box, 0.05, 0.2, 0.2);
    Eigen::Vector3d direction;
    REQUIRE(box_field.sample_direction({0, 0, 1.05}, direction));
    REQUIRE(direction.dot(Eigen::Vector3d::UnitZ()) > 0.99);

    lagrange::primitive::SphereOptions options;
    options.radius = 1;
    options.num_longitude_sections = 48;
    options.num_latitude_sections = 48;
    options.triangulate = true;
    auto sphere = lagrange::primitive::generate_sphere<double, uint32_t>(options);
    lagrange::volume::SmoothedMeshGradientField sphere_field(sphere, 0.04, 0.12, 0.2);
    for (const Eigen::Vector3d p :
         {Eigen::Vector3d(1.05, 0, 0), Eigen::Vector3d(0, 1.05, 0), Eigen::Vector3d(0, 0, 1.05)}) {
        CAPTURE(p.x(), p.y(), p.z());
        REQUIRE(sphere_field.sample_direction(p, direction));
        REQUIRE(direction.dot(p.normalized()) > 0.98);
    }

    const Eigen::Vector3d interior_point(0.95, 0, 0);
    REQUIRE(sphere_field.sample_direction(interior_point, direction));
    REQUIRE(direction.dot(Eigen::Vector3d::UnitX()) > 0.98);
}

TEST_CASE("smoothed mesh gradient: feature rounding follows epsilon", "[volume][smoothed_gradient]")
{
    auto box = make_box();
    lagrange::volume::SmoothedMeshGradientField narrow(box, 0.03, 0.09, 0.2);
    lagrange::volume::SmoothedMeshGradientField wide(box, 0.03, 0.3, 0.2);
    const Eigen::Vector3d p(1.06, 0, 0.82);
    Eigen::Vector3d narrow_direction;
    Eigen::Vector3d wide_direction;
    REQUIRE(narrow.sample_direction(p, narrow_direction));
    REQUIRE(wide.sample_direction(p, wide_direction));
    REQUIRE(wide_direction.z() > narrow_direction.z() + 0.08);
}

TEST_CASE(
    "smoothed mesh gradient: sharp feature continuity and band",
    "[volume][smoothed_gradient]")
{
    auto box = make_box();
    lagrange::volume::SmoothedMeshGradientField field(box, 0.04, 0.24, 0.2);

    std::vector<Eigen::Vector3d> directions;
    for (int i = 0; i <= 20; ++i) {
        const double t = static_cast<double>(i) / 20.0;
        const Eigen::Vector3d p(1.08 - 0.18 * t, 0, 0.9 + 0.18 * t);
        Eigen::Vector3d direction;
        CAPTURE(i, p.x(), p.z());
        REQUIRE(field.sample_direction(p, direction));
        directions.push_back(direction);
    }
    REQUIRE(directions.front().dot(Eigen::Vector3d::UnitX()) > 0.85);
    REQUIRE(directions.back().dot(Eigen::Vector3d::UnitZ()) > 0.85);
    for (size_t i = 1; i < directions.size(); ++i) {
        REQUIRE(directions[i - 1].dot(directions[i]) > 0.97);
    }

    Eigen::Vector3d direction;
    REQUIRE_FALSE(field.sample_direction({4, 0, 0}, direction));

    openvdb::Coord edge;
    bool found_edge = false;
    for (auto iter = field.grid()->cbeginValueOn(); iter; ++iter) {
        if (iter.getValue().lengthSqr() <= 1e-8f) continue;
        if (!found_edge || iter.getCoord().x() > edge.x()) {
            edge = iter.getCoord();
            found_edge = true;
        }
    }
    REQUIRE(found_edge);
    const openvdb::Vec3d edge_index(edge.x() + 0.75, edge.y(), edge.z());
    const openvdb::Vec3d edge_world = field.grid()->indexToWorld(edge_index);
    REQUIRE_FALSE(
        field.sample_direction({edge_world.x(), edge_world.y(), edge_world.z()}, direction));
}
