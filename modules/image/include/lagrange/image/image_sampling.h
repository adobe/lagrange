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
#pragma once

#include <lagrange/common.h>
#include <lagrange/image/ImageView.h>

namespace lagrange {
namespace image {

/**
 * Populates a list of approximately \p n_samples sample points with integer coordinates ranging
 * from (0, 0) to (width - 1, height - 1), where (width, height) is the size of the input
 * density map.
 *
 * @param[in]  density_map The desired density of samples, with larger values indicating higher
 * likelihood of sampling.
 * @param[in]  n_samples The approximate number of samples to generate. The actual number of samples
 * may may be smaller.
 * @param[out] samples The resulting samples.
 */
void sample_from_density_map(
    const image::ImageView<float>& density_map,
    size_t n_samples,
    lagrange::Vertices2Df& samples);

/**
 * Type of sampling to use 
 */
enum class SampleType
{
    Density, ///< sample according to density map
    Regular ///< sample regularly (ignores density)
};

/**
 * Populates a list of \p n_samples samples with samples along the image borders. 
 * 
 * There are two sampling modes:
 * SampleType::Density: sample according to density map.
 * SampleType::Regular: sample regularly (ignores density).
 * 
 * @param[in]  density_map The input density map.
 * @param[in]  n_samples The approximate number of samples to generate. The actual number of samples
 * may be smaller.
 * @param[out] samples The resulting samples.
 * @param[in]  type The sampling type. Default is SampleType::Regular.
 */
void sample_borders(
    const image::ImageView<float>& density_map,
    size_t n_samples,
    lagrange::Vertices2Df& samples,
    SampleType type = SampleType::Regular);

/**
 * Populates a list of \p n_samples sample according to the density of the border pixeslin \p
 * density_map.
 *
 * @param[in]  density_map The input density map.
 * @param[in]  n_samples The approximate number of samples to generate. The actual number of samples
 * may be smaller.
 * @param[out] samples The resulting samples.
 */
void density_sample_borders(
    const image::ImageView<float>& density_map,
    size_t n_samples,
    lagrange::Vertices2Df& samples);


/**
 * Populates a list of approximately \p n_samples sample points with integer coordinates, including
 * the four corners as well as uniformly spaced points along all four edges of a rectangle matching
 * the size of the input density map. The values of the density map are ignored.
 *
 * @param[in]  density_map The input density map.
 * @param[in]  n_samples The approximate number of samples to generate. The actual number of samples
 * may be smaller.
 * @param[out] samples The resulting samples.
 */
void regular_sample_borders(
    const image::ImageView<float>& density_map,
    size_t n_samples,
    lagrange::Vertices2Df& samples);

/**
 * Samples a single point from the input image using bilinear interpolation.
 *
 * @param image with float values
 * @param x x coordinate
 * @param y y coordinate
 * @return float value at (x, y) bilinearly interpolated from the image
 */
float bilinear_interpolation(const image::ImageView<float>& image, float x, float y);

/**
 * Performs nearest neighbor interpolation on an image.
 *
 * This function retrieves the pixel value from the input image at the provided floating-point
 * coordinates using the nearest neighbor interpolation method. This method finds the closest pixel
 * to the provided coordinates and returns its value.
 *
 * @param image An ImageView<float> object representing the input image. This image is the source
 * for the interpolation.
 * @param x The x-coordinate where the interpolation should be performed. This should be a
 * floating-point value within the image dimensions.
 * @param y The y-coordinate where the interpolation should be performed. This should be a
 * floating-point value within the image dimensions.
 *
 * @return The interpolated pixel value at the provided coordinates.
 */
float nearest_neighbor_interpolation(const image::ImageView<float>& image, float x, float y);


/**
 * Calculate an approximation of the x-th percentile of an image using a histogram.
 *
 * This function constructs a histogram of the pixel intensities in the image, then
 * approximates the x-th percentile based on this histogram. The percentile is calculated
 * by linearly interpolating the values within the bin that contains the target percentile.
 * This function assumes that the pixel intensities within each bin follow a uniform
 * distribution, and the interpolation represents this distribution reasonably well.
 *
 * @param image An ImageView<float> object representing the image. Each pixel intensity
 *              in the image should be a floating point value.
 * @param x A float between 0 and 1 representing the target percentile. For example,
 *          a value of 0.5 represents the 50th percentile (median).
 * @param num_bins (Optional) The number of bins to use for the histogram. More bins
 *                 will result in a more accurate approximation but will also require
 *                 more computational resources. Default is 1000.
 *
 * @return An approximation of the x-th percentile of the pixel intensities in the image.
 *         The return value is a floating point number representing a pixel intensity.
 */
float percentile(const image::ImageView<float>& image, const float x, const int num_bins = 1000);

} // namespace image
} // namespace lagrange
