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
#include <lagrange/image/ImageView.h>
#include <lagrange/image/RawInputImage.h>
#include <lagrange/image_io/save_image.h>

#include <spdlog/sinks/basic_file_sink.h>
#include <CLI/CLI.hpp>

#include <array>
#include <fstream>
#include <iostream>
#include <tuple>

template <typename Scalar, size_t Dim>
void texture_sampling_impl(const std::string output_dir)
{
    lagrange::image::RawInputImage::precision_semantic pixel_precision;
    lagrange::image::RawInputImage::texture_format tex_format;
    std::string fn_ext;

    if (std::is_same<unsigned char, Scalar>::value) {
        pixel_precision = lagrange::image::RawInputImage::precision_semantic::byte_p;
        fn_ext = ".png";
    } else if (std::is_same<float, Scalar>::value) {
        pixel_precision = lagrange::image::RawInputImage::precision_semantic::single_p;
        fn_ext = ".exr";
    } else if (std::is_same<double, Scalar>::value) {
        pixel_precision = lagrange::image::RawInputImage::precision_semantic::double_p;
        fn_ext = ".bin";
    } else {
        throw std::runtime_error("unsupported Scalar!");
    }

    if (1 == Dim) {
        tex_format = lagrange::image::RawInputImage::texture_format::luminance;
    } else if (3 == Dim) {
        tex_format = lagrange::image::RawInputImage::texture_format::rgb;
    } else if (4 == Dim) {
        tex_format = lagrange::image::RawInputImage::texture_format::rgba;
    } else {
        throw std::runtime_error("unsupported Dim!");
    }

    using Traits = lagrange::image::RawInputImage::PixelTraits<Scalar, Dim>;
    using Pixel = typename Traits::Pixel;
    std::array<std::tuple<size_t, size_t>, 3> resolutions{
        std::make_tuple<size_t, size_t>(4, 4),
        std::make_tuple<size_t, size_t>(4, 6),
        std::make_tuple<size_t, size_t>(6, 4),
    };
    const std::string fn0 =
        "precision-" + std::to_string(sizeof(Scalar)) + "-dim-" + std::to_string(Dim);
    for (auto resolution : resolutions) {
        const auto width = std::get<0>(resolution);
        const auto height = std::get<1>(resolution);
        const std::string fn1 =
            fn0 + "-width-" + std::to_string(width) + "-height-" + std::to_string(height);
        lagrange::image::ImageView<Pixel> img(width, height, 1);
        for (size_t y = 0; y < height; ++y) {
            for (size_t x = 0; x < width; ++x) {
                size_t v[4] = {
                    x + y * width,
                    y + x * height,
                    ((x + (y + height / 2) * width) % (width * height)),
                    ((y + (x + width / 2) * height) % (width * height))};
                Pixel pix;
                if (std::is_same<unsigned char, Scalar>::value) {
                    for (size_t c = 0; c < Dim; ++c) {
                        Traits::coeff(pix, c) = static_cast<Scalar>(255 / (width * height) * v[c]);
                    }
                } else {
                    for (size_t c = 0; c < Dim; ++c) {
                        Traits::coeff(pix, c) = static_cast<Scalar>(1) /
                                                static_cast<Scalar>(width * height) *
                                                static_cast<Scalar>(v[c]);
                    }
                }
                img(x, y) = pix;
            }
        }
        lagrange::image_io::save_image<Pixel>(output_dir + "/" + fn1 + fn_ext, img);
        for (auto storage :
             {lagrange::image::RawInputImage::image_storage_format::first_pixel_row_at_top,
              lagrange::image::RawInputImage::image_storage_format::first_pixel_row_at_bottom}) {
            const std::string fn2 =
                fn1 + "-storage-" + std::to_string(static_cast<uint32_t>(storage));
            for (auto wrap_u :
                 {lagrange::image::RawInputImage::wrap_mode::repeat,
                  lagrange::image::RawInputImage::wrap_mode::clamp,
                  lagrange::image::RawInputImage::wrap_mode::mirror}) {
                for (auto wrap_v :
                     {lagrange::image::RawInputImage::wrap_mode::repeat,
                      lagrange::image::RawInputImage::wrap_mode::clamp,
                      lagrange::image::RawInputImage::wrap_mode::mirror}) {
                    const std::string fn3 =
                        fn2 + "-wrap_u-" + std::to_string(static_cast<uint32_t>(wrap_u)) +
                        "-wrap_v-" + std::to_string(static_cast<uint32_t>(wrap_v));
                    lagrange::image::RawInputImage raw;
                    raw.set_width(width);
                    raw.set_height(height);
                    raw.set_row_byte_stride(0);
                    raw.set_pixel_precision(pixel_precision);
                    raw.set_color_space(lagrange::image::RawInputImage::color_space::linear);
                    raw.set_tex_format(tex_format);
                    raw.set_wrap_u(wrap_u);
                    raw.set_wrap_v(wrap_v);
                    raw.set_storage_format(storage);
                    if (lagrange::image::RawInputImage::image_storage_format::
                            first_pixel_row_at_top == storage) {
                        raw.set_pixel_data(reinterpret_cast<const void *>(&(img(0, 0))), false);
                    } else {
                        raw.set_pixel_data(
                            reinterpret_cast<const void *>(&(img(0, height - 1))),
                            false);
                    }
                    for (auto filtering :
                         {lagrange::image::RawInputImage::texture_filtering::nearest,
                          lagrange::image::RawInputImage::texture_filtering::bilinear}) {
                        const std::string fn4 =
                            fn3 + "-filtering-" + std::to_string(static_cast<uint32_t>(filtering));
                        lagrange::image::ImageView<Pixel> img_samp(
                            width * 5 * 2,
                            height * 5 * 2,
                            1);
                        for (int y = -2; y < 3; ++y) {
                            for (size_t yy = 0; yy < height * 2; ++yy) {
                                float v =
                                    static_cast<float>(y) +
                                    static_cast<float>(yy + 0.5f) / static_cast<float>(height * 2);
                                for (int x = -2; x < 3; ++x) {
                                    for (size_t xx = 0; xx < width * 2; ++xx) {
                                        float u = static_cast<float>(x) +
                                                  static_cast<float>(xx + 0.5f) /
                                                      static_cast<float>(width * 2);
                                        img_samp(
                                            (x + 2) * width * 2 + xx,
                                            (y + 2) * height * 2 + yy) =
                                            raw.sample<float, Scalar, Dim>(u, v, filtering);
                                    }
                                }
                            }
                        }
                        lagrange::image_io::save_image<Pixel>(
                            output_dir + "/" + fn4 + fn_ext,
                            img_samp);
                    }
                }
            }
        }
    }
}

int main(int argc, char **argv)
{
    struct
    {
        std::string output = "./";
        std::string log_file;
        bool quiet = false;
        int log_level = 1; // debug
    } args;

    CLI::App app{argv[0]};
    app.option_defaults()->always_capture_default();
    app.add_option("-o,--output", args.output, "Output directory");

    // Logging options
    app.option_defaults()->always_capture_default();
    app.add_flag("-q,--quiet", args.quiet, "Hide logger on stdout.");
    app.add_option("-l,--level", args.log_level, "Log level (0 = most verbose, 6 = off).");
    app.add_option("-f,--log-file", args.log_file, "Log file.");
    CLI11_PARSE(app, argc, argv)

    // test
    texture_sampling_impl<unsigned char, 1>(args.output);
    texture_sampling_impl<unsigned char, 3>(args.output);
    texture_sampling_impl<unsigned char, 4>(args.output);
    texture_sampling_impl<float, 1>(args.output);
    texture_sampling_impl<float, 3>(args.output);
    texture_sampling_impl<float, 4>(args.output);

    return 0;
}
