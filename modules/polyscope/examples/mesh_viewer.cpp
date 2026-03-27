/*
 * Copyright 2024 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/find_matching_attributes.h>
#include <lagrange/io/load_mesh.h>
#include <lagrange/io/load_simple_scene.h>
#include <lagrange/io/save_mesh.h>
#include <lagrange/map_attribute.h>
#include <lagrange/polyscope/register_structure.h>
#include <lagrange/scene/simple_scene_convert.h>
#include <lagrange/unify_index_buffer.h>
#include <lagrange/views.h>

#include <spdlog/fmt/ranges.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <polyscope/polyscope.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <CLI/CLI.hpp>

using Scalar = double;
using Index = uint32_t;
using SurfaceMesh = lagrange::SurfaceMesh<Scalar, Index>;
using SimpleScene = lagrange::scene::SimpleScene<Scalar, Index, 3>;

void prepare_mesh(SurfaceMesh& mesh)
{
    lagrange::AttributeMatcher matcher;
    matcher.element_types = lagrange::AttributeElement::Indexed;

    // 1st, unify index buffers for all non-UV indexed attributes
    matcher.usages = ~lagrange::BitField(lagrange::AttributeUsage::UV);
    auto ids = lagrange::find_matching_attributes(mesh, matcher);
    if (!ids.empty()) {
        std::vector<std::string> attr_names;
        for (auto id : ids) {
            attr_names.emplace_back(mesh.get_attribute_name(id));
        }
        lagrange::logger().info(
            "Unifying index buffers for {} non-UV indexed attributes: {}",
            ids.size(),
            fmt::join(attr_names, ", "));
        mesh = lagrange::unify_index_buffer(mesh, ids);
    }

    // 2nd, convert indexed UV attributes into corner attributes
    matcher.usages = lagrange::AttributeUsage::UV;
    ids = lagrange::find_matching_attributes(mesh, matcher);
    for (auto id : ids) {
        lagrange::logger().info(
            "Converting indexed UV attribute to corner attribute: {}",
            mesh.get_attribute_name(id));
        map_attribute_in_place(mesh, id, lagrange::AttributeElement::Corner);
    }
}

int main(int argc, char** argv)
{
    struct
    {
        std::vector<lagrange::fs::path> inputs;
        bool scene = false;
        int log_level = 2; // normal
    } args;

    CLI::App app{argv[0]};
    app.add_option("inputs", args.inputs, "Input mesh(es).")->required()->check(CLI::ExistingFile);
    app.add_option("-l,--level", args.log_level, "Log level (0 = most verbose, 6 = off).");
    app.add_flag("--scene", args.scene, "Load as a scene (instead of a single mesh per file).");
    CLI11_PARSE(app, argc, argv)

    spdlog::set_level(static_cast<spdlog::level::level_enum>(args.log_level));

    polyscope::options::configureImGuiStyleCallback = []() {
        ImGui::Spectrum::StyleColorsSpectrum();
        ImGui::Spectrum::LoadFont();
    };
    polyscope::init();

    lagrange::io::LoadOptions load_options;
    load_options.stitch_vertices = true;

    for (auto input : args.inputs) {
        lagrange::logger().info("Loading input: {}", input.string());

        if (args.scene) {
            SimpleScene simple_scene =
                lagrange::io::load_simple_scene<SimpleScene>(input, load_options);
            auto meshes = lagrange::scene::simple_scene_to_meshes(simple_scene);
            lagrange::logger().info(
                "Loaded scene with {} mesh(es) from {}",
                meshes.size(),
                input.string());
            for (size_t i = 0; i < meshes.size(); ++i) {
                prepare_mesh(meshes[i]);
                std::string name = input.stem().string() + "_" + std::to_string(i);
                lagrange::polyscope::register_structure(name, std::move(meshes[i]));
            }
        } else {
            SurfaceMesh mesh = lagrange::io::load_mesh<SurfaceMesh>(input, load_options);
            prepare_mesh(mesh);
            lagrange::polyscope::register_structure(input.stem().string(), std::move(mesh));
        }
    }

    polyscope::show();

    return 0;
}
