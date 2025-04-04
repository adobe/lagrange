/*
 * Copyright 2022 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/image/image_filters.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/range.h>

namespace lagrange {
namespace image {

void make_box_kernel(size_t size, image::ImageView<float>& kernel)
{
    kernel.resize(size, size, 1);
    for (auto j : range(size)) {
        for (auto i : range(size)) {
            kernel(i, j) = 1.0f / (float)size;
        }
    }
}

void make_sobelh_kernel(image::ImageView<float>& kernel)
{
    kernel.resize(3, 3, 1);
    kernel(0, 0) = -1.0f / 8.0f;
    kernel(1, 0) = 0.0f / 8.0f;
    kernel(2, 0) = 1.0f / 8.0f;
    kernel(0, 1) = -2.0f / 8.0f;
    kernel(1, 1) = 0.0f / 8.0f;
    kernel(2, 1) = 2.0f / 8.0f;
    kernel(0, 2) = -1.0f / 8.0f;
    kernel(1, 2) = 0.0f / 8.0f;
    kernel(2, 2) = 1.0f / 8.0f;
}

void make_sobelv_kernel(image::ImageView<float>& kernel)
{
    kernel.resize(3, 3, 1);
    kernel(0, 0) = -1.0f / 8.0f;
    kernel(1, 0) = -2.0f / 8.0f;
    kernel(2, 0) = -1.0f / 8.0f;
    kernel(0, 1) = 0.0f / 8.0f;
    kernel(1, 1) = 0.0f / 8.0f;
    kernel(2, 1) = 0.0f / 8.0f;
    kernel(0, 2) = 1.0f / 8.0f;
    kernel(1, 2) = 2.0f / 8.0f;
    kernel(2, 2) = 1.0f / 8.0f;
}

void make_gaussian_kernel(image::ImageView<float>& kernel)
{
    kernel.resize(3, 3, 1);
    kernel(0, 0) = 1.0f / 16.0f;
    kernel(1, 0) = 2.0f / 16.0f;
    kernel(2, 0) = 1.0f / 16.0f;
    kernel(0, 1) = 2.0f / 16.0f;
    kernel(1, 1) = 4.0f / 16.0f;
    kernel(2, 1) = 2.0f / 16.0f;
    kernel(0, 2) = 1.0f / 16.0f;
    kernel(1, 2) = 2.0f / 16.0f;
    kernel(2, 2) = 1.0f / 16.0f;
}

void make_weighted_avg_xkernel(image::ImageView<float>& kernel)
{
    kernel.resize(3, 1, 1);
    kernel(0, 0) = 1.0f / 4.0f;
    kernel(1, 0) = 2.0f / 4.0f;
    kernel(2, 0) = 1.0f / 4.0f;
}

void make_weighted_avg_ykernel(image::ImageView<float>& kernel)
{
    kernel.resize(1, 3, 1);
    kernel(0, 0) = 1.0f / 4.0f;
    kernel(0, 1) = 2.0f / 4.0f;
    kernel(0, 2) = 1.0f / 4.0f;
}

void make_diff_xkernel(image::ImageView<float>& kernel)
{
    kernel.resize(3, 1, 1);
    kernel(0, 0) = 1.0f;
    kernel(1, 0) = 0.0f;
    kernel(2, 0) = -1.0f;
}

void make_diff_ykernel(image::ImageView<float>& kernel)
{
    kernel.resize(1, 3, 1);
    kernel(0, 0) = 1.0f;
    kernel(0, 1) = 0.0f;
    kernel(0, 2) = -1.0f;
}

void convolve(
    const image::ImageView<float>& image,
    const image::ImageView<float>& kernel,
    image::ImageView<float>& result)
{
    const int image_width = (int)image.get_view_size()[0];
    const int image_height = (int)image.get_view_size()[1];

    const int kernel_width = (int)kernel.get_view_size()[0];
    const int kernel_height = (int)kernel.get_view_size()[1];

    la_runtime_assert(image_width > kernel_width);
    la_runtime_assert(image_height > kernel_height);

    image::ImageView<float> tmp(image_width, image_height, 1);

    int kernel_w_center = kernel_width / 2;
    int kernel_h_center = kernel_height / 2;
    // Traverse all the pixels in image
    for (auto j : range(image_height)) {
        for (auto i : range(image_width)) {
            // Apply kernel!
            float result_wh = 0.0f;
            for (auto kh : range(kernel_height)) {
                int h_index = j + (kh - kernel_h_center);
                if (h_index < 0) h_index = -h_index;
                if (h_index >= image_height)
                    h_index = (image_height - 1) + (image_height - h_index);

                for (auto kw : range(kernel_width)) {
                    float kvalue = kernel(kw, kh);

                    int w_index = i + (kw - kernel_w_center);
                    if (w_index < 0) w_index = -w_index;
                    if (w_index >= image_width)
                        w_index = (image_width - 1) + (image_width - w_index);

                    result_wh += kvalue * image(w_index, h_index);
                }
            }
            tmp(i, j) = std::abs(result_wh);
        }
    }

    result = tmp;
}

void sobel_x(const image::ImageView<float>& image, image::ImageView<float>& result)
{
    image::ImageView<float> weighted_avg_xkernel;
    make_weighted_avg_xkernel(weighted_avg_xkernel);

    image::ImageView<float> diff_xkernel;
    make_diff_xkernel(diff_xkernel);

    convolve(image, weighted_avg_xkernel, result);
    convolve(result, diff_xkernel, result);
}

void sobel_y(const image::ImageView<float>& image, image::ImageView<float>& result)
{
    image::ImageView<float> weighted_avg_ykernel;
    make_weighted_avg_ykernel(weighted_avg_ykernel);

    image::ImageView<float> diff_ykernel;
    make_diff_ykernel(diff_ykernel);

    convolve(image, weighted_avg_ykernel, result);
    convolve(result, diff_ykernel, result);
}

void image_dxx(const image::ImageView<float>& image, image::ImageView<float>& result)
{
    sobel_x(image, result);
    sobel_x(result, result);
}

void image_dyy(const image::ImageView<float>& image, image::ImageView<float>& result)
{
    sobel_y(image, result);
    sobel_y(result, result);
}

} // namespace image
} // namespace lagrange
