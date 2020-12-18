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
#include <sstream>
#include <string>

#include <lagrange/Logger.h>
#include <lagrange/io/load_mesh.h>
#include <lagrange/io/load_mesh_ext.h>

#include "test_load_mesh_data.h"

template <typename MeshType>
bool faces_in_range(const MeshType& mesh)
{
    auto& F = mesh.get_facets();
    for (auto row = 0; row < F.rows(); row++) {
        for (auto col = 0; col < F.cols(); col++) {
            const auto index = F(row, col);
            if (index < 0 || index >= mesh.get_num_vertices()) return false;
        }
    }
    return true;
}

TEST_CASE("MeshLoad Params", "[Mesh][Load]" LA_CORP_FLAG)
{
    const std::string name = "corp/core/banner_single.obj";
    for (bool load_normals : {true, false}) {
        for (bool load_materials : {true, false}) {
            for (bool load_uvs : {true, false}) {
                lagrange::io::MeshLoaderParams params;
                params.load_materials = load_materials;
                params.load_normals = load_normals;
                params.load_uvs = load_uvs;
                tinyobj::MaterialFileReader mtl_reader(lagrange::testing::get_data_path("corp/core/").string());
                auto result = lagrange::io::load_mesh_ext<lagrange::TriangleMesh3D>(
                    lagrange::testing::get_data_path(name),
                    params,
                    &mtl_reader);
                REQUIRE(result.success);
                REQUIRE(result.meshes.size() == 1);
                REQUIRE((load_materials != result.materials.empty()));
                REQUIRE(
                    (load_materials == result.meshes.front()->has_facet_attribute("material_id")));
                REQUIRE((load_uvs == result.meshes.front()->has_corner_attribute("uv")));
                REQUIRE((load_uvs == result.meshes.front()->is_uv_initialized()));
                REQUIRE((load_normals == result.meshes.front()->has_corner_attribute("normal")));
            }
        }
    }
    // Yay, we didn't crash
    SUCCEED();
}

TEST_CASE("MeshLoad Params (open)", "[Mesh][Load]")
{
    const std::string name = "open/core/hemisphere.obj";
    for (bool load_normals : {true, false}) {
        for (bool load_materials : {true, false}) {
            for (bool load_uvs : {true, false}) {
                lagrange::io::MeshLoaderParams params;
                params.load_materials = load_materials;
                params.load_normals = load_normals;
                params.load_uvs = load_uvs;
                tinyobj::MaterialFileReader mtl_reader(lagrange::testing::get_data_path("open/core/").string());
                auto result = lagrange::io::load_mesh_ext<lagrange::TriangleMesh3D>(
                    lagrange::testing::get_data_path(name),
                    params,
                    &mtl_reader);
                REQUIRE(result.success);
                REQUIRE(result.meshes.size() == 1);
                REQUIRE(result.materials.empty());
                REQUIRE(!result.meshes.front()->has_facet_attribute("material_id"));
                REQUIRE(!result.meshes.front()->has_corner_attribute("uv"));
                REQUIRE(!result.meshes.front()->is_uv_initialized());
                REQUIRE(!result.meshes.front()->has_corner_attribute("normal"));
            }
        }
    }
    // Yay, we didn't crash
    SUCCEED();
}


TEST_CASE("MeshLoad", "[Mesh][Load]")
{
    using namespace lagrange;
    using namespace lagrange::io;
    using Vertices3DL = Eigen::Matrix<long double, Eigen::Dynamic, 3, Eigen::RowMajor>;

    const std::string tmp_filename = "tmp.obj";

    SECTION("TriangleMesh3DTwoObjects")
    {
        {
            std::ofstream f(tmp_filename);
            f << obj_quad_multiple;
        }

        auto test_type = [](auto result) {
            REQUIRE(result.meshes.size() == 2);

            {
                REQUIRE(result.meshes[0]);
                auto& mesh = *result.meshes[0];
                REQUIRE(mesh.get_num_vertices() == 8);
                REQUIRE(mesh.get_num_facets() == 2 * 6);
                REQUIRE(faces_in_range(mesh));
                REQUIRE(mesh.is_uv_initialized());
                REQUIRE(mesh.get_uv_indices().rows() == mesh.get_num_facets());
            }
            {
                REQUIRE(result.meshes[1]);
                auto& mesh = *result.meshes[1];
                REQUIRE(mesh.get_num_vertices() == 4);
                REQUIRE(mesh.get_num_facets() == 2);
                REQUIRE(faces_in_range(mesh));
                REQUIRE(mesh.is_uv_initialized());
                REQUIRE(mesh.get_uv_indices().rows() == mesh.get_num_facets());
            }
        };

        // Single precision
        test_type(load_mesh_ext<Mesh<Vertices3Df, Triangles>>(tmp_filename));

        // Double
        test_type(load_mesh_ext<Mesh<Vertices3D, Triangles>>(tmp_filename));

        // Long double
        test_type(load_mesh_ext<Mesh<Vertices3DL, Triangles>>(tmp_filename));
    }

    SECTION("QuadMesh3DTwoObjects")
    {
        {
            std::ofstream f(tmp_filename);
            f << obj_quad_multiple;
        }

        auto test_type = [](auto result) {
            REQUIRE(result.meshes.size() == 2);

            {
                REQUIRE(result.meshes[0]);
                auto& mesh = *result.meshes[0];
                REQUIRE(mesh.get_num_vertices() == 8);
                REQUIRE(mesh.get_num_facets() == 6);
                REQUIRE(faces_in_range(mesh));
                REQUIRE(mesh.is_uv_initialized());
                REQUIRE(mesh.get_uv_indices().rows() == mesh.get_num_facets());
            }
            {
                REQUIRE(result.meshes[1]);
                auto& mesh = *result.meshes[1];
                REQUIRE(mesh.get_num_vertices() == 4);
                REQUIRE(mesh.get_num_facets() == 1);
                REQUIRE(faces_in_range(mesh));
                REQUIRE(mesh.is_uv_initialized());
                REQUIRE(mesh.get_uv_indices().rows() == mesh.get_num_facets());
            }
        };

        // Single precision
        test_type(load_mesh_ext<Mesh<Vertices3Df, Quads>>(tmp_filename));

        // Double
        test_type(load_mesh_ext<Mesh<Vertices3D, Quads>>(tmp_filename));

        // Long double
        test_type(load_mesh_ext<Mesh<Vertices3DL, Quads>>(tmp_filename));
    }

    SECTION("TwoObjectAsOne")
    {
        {
            std::ofstream f(tmp_filename);
            f << obj_quad_multiple;
        }

        MeshLoaderParams params;
        params.as_one_mesh = true;
        auto result = load_mesh_ext<Mesh<Vertices3Df, Triangles>>(tmp_filename, params);

        REQUIRE(result.meshes.size() == 1);
        {
            REQUIRE(result.meshes[0]);
            auto& mesh = *result.meshes[0];
            REQUIRE(mesh.get_num_vertices() == 8 + 4);
            REQUIRE(mesh.get_num_facets() == 12 + 2);
            REQUIRE(mesh.is_uv_initialized());
            REQUIRE(mesh.get_uv_indices().rows() == mesh.get_num_facets());
        }
    }


    SECTION("MixedToQuads")
    {
        {
            std::ofstream f(tmp_filename);
            f << obj_mixed_plane;
        }

        auto result = load_mesh_ext<Mesh<Vertices3D, Quads>>(tmp_filename);
        REQUIRE(result.meshes.size() == 1);

        {
            REQUIRE(result.meshes[0]);
            auto& mesh = *result.meshes[0];
            REQUIRE(mesh.get_num_facets() == 3);

            // Check padding
            auto& F = mesh.get_facets();
            REQUIRE(F(1, 3) == INVALID<typename Quads::Scalar>());
            REQUIRE(F(2, 3) == INVALID<typename Quads::Scalar>());
        }
    }

    SECTION("MixedToTriangles")
    {
        {
            std::ofstream f(tmp_filename);
            f << obj_mixed_plane;
        }

        auto result = load_mesh_ext<Mesh<Vertices3D, Triangles>>(tmp_filename);
        REQUIRE(result.meshes.size() == 1);

        {
            REQUIRE(result.meshes[0]);
            auto& mesh = *result.meshes[0];
            REQUIRE(mesh.get_num_facets() == 4);

            // Check no padding
            auto& F = mesh.get_facets();
            for (auto i = 0; i < F.size(); i++) {
                REQUIRE(F(i) != INVALID<typename Triangles::Scalar>());
            }
        }
    }

    SECTION("MixedVertexOnly")
    {
        {
            std::ofstream f(tmp_filename);
            f << obj_mixed_plane_vertex_only;
        }

        auto result = load_mesh_ext<Mesh<Vertices3D, Triangles>>(tmp_filename);
        REQUIRE(result.meshes.size() == 1);

        REQUIRE(result.meshes[0]);
        auto& mesh = *result.meshes[0];
        REQUIRE(mesh.get_num_facets() == 4);
        REQUIRE(mesh.get_num_vertices() == 4);
        REQUIRE(mesh.is_uv_initialized() == false);
        REQUIRE(mesh.has_corner_attribute("uv") == false);
        REQUIRE(mesh.has_corner_attribute("normal") == false);
    }

    SECTION("LoadMaterial")
    {
        {
            std::ofstream f(tmp_filename);
            f << obj_quad_multiple;
        }

        {
            std::ofstream f("material.mtl");
            f << mtl_material;
        }

        {
            tinyobj::MaterialFileReader mtl_reader("");
            auto result = load_mesh_ext<Mesh<Vertices3D, Triangles>>(tmp_filename, {}, &mtl_reader);
            REQUIRE(result.meshes.size() == 2);
            REQUIRE(result.materials.size() == 1);
            REQUIRE(result.materials[0].name == "Material");
        }

        {
            tinyobj::MaterialFileReader mtl_reader("some/random/path");
            auto result = load_mesh_ext<Mesh<Vertices3D, Triangles>>(tmp_filename, {}, &mtl_reader);
            REQUIRE(result.meshes.size() == 2);
            REQUIRE(result.materials.size() == 0);
        }
        {
            MeshLoaderParams p;
            p.load_materials = false;
            auto result = load_mesh_ext<Mesh<Vertices3D, Triangles>>(tmp_filename, p);
            REQUIRE(result.meshes.size() == 2);
            REQUIRE(result.materials.size() == 0);
        }
    }

    SECTION("LoadFromStream")
    {
        std::stringstream obj_stream;
        obj_stream << obj_quad_multiple;

        std::stringstream material_stream;
        material_stream << mtl_material;

        // Load both obj and material lib from streams
        {
            tinyobj::MaterialStreamReader mtl_reader(material_stream);
            auto result = load_mesh_ext<Mesh<Vertices3D, Triangles>>(obj_stream, {}, &mtl_reader);
            REQUIRE(result.meshes.size() == 2);
            REQUIRE(result.materials.size() == 1);
            REQUIRE(result.materials[0].name == "Material");
        }

        // Only load obj
        {
            obj_stream.seekg(0);

            auto result = load_mesh_ext<Mesh<Vertices3D, Triangles>>(obj_stream);
            REQUIRE(result.meshes.size() == 2);
            REQUIRE(result.materials.size() == 0);
        }
    }
}
