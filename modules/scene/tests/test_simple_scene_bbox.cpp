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

#include <lagrange/scene/simple_scene_bbox.h>
#include <lagrange/testing/common.h>

#include <utility>

TEST_CASE("simple_scene_bbox 3D", "[scene][simple_scene_bbox]")
{
    using Scalar = double;
    using Index = uint32_t;
    using SceneType = lagrange::scene::SimpleScene<Scalar, Index, 3>;
    using MeshType = lagrange::SurfaceMesh<Scalar, Index>;

    SECTION("empty scene")
    {
        const SceneType scene;
        REQUIRE(lagrange::scene::simple_scene_bbox(scene).isEmpty());
    }

    SECTION("mesh without instances")
    {
        SceneType scene;
        MeshType mesh;
        mesh.add_vertex({1, 2, 3});
        scene.add_mesh(std::move(mesh));
        REQUIRE(lagrange::scene::simple_scene_bbox(scene).isEmpty());
    }

    SECTION("empty instantiated mesh")
    {
        SceneType scene;
        const Index mesh_index = scene.add_mesh(MeshType{});
        SceneType::InstanceType instance;
        instance.mesh_index = mesh_index;
        scene.add_instance(instance);
        REQUIRE(lagrange::scene::simple_scene_bbox(scene).isEmpty());
    }

    SECTION("multiple transformed instances")
    {
        SceneType scene;
        MeshType mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 2, 3});
        const Index mesh_index = scene.add_mesh(std::move(mesh));

        SceneType::InstanceType first;
        first.mesh_index = mesh_index;
        first.transform = Eigen::Translation3d(-2, 0, 1) * Eigen::Scaling(2.0, 1.0, 1.0);
        scene.add_instance(first);

        SceneType::InstanceType second;
        second.mesh_index = mesh_index;
        second.transform = Eigen::Translation3d(4, -3, -1);
        scene.add_instance(second);

        const auto bbox = lagrange::scene::simple_scene_bbox(scene);
        REQUIRE(bbox.min() == Eigen::Vector3d(-2, -3, -1));
        REQUIRE(bbox.max() == Eigen::Vector3d(5, 2, 4));
    }
}

TEST_CASE("simple_scene_bbox 2D", "[scene][simple_scene_bbox]")
{
    using Scalar = float;
    using Index = uint64_t;
    using SceneType = lagrange::scene::SimpleScene<Scalar, Index, 2>;
    using MeshType = lagrange::SurfaceMesh<Scalar, Index>;

    SceneType scene;
    MeshType mesh(2);
    mesh.add_vertex({-1, 3});
    mesh.add_vertex({4, -2});
    const Index mesh_index = scene.add_mesh(std::move(mesh));

    SceneType::InstanceType instance;
    instance.mesh_index = mesh_index;
    instance.transform = Eigen::Translation2f(2, -1) * Eigen::Scaling(2.f, 3.f);
    scene.add_instance(instance);

    const auto bbox = lagrange::scene::simple_scene_bbox(scene);
    REQUIRE(bbox.min() == Eigen::Vector2f(0, -7));
    REQUIRE(bbox.max() == Eigen::Vector2f(10, 8));
}
