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

#include "../tests/image_helpers.h"

#include <lagrange/texproc/extract_mesh_with_alpha_mask.h>

#include <lagrange/AttributeValueType.h>
#include <lagrange/IndexedAttribute.h>
#include <lagrange/Logger.h>
#include <lagrange/common.h>
#include <lagrange/image/Array3D.h>
#include <lagrange/io/load_scene.h>
#include <lagrange/io/save_scene.h>
#include <lagrange/scene/Scene.h>
#include <lagrange/scene/scene_utils.h>
#include <lagrange/triangulate_polygonal_facets.h>

#include <CLI/CLI.hpp>

#include <unordered_map>

namespace fs = lagrange::fs;
using Scene = lagrange::scene::Scene<float, uint32_t>;
using SurfaceMesh = Scene::MeshType;

int main(int argc, char** argv)
{
    struct
    {
        fs::path input_path;
        fs::path output_path;
        bool split_grids = false;
        size_t log_level = 2;
    } args;

    CLI::App app{argv[0]};
    app.option_defaults()->always_capture_default();
    app.add_option("input", args.input_path, "Input scene.")->required()->check(CLI::ExistingFile);
    app.add_option("output", args.output_path, "Output scene.");
    app.add_option("-l,--level", args.log_level, "Log level.");

    CLI11_PARSE(app, argc, argv)

    auto logger = lagrange::logger();
    spdlog::set_level(static_cast<spdlog::level::level_enum>(args.log_level));

    logger.info("loading scene \"{}\"", args.input_path.string());
    auto scene_options = lagrange::io::LoadOptions();
    scene_options.stitch_vertices = true;
    auto scene = lagrange::io::load_scene<Scene>(args.input_path, scene_options);

    using ElementId = lagrange::scene::ElementId;
    struct Payload
    {
        ElementId image_id;
        int texcoord_id;
        float alpha_threshold;
    };
    std::unordered_map<ElementId, Payload> material_to_payloads;
    {
        ElementId material_id = 0;
        for (const auto& material : scene.materials) {
            if (material.alpha_mode == lagrange::scene::MaterialExperimental::AlphaMode::Opaque) {
                material_id += 1;
                continue;
            }
            if (material.alpha_cutoff <= 0.0 || material.alpha_cutoff >= 1.0) {
                material_id += 1;
                continue;
            }
            if (material.base_color_texture.index >= scene.textures.size()) {
                material_id += 1;
                continue;
            }
            const auto& base_color_texture = scene.textures.at(material.base_color_texture.index);
            if (scene.images.at(base_color_texture.image).image.num_channels != 4) {
                material_id += 1;
                continue;
            }
            material_to_payloads[material_id] = Payload{
                base_color_texture.image,
                material.base_color_texture.texcoord,
                material.alpha_cutoff,
            };
            material_id += 1;
        }
    }
    logger.info("found {} compatible materials", material_to_payloads.size());

    lagrange::scene::MaterialExperimental material_;
    const auto material_id_ = scene.add(material_);
    size_t num_extracted = 0;
    for (auto& node : scene.nodes) {
        la_runtime_assert(!node.name.empty());
        for (auto& instance : node.meshes) {
            if (instance.materials.size() != 1) {
                throw std::runtime_error("multi-material instance not supported");
                continue;
            }
            la_runtime_assert(instance.materials.size() == 1);
            const auto iter = material_to_payloads.find(instance.materials.front());
            if (iter == material_to_payloads.end()) continue;
            const auto& payload = iter->second;
            const auto image =
                test::scene_image_to_image_array(scene.images.at(payload.image_id).image);
            auto mesh = scene.meshes.at(instance.mesh);
            la_runtime_assert(image.extent(2) == 4, "must have alpha channel");
            const auto texcoord_id =
                mesh.get_attribute_id(fmt::format("texcoord_{}", payload.texcoord_id));
            if (texcoord_id == lagrange::invalid_attribute_id()) continue;
            if (!mesh.is_attribute_indexed(texcoord_id)) continue;
            lagrange::texproc::ExtractMeshWithAlphaMaskOptions extract_options;
            extract_options.texcoord_id = texcoord_id;
            extract_options.alpha_threshold = payload.alpha_threshold;
            auto mesh_ = lagrange::texproc::extract_mesh_with_alpha_mask(
                mesh,
                image.to_mdspan(),
                extract_options);
            const auto mesh_id_ = scene.add(mesh_);
            instance.mesh = mesh_id_;
            instance.materials.clear();
            instance.materials.emplace_back(material_id_);
            num_extracted += 1;
        }
    }
    logger.info("extracted {} meshes", num_extracted);

    if (!args.output_path.empty()) {
        logger.info("saving scene \"{}\"", args.output_path.string());
        lagrange::io::save_scene(args.output_path, scene);
    }

    return 0;
}
