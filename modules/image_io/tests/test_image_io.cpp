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
#include <lagrange/testing/common.h>
#include <catch2/catch_approx.hpp>

#include <lagrange/image_io/common.h>
#include <lagrange/image_io/load_image.h>
#include <lagrange/image_io/save_image.h>

#include <array>
#include <cstdio>

constexpr int test_image_width = 509;
constexpr int test_image_height = 184;

// Test image has the first few pixels set to:
// white, red, green, blue, black, empty (alpha = 0).
// blue pixel on the top right corner
// green pixel on bottom left corner
// red pixel on bottom right corner

void check_example_image(const lagrange::image_io::LoadImageResult& image)
{
    REQUIRE(image.valid);
    REQUIRE(image.width == test_image_width);
    REQUIRE(image.height == test_image_height);
    REQUIRE(
        image.storage->get_full_size().x() ==
        image.width * static_cast<int>(image.channel) *
            lagrange::image_io::size_of_precision(image.precision));
    REQUIRE(image.storage->get_full_size().y() == image.height);
}


TEST_CASE("load png", "[image_io]")
{
    auto image = lagrange::image_io::load_image(
        lagrange::testing::get_data_path("corp/image_io/example.png"));
    check_example_image(image);
    REQUIRE(image.channel == lagrange::image::ImageChannel::four);
    REQUIRE(image.precision == lagrange::image::ImagePrecision::uint8);
    unsigned int* uidata = (unsigned int*)image.storage->data();
    auto check_pixel = [&](size_t x, size_t y, unsigned int abgr) {
        size_t idx = (y * image.width) + x;
        REQUIRE(uidata[idx] == abgr);
    };
    check_pixel(0, 0, 0xFFFFFFFF);
    check_pixel(1, 0, 0xFF0000FF);
    check_pixel(2, 0, 0xFFFF0000);
    check_pixel(3, 0, 0xFF00FF00);
    check_pixel(4, 0, 0xFF000000);
    check_pixel(5, 0, 0x00000000);
    check_pixel(image.width - 1, 0, 0xFFFF0000);
    check_pixel(0, image.height - 1, 0xFF00FF00);
    check_pixel(image.width - 1, image.height - 1, 0xFF0000FF);
}

TEST_CASE("load jpg", "[image_io]")
{
    auto image = lagrange::image_io::load_image(
        lagrange::testing::get_data_path("corp/image_io/example.jpg"));
    check_example_image(image);
    // since jpg is a lossy format, don't check for pixel values
    REQUIRE(image.channel == lagrange::image::ImageChannel::three);
    REQUIRE(image.precision == lagrange::image::ImagePrecision::uint8);
}

TEST_CASE("load exr", "[image_io]")
{
    auto image = lagrange::image_io::load_image(
        lagrange::testing::get_data_path("corp/image_io/example.exr"));
    check_example_image(image);
    REQUIRE(image.channel == lagrange::image::ImageChannel::four);
    REQUIRE(image.precision == lagrange::image::ImagePrecision::float32);

    unsigned char* data = image.storage->data();
    float* fdata = (float*)data;
    auto check_pixel = [&](size_t x, size_t y, float a, float r, float g, float b) {
        size_t idx = ((y * image.width) + x) * 4;
        REQUIRE(fdata[idx + 0] == r);
        REQUIRE(fdata[idx + 1] == g);
        REQUIRE(fdata[idx + 2] == b);
        REQUIRE(fdata[idx + 3] == a);
    };
    check_pixel(0, 0, 1.f, 1.f, 1.f, 1.f);
    check_pixel(1, 0, 1.f, 1.f, 0.f, 0.f);
    check_pixel(2, 0, 1.f, 0.f, 0.f, 1.f);
    check_pixel(3, 0, 1.f, 0.f, 1.f, 0.f);
    check_pixel(4, 0, 1.f, 0.f, 0.f, 0.f);
    check_pixel(5, 0, 0.f, 0.f, 0.f, 0.f);
    check_pixel(image.width - 1, 0, 1.f, 0.f, 0.f, 1.f);
    check_pixel(0, image.height - 1, 1.f, 0.f, 1.f, 0.f);
    check_pixel(image.width - 1, image.height - 1, 1.f, 1.f, 0.f, 0.f);
}

TEST_CASE("Exr IO", "[image_io]")
{
    size_t width = 2, height = 2;
    size_t num_channels = 4; // rgba
    std::vector<float> colors(width * height * num_channels, 0);
    for (size_t j = 0; j < height; j++) {
        for (size_t i = 0; i < width; i++) {
            size_t index = j * width + i;
            colors[index * 4] = float(i) / float(width);
            colors[index * 4 + 1] = float(j) / float(height);
            colors[index * 4 + 2] = float(index) / float(width * height);
            colors[index * 4 + 3] = 1.0;
        }
    }

    lagrange::fs::path tmp_file("image_io_debug.exr");

    lagrange::image_io::save_image_exr(
        tmp_file,
        reinterpret_cast<unsigned char*>(colors.data()),
        width,
        height,
        lagrange::image::ImagePrecision::float32,
        lagrange::image::ImageChannel::four);

    auto image = lagrange::image_io::load_image(tmp_file);
    float* data = reinterpret_cast<float*>(image.storage->data());

    for (size_t i = 0; i < width * height * 4; i++) {
        REQUIRE(colors[i] == Catch::Approx(data[i]));
    }

    std::remove(tmp_file.string().c_str());
}
