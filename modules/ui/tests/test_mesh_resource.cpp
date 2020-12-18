/*
 * Copyright 2020 Adobe. All rights reserved.
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

#include <lagrange/Logger.h>
#include <lagrange/ui/MeshModel.h>
#include <lagrange/ui/default_resources.h>
#include <lagrange/utils/warning.h>

using namespace lagrange;
using namespace lagrange::ui;

TEST_CASE("dummy", "[ui][resource][mesh]")
{
    // Reference gl3w library
    // Makes Apple Clang linker happy
    gl3wIsSupported(3, 3);
    SUCCEED();
}

TEST_CASE("VertexFacetInitialiation", "[ui][resource][mesh]")
{
    using V = Vertices3Df;
    using F = Triangles;
    using MeshType = Mesh<V, F>;
    register_default_resources();
    register_mesh_resource<V, F>();

    V vertices(4, 3);
    vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0;

    F facets(2, 3);
    facets << 0, 1, 2, 2, 1, 3;


    auto res = Resource<MeshBase>::create_deferred(std::move(vertices), std::move(facets));
    auto proxy = Resource<ProxyMesh>::create_deferred(res, (MeshType*)(nullptr));

    SECTION("Initialization")
    {
        auto mesh_ptr = &(*res);
        LA_IGNORE(mesh_ptr);
        auto mesh = res.try_cast<MeshType>();
        REQUIRE(mesh);
        REQUIRE(mesh->get_num_vertices() == 4);
        REQUIRE(mesh->get_num_facets() == 2);
    }


    SECTION("ProxyInitialization")
    {
        REQUIRE(proxy->get_num_vertices() == 4);
        REQUIRE(proxy->get_num_triangles() == 2);
    }

    ResourceFactory::clear();
}

#ifdef LAGRANGE_UI_OPENGL_TESTS

// TODO deprecate MeshModel
TEST_CASE("MeshModelInit", "[ui][resource][mesh][model]")
{
    using V = Vertices3D;
    using F = Triangles;
    register_default_resources();
    register_mesh_resource<V, F>();

    auto mesh = lagrange::create_sphere(2);

    auto mm = MeshModel(std::move(mesh));
    ResourceFactory::clear();
}


TEST_CASE("FileInitializationNoMat", "[ui][resource][mesh]")
{
    using V = Vertices3Df;
    using F = Triangles;
    using MeshType = Mesh<V, F>;
    register_default_resources();
    register_mesh_resource<V, F>();

    lagrange::fs::path obj_path = lagrange::testing::get_data_path("open/core/rounded_cube.obj");

    SECTION("fs::path")
    {
        auto res = Resource<ObjResult<V, F>>::create(obj_path, io::MeshLoaderParams());

        REQUIRE(res->meshes.size() == 1);
        REQUIRE(res->mesh_to_material[0].size() == 0);

        auto& mesh = res->meshes[0].cast<MeshType>();

        REQUIRE(mesh.get_num_vertices() == 864);
        REQUIRE(mesh.get_num_facets() == 1724);
    }

    SECTION("std::string")
    {
        auto res = Resource<ObjResult<V, F>>::create(obj_path.string(), io::MeshLoaderParams());
        REQUIRE(res);
    }

    SECTION("const char *")
    {
        auto res =
            Resource<ObjResult<V, F>>::create(obj_path.string().c_str(), io::MeshLoaderParams());
        REQUIRE(res);
    }

    ResourceFactory::clear();
}


TEST_CASE("FileInitializationWithMat", "[ui][resource][mesh]")
{
    using V = Vertices3Df;
    using F = Triangles;
    using MeshType = Mesh<V, F>;
    register_default_resources();
    register_mesh_resource<V, F>();

    auto res = Resource<ObjResult<V, F>>::create(
        lagrange::testing::get_data_path("open/core/blub/blub.obj"),
        io::MeshLoaderParams());

    REQUIRE(res->meshes.size() == 1);
    REQUIRE(res->mesh_to_material[0].size() == 1);

    auto& mesh = res->meshes[0].cast<MeshType>();

    LA_IGNORE(mesh);

    ResourceFactory::clear();
}

#endif
