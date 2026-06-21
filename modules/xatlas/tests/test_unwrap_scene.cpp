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
#include <lagrange/xatlas/repack_scene.h>
#include <lagrange/xatlas/unwrap_scene.h>

#include <lagrange/SurfaceMesh.h>
#include <lagrange/scene/SimpleScene.h>
#include <lagrange/testing/common.h>
#include <lagrange/utils/Error.h>

namespace {

template <typename Scalar, typename Index>
lagrange::SurfaceMesh<Scalar, Index> make_triangle()
{
    lagrange::SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_triangle(0, 1, 2);
    return mesh;
}

template <typename Scalar, typename Index>
lagrange::scene::SimpleScene<Scalar, Index, 3> make_test_scene()
{
    lagrange::scene::SimpleScene<Scalar, Index, 3> scene;
    auto m = make_triangle<Scalar, Index>();
    auto idx = scene.add_mesh(m);
    typename lagrange::scene::SimpleScene<Scalar, Index, 3>::InstanceType inst;
    inst.mesh_index = idx;
    scene.add_instance(inst);
    typename lagrange::scene::SimpleScene<Scalar, Index, 3>::InstanceType inst2;
    inst2.mesh_index = idx;
    inst2.transform.translate(Eigen::Matrix<Scalar, 3, 1>(2, 0, 0));
    scene.add_instance(inst2);
    return scene;
}

} // namespace

TEST_CASE("empty unwrap", "[xatlas][unwrap_scene]")
{
    lagrange::scene::SimpleScene<float, uint32_t, 3> scene;
    auto out = lagrange::xatlas::unwrap_scene(scene);
    REQUIRE(out.get_num_meshes() == 0);
}

TEST_CASE("empty repack", "[xatlas][unwrap_scene]")
{
    lagrange::scene::SimpleScene<float, uint32_t, 3> scene;
    auto out = lagrange::xatlas::repack_scene(scene);
    REQUIRE(out.get_num_meshes() == 0);
}

TEST_CASE("shared UVs", "[xatlas][unwrap_scene]")
{
    auto scene = make_test_scene<float, uint32_t>();
    lagrange::xatlas::UnwrapOptions options;
    options.enable_sharing_uvs_between_instances = true;
    auto out = lagrange::xatlas::unwrap_scene(scene, options);
    REQUIRE(out.get_num_meshes() == scene.get_num_meshes());
    REQUIRE(out.compute_num_instances() == scene.compute_num_instances());
    REQUIRE(out.get_mesh(0).has_attribute(options.output_uv_attribute_name));
    REQUIRE(out.get_mesh(0).is_attribute_indexed(options.output_uv_attribute_name));
}

TEST_CASE("per-instance UVs", "[xatlas][unwrap_scene]")
{
    auto scene = make_test_scene<float, uint32_t>();
    lagrange::xatlas::UnwrapOptions options;
    options.enable_sharing_uvs_between_instances = false;
    auto out = lagrange::xatlas::unwrap_scene(scene, options);
    REQUIRE(out.get_num_meshes() == scene.compute_num_instances());
    REQUIRE(out.compute_num_instances() == scene.compute_num_instances());
    for (uint32_t i = 0; i < out.get_num_meshes(); ++i) {
        REQUIRE(out.get_mesh(i).has_attribute(options.output_uv_attribute_name));
        REQUIRE(out.get_mesh(i).is_attribute_indexed(options.output_uv_attribute_name));
    }
}

TEST_CASE("per_instance_importance size mismatch throws", "[xatlas][unwrap_scene]")
{
    auto scene = make_test_scene<float, uint32_t>();
    lagrange::xatlas::UnwrapOptions opts;
    lagrange::xatlas::SceneOptions sopts;
    sopts.per_instance_importance = {1.f}; // scene has 2 instances
    REQUIRE_THROWS_AS(lagrange::xatlas::unwrap_scene(scene, opts, sopts), lagrange::Error);
}
