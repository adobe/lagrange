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
#include <lagrange/Logger.h>
#include <lagrange/io/load_scene.h>
#include <lagrange/io/save_scene.h>

#include <CLI/CLI.hpp>

#include <iostream>

int main(int argc, char** argv)
{
    struct
    {
        std::string input;
        std::string output;
        bool embed_images = false;
    } args;

    lagrange::logger().set_level(spdlog::level::info);

    CLI::App app{argv[0]};
    app.option_defaults()->always_capture_default();
    app.add_option("input", args.input, "Input mesh.")->required()->check(CLI::ExistingFile);
    app.add_option("output", args.output, "Output mesh.")->required();
    app.add_flag("--embed-images", args.embed_images, "Embed images in the output scene.");
    CLI11_PARSE(app, argc, argv)

    auto scene = lagrange::io::load_scene<lagrange::scene::Scene<float, uint32_t>>(args.input);

    for (const auto& img : scene.images) {
        lagrange::logger().info("name: {}", img.name);
        const auto& image_buffer = img.image;
        lagrange::logger().info(
            "size: {} x {} x {}",
            image_buffer.width,
            image_buffer.height,
            image_buffer.num_channels);
        if (!img.uri.empty()) {
            lagrange::logger().info("uri: {}", img.uri.string());
        }
    }


    lagrange::io::SaveOptions save_options;
    save_options.embed_images = args.embed_images;
    lagrange::io::save_scene(args.output, scene, save_options);

    return 0;
}
