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

#include <lagrange/io/internal/scene_utils.h>
#include <lagrange/image_io/load_image.h>

namespace lagrange::io::internal {

bool try_load_image(const std::string& name, const LoadOptions& options, scene::ImageLegacy& image)
{
    fs::path path = name;
    if (path.is_relative() && !options.search_path.empty()) path = options.search_path / name;
    if (path.empty()) return false;
    if (image.data) return false;

    image_io::LoadImageResult result = image_io::load_image(path);
    if (!result.valid) return false;

    image.width = result.width;
    image.height = result.height;
    image.precision = result.precision;
    image.channel = result.channel;
    image.data = result.storage;
    image.uri = name;
    if (path.has_extension()) {
        auto extension = path.extension();
        if (extension == ".jpeg" || extension == ".jpg") {
            image.type = scene::ImageLegacy::Type::Jpeg;
        } else if (extension == ".png") {
            image.type = scene::ImageLegacy::Type::Png;
        } else if (extension == ".bmp") {
            image.type = scene::ImageLegacy::Type::Bmp;
        } else if (extension == ".gif") {
            image.type = scene::ImageLegacy::Type::Gif;
        } else {
            image.type = scene::ImageLegacy::Type::Unknown;
        }
    }
    return true;
}

} // namespace lagrange::io::internal
