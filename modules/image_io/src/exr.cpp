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
#include <lagrange/Logger.h>
#include <lagrange/image_io/exr.h>

#include <tinyexr.h>

#include <cstring>
#include <iostream>
#include <sstream>
#include <vector>

namespace {
void SetErrorMessage(const std::string& msg, const char** err)
{
    if (err) {
#ifdef _WIN32
        (*err) = _strdup(msg.c_str());
#else
        (*err) = strdup(msg.c_str());
#endif
    }
}
} // namespace

namespace {

// internal utilities to load exr
int LoadEXR(
    void** data,
    int* width,
    int* height,
    int* components,
    lagrange::image_io::TinyexrPixelType* pixeltype,
    const char* filename,
    const char** err)
{
    if (data == NULL) {
        SetErrorMessage("Invalid argument for LoadEXR()", err);
        return TINYEXR_ERROR_INVALID_ARGUMENT;
    }

    EXRVersion exr_version;
    EXRImage exr_image;
    EXRHeader exr_header;
    InitEXRHeader(&exr_header);
    InitEXRImage(&exr_image);

    {
        int ret = ParseEXRVersionFromFile(&exr_version, filename);
        if (ret != TINYEXR_SUCCESS) {
            SetErrorMessage("Invalid EXR header", err);
            return ret;
        }

        if (exr_version.multipart || exr_version.non_image) {
            SetErrorMessage(
                "Loading multipart or DeepImage is not supported  in LoadEXR() API",
                err);
            return TINYEXR_ERROR_INVALID_DATA; // @fixme.
        }
    }

    {
        int ret = ParseEXRHeaderFromFile(&exr_header, &exr_version, filename, err);
        if (ret != TINYEXR_SUCCESS) {
            FreeEXRHeader(&exr_header);
            return ret;
        }
    }

    // sanity check, only support all channels are the same type
    for (int i = 0; i < exr_header.num_channels; i++) {
        if (exr_header.pixel_types[i] != exr_header.pixel_types[0] ||
            exr_header.requested_pixel_types[i] != exr_header.pixel_types[i]) {
            SetErrorMessage(
                "pixel_types and requested_pixel_types are not consistent, or with "
                "hybrid pixel format",
                err);
            FreeEXRHeader(&exr_header);
            FreeEXRImage(&exr_image);
            return TINYEXR_ERROR_INVALID_DATA; // @fixme.
        }
    }

    // Read HALF channel as FLOAT.
    for (int i = 0; i < exr_header.num_channels; i++) {
        if (exr_header.pixel_types[i] == TINYEXR_PIXELTYPE_HALF) {
            exr_header.requested_pixel_types[i] = TINYEXR_PIXELTYPE_FLOAT;
        }
    }

    if (TINYEXR_PIXELTYPE_FLOAT == exr_header.requested_pixel_types[0]) {
        *pixeltype = lagrange::image_io::TinyexrPixelType::float32;
    } else if (TINYEXR_PIXELTYPE_UINT == exr_header.requested_pixel_types[0]) {
        *pixeltype = lagrange::image_io::TinyexrPixelType::uint32;
    } else {
        *pixeltype = lagrange::image_io::TinyexrPixelType::unknown;
        SetErrorMessage("unknown pixel type", err);
        FreeEXRHeader(&exr_header);
        FreeEXRImage(&exr_image);
        return TINYEXR_ERROR_INVALID_DATA;
    }

    {
        int ret = LoadEXRImageFromFile(&exr_image, &exr_header, filename, err);
        if (ret != TINYEXR_SUCCESS) {
            FreeEXRHeader(&exr_header);
            FreeEXRImage(&exr_image);
            return ret;
        }
    }

    // RGBA
    int idxR = -1;
    int idxG = -1;
    int idxB = -1;
    int idxA = -1;
    for (int c = 0; c < exr_header.num_channels; c++) {
        if (strcmp(exr_header.channels[c].name, "R") == 0) {
            idxR = c;
        } else if (strcmp(exr_header.channels[c].name, "G") == 0) {
            idxG = c;
        } else if (strcmp(exr_header.channels[c].name, "B") == 0) {
            idxB = c;
        } else if (strcmp(exr_header.channels[c].name, "A") == 0) {
            idxA = c;
        }
    }

    if (1 != exr_header.num_channels && 3 != exr_header.num_channels &&
        4 != exr_header.num_channels) {
        SetErrorMessage("num_channels must be 1, 3, or 4", err);
        FreeEXRHeader(&exr_header);
        FreeEXRImage(&exr_image);
        return TINYEXR_ERROR_INVALID_DATA;
    }
    if (3 == exr_header.num_channels) {
        if (idxR == -1 || idxG == -1 || idxB == -1) {
            SetErrorMessage("Not all RGB channel found", err);
            FreeEXRHeader(&exr_header);
            FreeEXRImage(&exr_image);
            return TINYEXR_ERROR_INVALID_DATA;
        }
    }

    static_assert(
        sizeof(float) == sizeof(unsigned int),
        "always copies as uint, assuming uint and float are with same size");

    (*data) = malloc(
        static_cast<size_t>(exr_header.num_channels) * sizeof(unsigned int) *
        static_cast<size_t>(exr_image.width) * static_cast<size_t>(exr_image.height));
    unsigned int* data_uint = static_cast<unsigned int*>(*data);
    int channel_slots[4] = {idxR, idxG, idxB};
    if (1 == exr_header.num_channels) {
        channel_slots[0] = 0;
    }
    const int gray_or_rgb_ch = (exr_header.num_channels > 3 ? 3 : exr_header.num_channels);
    if (exr_header.tiled) {
        for (int it = 0; it < exr_image.num_tiles; it++) {
            for (int j = 0; j < exr_header.tile_size_y; j++) {
                for (int i = 0; i < exr_header.tile_size_x; i++) {
                    const int ii = exr_image.tiles[it].offset_x * exr_header.tile_size_x + i;
                    const int jj = exr_image.tiles[it].offset_y * exr_header.tile_size_y + j;
                    const int idx = ii + jj * exr_image.width;

                    // out of region check.
                    if (ii >= exr_image.width) {
                        continue;
                    }
                    if (jj >= exr_image.height) {
                        continue;
                    }
                    const int srcIdx = i + j * exr_header.tile_size_x;
                    unsigned char** src = exr_image.tiles[it].images;
                    for (int ch = 0; ch < gray_or_rgb_ch; ++ch) {
                        data_uint[idx * exr_header.num_channels + ch] =
                            reinterpret_cast<unsigned int**>(src)[channel_slots[ch]][srcIdx];
                    }
                    if (4 == exr_header.num_channels) {
                        if (idxA < 0) {
                            if (lagrange::image_io::TinyexrPixelType::float32 == *pixeltype) {
                                reinterpret_cast<float*>(data_uint)[idx * 4 + 3] = 1.0f;
                            } else if (lagrange::image_io::TinyexrPixelType::uint32 == *pixeltype) {
                                data_uint[idx * 4 + 3] = 1u;
                            }
                        } else {
                            data_uint[idx * 4 + 3] =
                                reinterpret_cast<unsigned int**>(src)[idxA][srcIdx];
                        }
                    }
                }
            }
        }
    } else {
        for (int i = 0; i < exr_image.width * exr_image.height; i++) {
            for (int ch = 0; ch < gray_or_rgb_ch; ++ch) {
                data_uint[i * exr_header.num_channels + ch] =
                    reinterpret_cast<unsigned int**>(exr_image.images)[channel_slots[ch]][i];
            }
            if (4 == exr_header.num_channels) {
                if (idxA < 0) {
                    if (lagrange::image_io::TinyexrPixelType::float32 == *pixeltype) {
                        reinterpret_cast<float*>(data_uint)[i * 4 + 3] = 1.0f;
                    } else if (lagrange::image_io::TinyexrPixelType::uint32 == *pixeltype) {
                        data_uint[i * 4 + 3] = 1u;
                    }
                } else {
                    data_uint[i * 4 + 3] =
                        reinterpret_cast<unsigned int**>(exr_image.images)[idxA][i];
                }
            }
        }
    }

    (*width) = exr_image.width;
    (*height) = exr_image.height;
    (*components) = exr_header.num_channels;

    FreeEXRHeader(&exr_header);
    FreeEXRImage(&exr_image);

    return TINYEXR_SUCCESS;
}

// internal utilities to save exr
int SaveEXR(
    const void* data,
    int width,
    int height,
    int components,
    const lagrange::image_io::TinyexrPixelType pixeltype,
    const char* outfilename,
    const char** err)
{
    if ((components == 1) || components == 3 || components == 4) {
        // OK
    } else {
        std::stringstream ss;
        ss << "Unsupported component value : " << components << std::endl;

        SetErrorMessage(ss.str(), err);
        return TINYEXR_ERROR_INVALID_ARGUMENT;
    }

    if (lagrange::image_io::TinyexrPixelType::unknown == pixeltype) {
        SetErrorMessage("unknown pixeltype", err);
        return TINYEXR_ERROR_INVALID_ARGUMENT;
    }

    EXRHeader header;
    InitEXRHeader(&header);

    if ((width < 16) && (height < 16)) {
        // No compression for small image.
        header.compression_type = TINYEXR_COMPRESSIONTYPE_NONE;
    } else {
        header.compression_type = TINYEXR_COMPRESSIONTYPE_ZIP;
    }

    EXRImage image;
    InitEXRImage(&image);

    image.num_channels = components;

    static_assert(
        sizeof(float) == sizeof(unsigned int),
        "always copies as uint, assuming uint and float are with same size");

    std::vector<unsigned int> images[4];

    if (components == 1) {
        images[0].resize(static_cast<size_t>(width * height));
        std::copy_n(
            static_cast<const unsigned int*>(data),
            static_cast<size_t>(width * height),
            images[0].data());
    } else {
        images[0].resize(static_cast<size_t>(width * height));
        images[1].resize(static_cast<size_t>(width * height));
        images[2].resize(static_cast<size_t>(width * height));
        images[3].resize(static_cast<size_t>(width * height));

        // Split RGB(A)RGB(A)RGB(A)... into R, G and B(and A) layers
        for (size_t i = 0; i < static_cast<size_t>(width * height); i++) {
            images[0][i] =
                (static_cast<const unsigned int*>(data))[static_cast<size_t>(components) * i + 0];
            images[1][i] =
                (static_cast<const unsigned int*>(data))[static_cast<size_t>(components) * i + 1];
            images[2][i] =
                (static_cast<const unsigned int*>(data))[static_cast<size_t>(components) * i + 2];
            if (components == 4) {
                images[3][i] = (static_cast<const unsigned int*>(
                    data))[static_cast<size_t>(components) * i + 3];
            }
        }
    }

    unsigned int* image_ptr[4] = {0, 0, 0, 0};
    if (components == 4) {
        image_ptr[0] = &(images[3].at(0)); // A
        image_ptr[1] = &(images[2].at(0)); // B
        image_ptr[2] = &(images[1].at(0)); // G
        image_ptr[3] = &(images[0].at(0)); // R
    } else if (components == 3) {
        image_ptr[0] = &(images[2].at(0)); // B
        image_ptr[1] = &(images[1].at(0)); // G
        image_ptr[2] = &(images[0].at(0)); // R
    } else if (components == 1) {
        image_ptr[0] = &(images[0].at(0)); // A
    }

    image.images = reinterpret_cast<unsigned char**>(image_ptr);
    image.width = width;
    image.height = height;

    header.num_channels = components;
    header.channels = static_cast<EXRChannelInfo*>(
        malloc(sizeof(EXRChannelInfo) * static_cast<size_t>(header.num_channels)));
    // Must be (A)BGR order, since most of EXR viewers expect this channel order.
    if (components == 4) {
#ifdef _MSC_VER
        strncpy_s(header.channels[0].name, "A", 255);
        strncpy_s(header.channels[1].name, "B", 255);
        strncpy_s(header.channels[2].name, "G", 255);
        strncpy_s(header.channels[3].name, "R", 255);
#else
        strncpy(header.channels[0].name, "A", 255);
        strncpy(header.channels[1].name, "B", 255);
        strncpy(header.channels[2].name, "G", 255);
        strncpy(header.channels[3].name, "R", 255);
#endif
        header.channels[0].name[strlen("A")] = '\0';
        header.channels[1].name[strlen("B")] = '\0';
        header.channels[2].name[strlen("G")] = '\0';
        header.channels[3].name[strlen("R")] = '\0';
    } else if (components == 3) {
#ifdef _MSC_VER
        strncpy_s(header.channels[0].name, "B", 255);
        strncpy_s(header.channels[1].name, "G", 255);
        strncpy_s(header.channels[2].name, "R", 255);
#else
        strncpy(header.channels[0].name, "B", 255);
        strncpy(header.channels[1].name, "G", 255);
        strncpy(header.channels[2].name, "R", 255);
#endif
        header.channels[0].name[strlen("B")] = '\0';
        header.channels[1].name[strlen("G")] = '\0';
        header.channels[2].name[strlen("R")] = '\0';
    } else {
#ifdef _MSC_VER
        strncpy_s(header.channels[0].name, "A", 255);
#else
        strncpy(header.channels[0].name, "A", 255);
#endif
        header.channels[0].name[strlen("A")] = '\0';
    }

    header.pixel_types =
        static_cast<int*>(malloc(sizeof(int) * static_cast<size_t>(header.num_channels)));
    header.requested_pixel_types =
        static_cast<int*>(malloc(sizeof(int) * static_cast<size_t>(header.num_channels)));

    if (lagrange::image_io::TinyexrPixelType::uint32 == pixeltype) {
        for (int i = 0; i < header.num_channels; i++) {
            header.pixel_types[i] = TINYEXR_PIXELTYPE_UINT; // pixel type of input image
            header.requested_pixel_types[i] = TINYEXR_PIXELTYPE_UINT; // save to
        }
    } else if (lagrange::image_io::TinyexrPixelType::float32 == pixeltype) {
        for (int i = 0; i < header.num_channels; i++) {
            header.pixel_types[i] = TINYEXR_PIXELTYPE_FLOAT; // pixel type of input image
            header.requested_pixel_types[i] = TINYEXR_PIXELTYPE_FLOAT; // save to
        }
    }

    int ret = SaveEXRImageToFile(&image, &header, outfilename, err);
    if (ret != TINYEXR_SUCCESS) {
        return ret;
    }

    free(header.channels);
    free(header.pixel_types);
    free(header.requested_pixel_types);

    return ret;
}
} // namespace

namespace lagrange {
namespace image_io {

// impl of load & save utilities
bool load_image_exr(
    const lagrange::fs::path& path,
    void** data,
    int* width,
    int* height,
    int* components,
    TinyexrPixelType* pixeltype)
{
    if (nullptr == data || nullptr == width || nullptr == height || nullptr == components ||
        nullptr == pixeltype || path.empty()) {
        logger().error(
            "failed in load_exr, invalid params: {}, {}, {}, {}, {}, {}",
            path.empty() ? "path is empty" : "path is good",
            nullptr == data ? "data is nullptr" : "data is good",
            nullptr == width ? "width is nullptr" : "width is good",
            nullptr == height ? "height is nullptr" : "height is good",
            nullptr == components ? "components is nullptr" : "components is good",
            nullptr == pixeltype ? "pixeltype is nullptr" : "pixeltype is good");
        return false;
    }
    const char* err = nullptr;
    int ret = LoadEXR(data, width, height, components, pixeltype, path.string().c_str(), &err);

    if (ret != TINYEXR_SUCCESS) {
        logger().error("load exr error, file: {}, code: {}", path.string(), ret);
        if (err) {
            logger().error("err msg: {}", err);
            FreeEXRErrorMessage(err); // release memory of error message.
        }
        return false;
    } else {
        return true;
    }
}

bool save_image_exr(
    const lagrange::fs::path& path,
    const void* data,
    const int width,
    const int height,
    const int components,
    const TinyexrPixelType pixeltype)
{
    if (nullptr == data || 0 >= width || 0 >= height ||
        (1 != components && 3 != components && 4 != components) ||
        TinyexrPixelType::unknown == pixeltype || path.empty()) {
        logger().error(
            "failed in save_exr, invalid params: {}, {}, {}, {}, {}, {}",
            path.empty() ? "path is empty" : "path is good",
            nullptr == data ? "data is nullptr" : "data is good",
            0 >= width ? "width is not positive" : "width is good",
            0 >= height ? "height is not positive" : "height is good",
            1 != components && 3 != components && 4 != components ? "components is not 1 or 3 or 4"
                                                                  : "components is good",
            TinyexrPixelType::unknown == pixeltype ? "pixeltype is unknown" : "pixeltype is good");
        return false;
    }
    const char* err = nullptr;
    auto result = SaveEXR(
        reinterpret_cast<const float*>(data),
        width,
        height,
        components,
        pixeltype,
        path.string().c_str(),
        &err);
    if (TINYEXR_SUCCESS != result) {
        logger().error("save exr error, file: {}, code {}", path.string(), result);
        if (err) {
            logger().error("err msg: {}", err);
            FreeEXRErrorMessage(err);
        }
        return false;
    } else {
        return true;
    }
}

} // namespace image_io
} // namespace lagrange
