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
////////////////////////////////////////////////////////////////////////////////
#include <lagrange/image/RawInputImage.h>
#include <lagrange/image/image_type_conversion.h>

#include <lagrange/Logger.h>

#include <lagrange/testing/common.h>
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("Image, boilerplate unit test", "[image]")
{
    // Very simple unit tests, basic checks

    std::vector<float> zero_image;
    zero_image.resize(1024, 0);

    LA_CHECK_THROWS(lagrange::image::make_default_rgba_image(16, 16, nullptr));
    LA_CHECK_THROWS(lagrange::image::make_default_rgba_image(0, 16, zero_image.data()));
    LA_CHECK_THROWS(lagrange::image::make_default_rgba_image(16, 0, zero_image.data()));


    lagrange::image::RawInputImage test2 =
        lagrange::image::make_default_rgba_image(16, 16, zero_image.data());
    lagrange::logger().info(
        "image test: {} x {}, {} rowbytes",
        test2.get_width(),
        test2.get_height(),
        test2.get_row_byte_stride());

    CHECK(test2.get_row_byte_stride() == 256);
}

TEST_CASE("Half precision view", "[image]")
{
    std::vector<float> zero_image;
    zero_image.resize(1024, 0);
    auto raw_img = lagrange::image::make_default_rgba_image(16, 16, zero_image.data());
    auto storage = lagrange::image::image_storage_from_raw_input_image(raw_img);

    auto float_view = std::make_shared<lagrange::image::ImageView<float>>(
        storage,
        raw_img.get_width(),
        raw_img.get_height());

    using Pixel = Eigen::Matrix<Eigen::half, 3, 1>;

    auto float16_view = std::make_shared<lagrange::image::ImageView<Pixel>>();

    float16_view->convert_from(*float_view, 1);

    REQUIRE(float16_view->get_precision() == lagrange::image::ImagePrecision::float16);
    REQUIRE(float16_view->get_channel() == lagrange::image::ImageChannel::three);
    REQUIRE(float16_view->get_view_stride_in_byte()(1) == sizeof(Pixel) * raw_img.get_width());

    lagrange::logger().info(
        "float16 image test: {} x {}, {} rowbytes",
        float16_view->get_view_size()(0),
        float16_view->get_view_size()(1),
        float16_view->get_view_stride_in_byte()(1));
}
