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
#include <lagrange/io/load_mesh.h>
#include <lagrange/io/load_scene.h>
#include <lagrange/io/load_simple_scene.h>
#include <lagrange/io/save_mesh.h>
#include <lagrange/io/save_scene.h>
#include <lagrange/io/save_simple_scene.h>
#include <lagrange/scene/scene_convert.h>
#include <lagrange/scene/simple_scene_convert.h>
#include <lagrange/testing/common.h>
#include <lagrange/utils/build.h>
#include <lagrange/utils/strings.h>
#include <lagrange/views.h>

#include <catch2/generators/catch_generators.hpp>

#include <string>

using namespace lagrange;

namespace {

void check_geometry(const SurfaceMesh32f& a, const SurfaceMesh32f& b)
{
    REQUIRE(a.get_num_vertices() == b.get_num_vertices());
    REQUIRE(a.get_num_facets() == b.get_num_facets());
    REQUIRE(vertex_view(a) == vertex_view(b));
    REQUIRE(facet_view(a) == facet_view(b));
}

} // namespace

TEST_CASE("case-insensitive IO extensions", "[io]")
{
    using Catch::Generators::table;
    const auto [extension, can_load_scene, can_load_simple_scene] = GENERATE(
        table<std::string, bool, bool>({
            {".obj", true, false},
            {".ply", false, false},
            {".msh", false, false},
            {".gltf", true, true},
            {".glb", true, true},
            {".fbx", true, true},
            {".stl", false, false},
            {".pcd", false, false},
            {".lgm", true, true},
            {".lgs", true, true},
            {".lgx", true, true},
        }));
    CAPTURE(extension);
#if LAGRANGE_TARGET_OS(WASM)
    if (extension == ".msh") {
        SKIP("MshIO is not supported on WebAssembly.");
    }
#endif

    const auto lower_path = testing::get_test_output_path("test_extension_case/lower" + extension);
    const auto upper_path =
        testing::get_test_output_path("test_extension_case/upper" + to_upper(extension));

    SurfaceMesh32f mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_triangle(0, 1, 2);

    io::SaveOptions save_options;
    save_options.quiet = true;
    const auto save = [&, ext = extension](const fs::path& path) {
        if (ext == ".fbx" || ext == ".stl") {
            const auto source = testing::get_data_path(
                ext == ".fbx" ? "open/io/avocado/avocado.fbx" : "open/io/61765.stl");
            fs::copy_file(source, path, fs::copy_options::overwrite_existing);
        } else if (ext == ".lgs") {
            io::save_simple_scene(path, scene::mesh_to_simple_scene(mesh), save_options);
        } else if (ext == ".lgx") {
            io::save_scene(path, scene::mesh_to_scene(mesh), save_options);
        } else {
            io::save_mesh(path, mesh, save_options);
        }
    };
    save(lower_path);
    save(upper_path);

    io::LoadOptions load_options;
    load_options.quiet = true;
    load_options.load_images = false;
    const auto lower_mesh = io::load_mesh<SurfaceMesh32f>(lower_path, load_options);
    const auto upper_mesh = io::load_mesh<SurfaceMesh32f>(upper_path, load_options);
    REQUIRE(lower_mesh.get_num_vertices() > 0);
    check_geometry(lower_mesh, upper_mesh);

    if (can_load_scene) {
        const auto lower_scene = io::load_scene<scene::Scene32f>(lower_path, load_options);
        const auto upper_scene = io::load_scene<scene::Scene32f>(upper_path, load_options);
        REQUIRE(lower_scene.meshes.size() == upper_scene.meshes.size());
        REQUIRE(lower_scene.nodes.size() == upper_scene.nodes.size());
        check_geometry(scene::scene_to_mesh(lower_scene), scene::scene_to_mesh(upper_scene));
    }

    if (can_load_simple_scene) {
        const auto lower_scene =
            io::load_simple_scene<scene::SimpleScene32f3>(lower_path, load_options);
        const auto upper_scene =
            io::load_simple_scene<scene::SimpleScene32f3>(upper_path, load_options);
        REQUIRE(lower_scene.get_num_meshes() == upper_scene.get_num_meshes());
        REQUIRE(lower_scene.compute_num_instances() == upper_scene.compute_num_instances());
        check_geometry(
            scene::simple_scene_to_mesh(lower_scene),
            scene::simple_scene_to_mesh(upper_scene));
    }
}
