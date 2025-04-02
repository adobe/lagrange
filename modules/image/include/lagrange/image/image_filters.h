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
#pragma once

#include <lagrange/image/ImageView.h>

namespace lagrange {
namespace image {

/**
 * Creates a \p size by \p size kernel with all values set to 1 / \p size.
 *
 * @param[in]  size The size of the kernel.
 * @param[out] kernel The resulting kernel.
 */
void make_box_kernel(size_t size, image::ImageView<float>& kernel);

/**
 * Creates a 3 by 3 horizontal Sobel filter kernel.
 *
 * @param[out] kernel The resulting kernel.
 */
void make_sobelh_kernel(image::ImageView<float>& kernel);

/**
 * Creates a 3 by 3 vertical Sobel filter kernel.
 *
 * @param[out] kernel The resulting kernel.
 */
void make_sobelv_kernel(image::ImageView<float>& kernel);

/**
 * Creates a 3 by 3 approximation of a Gaussian filter kernel.
 *
 * @param[out] kernel The resulting kernel.
 */
void make_gaussian_kernel(image::ImageView<float>& kernel);

/**
 * Creates a 3 by 1 horizontal kernel with weights (1, 2, 1) / 4.
 *
 * @param[out] kernel The resulting kernel.
 */
void make_weighted_avg_xkernel(image::ImageView<float>& kernel);

/**
 * Creates a 1 by 3 vertical kernel with weights (1, 2, 1) / 4.
 *
 * @param[out] kernel The resulting kernel.
 */
void make_weighted_avg_ykernel(image::ImageView<float>& kernel);

/**
 * Creates a 3 by 1 horizontal finite difference kernel with weights (-1, 0, 1).
 *
 * @param[out] kernel The resulting kernel.
 */
void make_diff_xkernel(image::ImageView<float>& kernel);

/**
 * Creates a 1 by 3 vertical finite difference kernel with weights (-1, 0, 1).
 *
 * @param[out] kernel The resulting kernel.
 */
void make_diff_ykernel(image::ImageView<float>& kernel);

/**
 * Convolves the given image with the specified kernel, using periodic boundary conditions.
 *
 * @param[in]  image The input image. Must be larger than the kernel
 * @param[in]  kernel The kernel.
 * @param[out] result The result of the convolution. Can be the same as \p image.
 */
void convolve(
    const image::ImageView<float>& image,
    const image::ImageView<float>& kernel,
    image::ImageView<float>& result);

/**
 * Convolves the given image with a horizontal Sobel filter.
 *
 * @param[in]  image The input image.
 * @param[out] result The result of the convolution. Can be the same as \p image.
 */
void sobel_x(const image::ImageView<float>& image, image::ImageView<float>& result);

/**
 * Convolves the given image with a vertical Sobel filter.
 *
 * @param[in]  image The input image.
 * @param[out] result The result of the convolution. Can be the same as \p image.
 */
void sobel_y(const image::ImageView<float>& image, image::ImageView<float>& result);

/**
 * Convolves the given image twice with a horizontal Sobel filter.
 *
 * @param[in]  image The input image.
 * @param[out] result The result of the convolution. Can be the same as \p image.
 */
void image_dxx(const image::ImageView<float>& image, image::ImageView<float>& result);

/**
 * Convolves the given image twice with a vertical Sobel filter.
 *
 * @param[in]  image The input image.
 * @param[out] result The result of the convolution. Can be the same as \p image.
 */
void image_dyy(const image::ImageView<float>& image, image::ImageView<float>& result);

} // namespace image
} // namespace lagrange
