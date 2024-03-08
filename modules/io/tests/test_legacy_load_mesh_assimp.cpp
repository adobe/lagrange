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

#ifdef LAGRANGE_WITH_ASSIMP

#include <lagrange/testing/common.h>
#include <lagrange/Logger.h>
#include <lagrange/io/load_mesh_assimp.h>
#include <lagrange/utils/safe_cast.h>

TEST_CASE("legacy_load_mesh_assimp", "[mesh][io]") {
    using namespace lagrange;
    auto meshes = lagrange::io::load_mesh_assimp<TriangleMesh3D>(
        lagrange::testing::get_data_path("open/core/drop_tri.obj"));

    REQUIRE(meshes.size() == 1);
    REQUIRE(meshes.front()->get_num_facets() > 0);
}

TEST_CASE("legacy_load_scene_assimp", "[io]") {
    auto scene = lagrange::io::load_scene_assimp(
        lagrange::testing::get_data_path("open/core/drop_tri.obj"));
    REQUIRE(scene != nullptr);
    REQUIRE(scene->mNumMeshes == 1);
    REQUIRE(scene->mMeshes[0]->mNumFaces > 0);
}

TEST_CASE("legacy load fbx", "[io]" LA_CORP_FLAG) {
    auto scene = lagrange::io::load_scene_assimp(
        lagrange::testing::get_data_path("corp/io/rp_adanna_rigged_001_zup_t.fbx"));
    REQUIRE(scene != nullptr);
    REQUIRE(scene->mNumMeshes == 1); // one mesh with one material, but multiple components
    auto* mesh = scene->mMeshes[0];
    REQUIRE(mesh->mNumFaces == 12025);
    REQUIRE(mesh->mNumBones == 88); // fbx has bone information

    REQUIRE(scene->mNumMaterials == 1);
    REQUIRE(scene->mMaterials[mesh->mMaterialIndex]->mNumProperties > 0);
}

TEST_CASE("legacy load glb", "[io]") {
    auto scene = lagrange::io::load_scene_assimp(
        lagrange::testing::get_data_path("open/core/blub/blub.glb"));
    REQUIRE(scene != nullptr);
    REQUIRE(scene->mNumMeshes == 1);
    auto* mesh = scene->mMeshes[0];
    REQUIRE(mesh->mNumFaces > 0);

    REQUIRE(scene->mNumMaterials == 2);
    REQUIRE(scene->mMaterials[mesh->mMaterialIndex]->mNumProperties > 0);

    auto lmeshes =
        lagrange::io::legacy::extract_meshes_assimp<lagrange::TriangleMesh3D>(scene.get());
    REQUIRE(lmeshes.size() == 1);

    using Index = lagrange::TriangleMesh3D::Index;
    auto lmesh = lagrange::io::legacy::convert_mesh_assimp<lagrange::TriangleMesh3D>(mesh);
    REQUIRE(lmesh->get_num_vertices() == lagrange::safe_cast<Index>(mesh->mNumVertices));
    REQUIRE(lmesh->get_num_facets() == lagrange::safe_cast<Index>(mesh->mNumFaces));
    REQUIRE(lmesh->is_uv_initialized());
}

#endif
