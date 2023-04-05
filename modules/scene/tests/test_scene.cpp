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
#include <lagrange/testing/common.h>

#include <lagrange/scene/SimpleScene.h>
#include <lagrange/scene/SimpleSceneTypes.h>

#include <unordered_set>

namespace {

template <typename Scalar, typename Index, size_t Dim>
void test_simple_scene_basic()
{
    using SceneType = lagrange::scene::SimpleScene<Scalar, Index, Dim>;
    using MeshType = lagrange::SurfaceMesh<Scalar, Index>;

    SceneType scene;
    MeshType mesh1, mesh2, mesh3;

    scene.reserve_meshes(3);
    REQUIRE(scene.get_num_meshes() == 0);

    auto mesh_index1 = scene.add_mesh(mesh1);
    REQUIRE(mesh_index1 == 0);
    auto mesh_index2 = scene.add_mesh(mesh2);
    REQUIRE(mesh_index2 == 1);
    auto mesh_index3 = scene.add_mesh(mesh3);
    REQUIRE(mesh_index3 == 2);

    scene.reserve_instances(mesh_index1, 3);
    REQUIRE(scene.get_num_instances(mesh_index1) == 0);

    auto mesh1_instance1 = scene.add_instance({mesh_index1, {}, {}});
    auto mesh1_instance2 = scene.add_instance({mesh_index1, {}, {}});
    auto mesh1_instance3 = scene.add_instance({mesh_index1, {}, {}});

    auto mesh2_instance1 = scene.add_instance({mesh_index2, {}, {}});
    auto mesh2_instance2 = scene.add_instance({mesh_index2, {}, {}});

    auto mesh3_instance1 = scene.add_instance({mesh_index3, {}, {}});

    REQUIRE(mesh1_instance1 == mesh2_instance1);
    REQUIRE(mesh1_instance2 == mesh2_instance2);
    REQUIRE(mesh1_instance1 == mesh3_instance1);
    REQUIRE(mesh1_instance1 == 0);
    REQUIRE(mesh1_instance2 == 1);
    REQUIRE(mesh1_instance3 == 2);
    REQUIRE(scene.compute_num_instances() == 6);

    REQUIRE(scene.get_mesh(mesh_index2).get_num_vertices() == 0);
    scene.ref_mesh(mesh_index2).add_vertices(10);
    REQUIRE(scene.get_mesh(mesh_index2).get_num_vertices() == 10);
    REQUIRE(scene.get_mesh(mesh_index1).get_num_vertices() == 0);
    REQUIRE(scene.get_mesh(mesh_index3).get_num_vertices() == 0);

    std::unordered_set<Index> valid_mesh_indices = {mesh_index1, mesh_index2, mesh_index3};
    scene.foreach_instances(
        [&](const auto& instance) { REQUIRE(valid_mesh_indices.count(instance.mesh_index)); });
}

} // namespace

TEST_CASE("SimpleScene: basic", "[scene]")
{
#define LA_X_simple_scene_basic(_, Scalar, Index, Dim) \
    test_simple_scene_basic<Scalar, Index, Dim>();
    LA_SIMPLE_SCENE_X(simple_scene_basic, 0)
}
