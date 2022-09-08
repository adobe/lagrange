/*
 * Copyright 2019 Adobe. All rights reserved.
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

#include <lagrange/MeshTrait.h>
#include <lagrange/create_mesh.h>

TEST_CASE("MeshTrait", "[mesh_trait][mesh]")
{
    using namespace lagrange;

    auto assert_is_mesh = [](auto&& mesh) {
        using MeshType = decltype(mesh);
        static_assert(MeshTrait<MeshType>::is_mesh(), "Arg is not a Mesh");
    };

    auto assert_is_not_mesh = [](auto&& mesh) {
        using MeshType = decltype(mesh);
        static_assert(!MeshTrait<MeshType>::is_mesh(), "Arg should not be a Mesh");
    };

    auto assert_is_mesh_smart_ptr = [](auto&& mesh) {
        using MeshType = decltype(mesh);
        static_assert(MeshTrait<MeshType>::is_mesh_smart_ptr(), "Arg is not a Mesh smart pointer");
    };

    auto assert_is_not_mesh_smart_ptr = [](auto&& mesh) {
        using MeshType = decltype(mesh);
        static_assert(
            !MeshTrait<MeshType>::is_mesh_smart_ptr(),
            "Arg should not be a Mesh smart pointer");
    };

    auto assert_is_mesh_raw_ptr = [](auto&& mesh) {
        using MeshType = decltype(mesh);
        static_assert(MeshTrait<MeshType>::is_mesh_raw_ptr(), "Arg is not a Mesh raw pointer");
    };

    auto assert_is_not_mesh_raw_ptr = [](auto&& mesh) {
        using MeshType = decltype(mesh);
        static_assert(
            !MeshTrait<MeshType>::is_mesh_raw_ptr(),
            "Arg should not be a Mesh raw pointer");
    };

    Vertices3D vertices(3, 3);
    vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0;
    Triangles facets(1, 3);
    facets << 0, 1, 2;

    auto mesh = create_mesh(vertices, facets);
    assert_is_mesh(*mesh);
    assert_is_not_mesh(mesh);
    assert_is_not_mesh(mesh.get());
    assert_is_not_mesh(12);
    assert_is_not_mesh(vertices);
    assert_is_not_mesh(&facets);

    assert_is_mesh_smart_ptr(mesh);
    assert_is_not_mesh_smart_ptr(*mesh);
    assert_is_not_mesh_smart_ptr(mesh.get());
    assert_is_not_mesh_smart_ptr(12);

    assert_is_mesh_raw_ptr(mesh.get());
    assert_is_not_mesh_raw_ptr(mesh);
    assert_is_not_mesh_raw_ptr(*mesh);
    assert_is_not_mesh_raw_ptr(12);
    assert_is_not_mesh_raw_ptr(&vertices);
}
