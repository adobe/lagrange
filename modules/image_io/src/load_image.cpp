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
#include <lagrange/image_io/load_image.h>

#include <lagrange/Logger.h>
#include <lagrange/image_io/common.h>
#include <lagrange/image_io/exr.h>

#include <stb_image.h>

namespace lagrange {
namespace image_io {

LoadImageResult load_image(const fs::path& path)
{
    LoadImageResult rtn;
    // basic sanity check
    if (path.empty()) {
        logger().error("load_image error: empty path '{}'", path.string());
        return rtn;
    }

    // get file extension
    auto ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    const auto type = file_extension_to_file_type(ext);
    if (FileType::unknown == type) {
        logger().error("load_image error: invalid extension '{}'", ext);
        return rtn;
    }

    // load
    if (FileType::png == type || FileType::jpg == type) {
        return load_image_stb(path);

    } else if (FileType::exr == type) {
        return load_image_exr(path);

    } else if (FileType::bin == type) {
        return load_image_bin(path);

    } else {
        logger().error(
            "load_image error, unknown file type: {}, {}",
            static_cast<unsigned int>(type),
            path.string());
        return rtn;
    }
}

LoadImageResult load_image_stb(const fs::path& path)
{
    LoadImageResult rtn;
    rtn.precision = image::ImagePrecision::uint8;
    int w, h, ch;

    unsigned char* data = stbi_load(path.string().c_str(), &w, &h, &ch, STBI_default);
    if (data == nullptr) return rtn;

    size_t _w = static_cast<size_t>(w);
    size_t _h = static_cast<size_t>(h);
    size_t _ch = static_cast<size_t>(ch);

    rtn.valid = true;
    rtn.width = _w;
    rtn.height = _h;
    rtn.channel = static_cast<image::ImageChannel>(ch);
    rtn.storage = std::make_shared<image::ImageStorage>(_ch * _w, _h, 1);
    std::copy_n(data, _ch * _w * _h, rtn.storage->data());
    stbi_image_free(data);
    data = nullptr;
    return rtn;
}

LoadImageResult load_image_exr(const fs::path& path)
{
    LoadImageResult rtn;

    void* out = nullptr;
    int width = 0;
    int height = 0;
    int components = 0;
    TinyexrPixelType pixeltype = TinyexrPixelType::unknown;

    bool success = load_image_exr(path, &out, &width, &height, &components, &pixeltype);
    if (!success) return rtn;

    size_t _width = static_cast<size_t>(width);
    size_t _height = static_cast<size_t>(height);
    size_t _components = static_cast<size_t>(components);

    if (pixeltype != TinyexrPixelType::unknown) {
        rtn.valid = true;
        rtn.channel = static_cast<image::ImageChannel>(components);
        rtn.width = _width;
        rtn.height = _height;

        if (TinyexrPixelType::uint32 == pixeltype) {
            rtn.precision = image::ImagePrecision::uint32;
            rtn.storage = std::make_shared<image::ImageStorage>(
                sizeof(unsigned int) * _components * _width,
                _height,
                1);
            std::copy_n(
                reinterpret_cast<const unsigned int*>(out),
                _components * _width * _height,
                reinterpret_cast<unsigned int*>(rtn.storage->data()));

        } else if (TinyexrPixelType::float32 == pixeltype) {
            rtn.precision = image::ImagePrecision::float32;
            rtn.storage = std::make_shared<image::ImageStorage>(
                sizeof(float) * _components * _width,
                _height,
                1);
            std::copy_n(
                reinterpret_cast<const float*>(out),
                _components * _width * _height,
                reinterpret_cast<float*>(rtn.storage->data()));
        }
    }

    free(out);
    return rtn;
}

LoadImageResult load_image_bin(const fs::path& path)
{
    LoadImageResult rtn;

    std::ifstream ifs(path, std::ios_base::binary);
    if (!ifs.is_open()) {
        logger().error("load_image error: cannot open file '{}'", path.string());
        return rtn;
    }

    std::string header;
    int width, height, components;
    {
        std::string buf;
        std::getline(ifs, buf);
        std::stringstream ss;
        ss << buf;
        ss >> header >> width >> height >> components;
        if (!ss.good() || ss.eof()) {
            logger().error("load_image error, cannot parse the header of *.bin: {}, {}", buf, path.string());
            return rtn;
        }
    }

    rtn.precision = bin_header_to_precision(header);
    if (image::ImagePrecision::unknown == rtn.precision) {
        logger().error("load_image error, invalid header of *.bin: {}, {}", header, path.string());
        return rtn;
    }

    if ((1 != components && 3 != components && 4 != components) || 0 >= width || 0 >= height) {
        logger().error(
            "load_image error, bad parameters of *.bin: {}, {}, {}, {}",
            path.string(),
            width,
            height,
            components);
        return rtn;
    }

    size_t _width = static_cast<size_t>(width);
    size_t _height = static_cast<size_t>(height);
    size_t _components = static_cast<size_t>(components);
    rtn.channel = static_cast<image::ImageChannel>(components);
    rtn.storage = std::make_shared<image::ImageStorage>(
        _width * _components * size_of_precision(rtn.precision),
        _height,
        1);
    ifs.read(
        reinterpret_cast<char*>(rtn.storage->data()),
        _width * _height * _components * size_of_precision(rtn.precision));
    if (ifs.eof() || !ifs.good()) {
        logger().error("load_image error, failed in reading data block for *.bin: {}", path.string());
        return rtn;
    }

    char tag;
    ifs.read(&tag, 1);
    if (!ifs.eof()) {
        logger().error(
            "load_image error, the data block is larger than expected for *.bin: {}",
            path.string());
        return rtn;
    }
    rtn.valid = true;
    return rtn;
}

} // namespace image_io
} // namespace lagrange
