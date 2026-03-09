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
#include <lagrange/io/save_mesh.h>
#include <lagrange/map_attribute.h>
#include <lagrange/polyscope/register_structure.h>
#include <lagrange/unify_index_buffer.h>
#include <lagrange/views.h>

#include <spdlog/fmt/ranges.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <polyscope/polyscope.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <CLI/CLI.hpp>

using SurfaceMesh = lagrange::SurfaceMesh32d;

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
        int log_level = 2; // normal
    } args;

    CLI::App app{argv[0]};
    app.add_option("inputs", args.inputs, "Input mesh(es).")->required()->check(CLI::ExistingFile);
    app.add_option("-l,--level", args.log_level, "Log level (0 = most verbose, 6 = off).");
    CLI11_PARSE(app, argc, argv)

    spdlog::set_level(static_cast<spdlog::level::level_enum>(args.log_level));

    polyscope::options::configureImGuiStyleCallback = []() {
        ImGui::Spectrum::StyleColorsSpectrum();
        ImGui::Spectrum::LoadFont();
    };
    polyscope::init();

    for (auto input : args.inputs) {
        lagrange::logger().info("Loading input mesh: {}", input.string());
        auto mesh = lagrange::io::load_mesh<SurfaceMesh>(input);
        prepare_mesh(mesh);
        lagrange::polyscope::register_structure(input.stem().string(), std::move(mesh));
    }

    polyscope::show();

    return 0;
}
