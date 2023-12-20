/*
 * Copyright 2023 Adobe. All rights reserved.
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
#include <lagrange/io/load_mesh_assimp.h>
#include <lagrange/io/load_simple_scene_assimp.h>
#include <lagrange/io/internal/load_assimp.h>
#include <lagrange/utils/safe_cast.h>
#include <lagrange/SurfaceMesh.h>
#include <lagrange/attribute_names.h>
#include <lagrange/internal/find_attribute_utils.h>

using namespace lagrange;

TEST_CASE("load_mesh_assimp", "[io]") {
    auto mesh = io::load_mesh_assimp<lagrange::SurfaceMesh32f>(
        testing::get_data_path("open/core/drop_tri.obj"));
    REQUIRE(mesh.get_num_facets() > 0);
    REQUIRE(mesh.has_attribute(std::string(AttributeName::texcoord) + "_0"));
    REQUIRE(mesh.has_attribute(AttributeName::normal));
}

TEST_CASE("load_assimp_fbx", "[io]")
{
    auto scene = lagrange::io::internal::load_assimp(
        lagrange::testing::get_data_path("corp/io/rp_adanna_rigged_001_zup_t.fbx"));
    REQUIRE(scene != nullptr);
    REQUIRE(scene->mNumMeshes == 1); // one mesh with one material, but multiple components
    auto* mesh = scene->mMeshes[0];
    REQUIRE(mesh->mNumFaces == 12025);
    REQUIRE(mesh->mNumBones == 88); // fbx has bone information

    REQUIRE(scene->mNumMaterials == 1);
    REQUIRE(scene->mMaterials[mesh->mMaterialIndex]->mNumProperties > 0);

    auto lmesh = lagrange::io::internal::load_mesh_assimp<lagrange::SurfaceMesh32f>(*scene);
    REQUIRE(lmesh.get_num_facets() > 0);
    REQUIRE(lmesh.get_num_vertices() > 0);
    //REQUIRE(lmesh.has_attribute(AttributeName::texcoord));
    REQUIRE(lmesh.has_attribute(AttributeName::normal));
    REQUIRE(lmesh.has_attribute(AttributeName::indexed_joint));
    REQUIRE(lmesh.has_attribute(AttributeName::indexed_weight));
    REQUIRE(
        internal::find_matching_attribute<float>(
            lmesh,
            "",
            BitField<AttributeElement>(AttributeElement::Vertex | AttributeElement::Indexed),
            AttributeUsage::UV,
            2) != invalid_attribute_id());
}

TEST_CASE("load_assimp_glb", "[io]")
{
    auto scene = lagrange::io::internal::load_assimp(
        lagrange::testing::get_data_path("open/core/blub/blub.glb"));
    REQUIRE(scene != nullptr);
    REQUIRE(scene->mNumMeshes == 1);
    auto* mesh = scene->mMeshes[0];
    REQUIRE(mesh->mNumFaces > 0);

    REQUIRE(scene->mNumMaterials == 2);
    REQUIRE(scene->mMaterials[mesh->mMaterialIndex]->mNumProperties > 0);

    using Index = lagrange::SurfaceMesh32f::Index;
    auto lmesh = lagrange::io::internal::load_mesh_assimp<lagrange::SurfaceMesh32f>(*scene);
    REQUIRE(lmesh.get_num_vertices() == lagrange::safe_cast<Index>(mesh->mNumVertices));
    REQUIRE(lmesh.get_num_facets() == lagrange::safe_cast<Index>(mesh->mNumFaces));
    REQUIRE(lmesh.has_attribute(std::string(AttributeName::texcoord) + "_0"));
    REQUIRE(lmesh.has_attribute(AttributeName::normal));
}

TEST_CASE("load_simple_scene_assimp", "[io]") {
    auto scene = lagrange::io::load_simple_scene_assimp<lagrange::scene::SimpleScene32f3>(
        lagrange::testing::get_data_path("open/io/three_cubes_instances.gltf"));
    REQUIRE(scene.get_num_meshes() == 1);
    REQUIRE(scene.get_num_instances(0) == 3);

    const auto& t1 = scene.get_instance(0, 0).transform;
    const auto& t2 = scene.get_instance(0, 1).transform;
    const auto& t3 = scene.get_instance(0, 2).transform;
    REQUIRE(!t1.isApprox(t2));
    REQUIRE(!t2.isApprox(t3));
    REQUIRE(!t3.isApprox(t1));
}

#endif
