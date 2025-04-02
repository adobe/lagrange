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

#include <lagrange/common.h>
#include <lagrange/image/ImageView.h>

namespace lagrange {
namespace image {

float image_standard_deviation(const image::ImageView<float>& image);

/**
 * Converts a depth image to a disparity image.
 *
 * This function converts a depth image, where each pixel's value represents the depth from the
 * camera,
 * into a disparity image. The disparity is calculated using the formula depth = focal_length / disparity.
 * This conversion is particularly useful in stereo vision and 3D reconstruction tasks.
 *
 * @param image An ImageView<float> object representing the depth image. Each pixel's value
 *              in the image should represent the depth from the camera to the object in the scene.
 * @param vfov The vertical field of view (VFOV) of the camera in degrees. This parameter is used to
 *             calculate the focal length, assuming the sensor's height is the same as the image's
 * height.
 * @param result An ImageView<float> object where the result disparity image will be stored. It must
 * have the same size as the input image, otherwise the function will throw an exception.
 *
 * @warning This function assumes that the sizes of the input image and the result image are the
 * same. If they are not, it will call la_runtime_assert. Make sure that the input image and the
 * result image have the same size before calling this function.
 *
 * @note If the depth at a pixel is zero or negative, the disparity at that pixel will be set to
 * zero. Depending on your application, you may want to handle this situation differently.
 */
void depth_to_disparity(
    const image::ImageView<float>& image,
    float vfov,
    image::ImageView<float>& result);


/**
 * Normalizes an image in a way that the maximum value is 1.0. This is done by dividing each
 * pixel by the maximum value.
 *
 * @param image input image
 * @param result output image
 */
void normalize_max_image(const image::ImageView<float>& image, image::ImageView<float>& result);

/**
 * A struct for storing an image histogram.
 *
 * ImageHistogram struct contains the counts of each bin,
 * the boundaries of each bin and the overall minimum and
 * maximum values of the image.
 */
struct ImageHistogram
{
    std::vector<int> counts; ///< Counts of each histogram bin
    std::vector<float> boundaries; ///< Boundaries of each bin
    float min_value; ///< Minimum pixel value in the image
    float max_value; ///< Maximum pixel value in the image
};


/**
 * Function to create an image histogram.
 *
 * This function generates a histogram of the input image with a specified
 * number of bins. The minimum and maximum values of the image are used to
 * define the range of the histogram. Each bin's boundaries are then calculated
 * based on this range, and the histogram counts are populated by iterating
 * through the image and incrementing the count of the corresponding bin for each pixel.
 *
 * @param image The image to create the histogram from.
 * @param num_bins The number of bins in the histogram.
 * @return An ImageHistogram struct containing the histogram counts, bin boundaries,
 *         and the overall minimum and maximum pixel values of the image.
 */
ImageHistogram create_image_histogram(const image::ImageView<float>& image, int num_bins);

/**
 * Computes the Otsu threshold value of a given histogram.
 *
 * Otsu's method is a way to automatically perform histogram shape-based image thresholding,
 * or, the reduction of a graylevel image to a binary image. It assumes that the image to be
 * thresholded contains two classes of pixels (e.g. foreground and background), it then calculates
 * the optimum threshold separating those two classes so that their combined spread (intraclass
 * variance) is minimal.
 *
 * This implementation has been adapted to consider only a specific range of the histogram,
 * up to a given upper limit. This modification is useful for scenarios where we want to exclude
 * certain values (such as near-zero values representing the sky in a disparity
 * image) from the thresholding process. The upper_limit parameter determines the highest value to
 * be considered in the threshold calculation.
 *
 * The computed threshold is a bin index; to convert this to an actual pixel value, the function
 * multiplies the threshold by the bin width and adds the minimum pixel value of the histogram.
 *
 * @param histogram The histogram of the image. It must contain equally-spaced bins, and the bin
 * counts must be non-negative.
 * @param upper_limit The maximum pixel value to consider in the threshold calculation. This should
 * be a value within the range of the histogram.
 *
 * @return The Otsu threshold, computed based on the given histogram and upper limit.
 *
 * For a detailed explanation of Otsu's method, see:
 * Otsu, N. (1979). A Threshold Selection Method from Gray-Level Histograms.
 * IEEE Transactions on Systems, Man, and Cybernetics, 9(1), 62–66. doi:10.1109/TSMC.1979.4310076
 */
float compute_otsu_threshold(const ImageHistogram& histogram, float upper_limit);

} // namespace image
} // namespace lagrange
