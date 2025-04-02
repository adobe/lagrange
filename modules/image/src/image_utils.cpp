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
#include <lagrange/image/image_utils.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/range.h>

namespace lagrange {
namespace image {

float image_standard_deviation(const image::ImageView<float>& image)
{
    Eigen::Vector2<size_t> size = image.get_view_size();
    float n_pixels = size.sum();

    float mean = 0.0f;
    for (auto h : range(size[1])) {
        for (auto w : range(size[0])) {
            mean += image(w, h);
        }
    }
    mean = mean / n_pixels;

    float std = 0.0f;
    for (auto h : range(size[1])) {
        for (auto w : range(size[0])) {
            std += (image(w, h) - mean) * (image(w, h) - mean);
        }
    }
    std = std::sqrt(std / n_pixels);

    return std;
}

void depth_to_disparity(
    const image::ImageView<float>& image,
    float vfov_degrees,
    image::ImageView<float>& result)
{
    // Assume the image width is the same as the sensor width
    const auto size = image.get_view_size();
    const float focal_length = size[1] / (2.0f * std::tan(vfov_degrees * M_PI / 180.0f));

    // Check if the result image has the same size as the input image
    la_runtime_assert(
        result.get_view_size()[0] == size[0] && result.get_view_size()[1] == size[1],
        "Result image size does not match input image size.");

    for (auto i : range(size[0])) {
        for (auto j : range(size[1])) {
            float depth = image(i, j);
            if (depth > 0) {
                result(i, j) = focal_length / depth;
            } else {
                result(i, j) = 0;
            }
        }
    }
}

void normalize_max_image(const image::ImageView<float>& image, image::ImageView<float>& result) {
    // asset the result image has the same size as the input image
    la_runtime_assert(
        result.get_view_size()[0] == image.get_view_size()[0] &&
        result.get_view_size()[1] == image.get_view_size()[1],
        "Result image size does not match input image size.");

    float max_value = image(0, 0);
    for (auto i : range(image.get_view_size()[0])) {
        for (auto j : range(image.get_view_size()[1])) {
            max_value = std::max(max_value, image(i, j));
        }
    }

    for (auto i : range(image.get_view_size()[0])) {
        for (auto j : range(image.get_view_size()[1])) {
            result(i, j) = image(i, j) / max_value;
        }
    }
}

ImageHistogram create_image_histogram(const image::ImageView<float>& image, const int num_bins)
{
    // Initialize min and max values
    float min_value = image(0, 0);
    float max_value = image(0, 0);

    // Find the min and max pixel values in the image
    for (auto i : range(image.get_view_size()[0])) {
        for (auto j : range(image.get_view_size()[1])) {
            min_value = std::min(min_value, image(i, j));
            max_value = std::max(max_value, image(i, j));
        }
    }

    // Calculate the width of each bin
    float bin_width = (max_value - min_value) / num_bins;

    // Create an empty histogram with the specified number of bins
    ImageHistogram histogram;
    histogram.counts.resize(num_bins, 0);
    histogram.boundaries.resize(num_bins + 1);
    histogram.min_value = min_value;
    histogram.max_value = max_value;

    // Generate the bin boundaries
    for (auto i : range(num_bins + 1)) {
        histogram.boundaries[i] = min_value + i * bin_width;
    }

    // Populate the histogram
    for (auto i : range(image.get_view_size()[0])) {
        for (auto j : range(image.get_view_size()[1])) {
            // Calculate which bin this pixel belongs to
            int bin = static_cast<int>((image(i, j) - min_value) / bin_width);

            // Make sure the bin is within the histogram range (in case of rounding errors)
            bin = std::clamp(bin, 0, num_bins - 1);

            // Increment the count for this bin
            ++histogram.counts[bin];
        }
    }

    return histogram;
}

float compute_otsu_threshold(const ImageHistogram& histogram, float upper_limit)
{
    // Find the bin that corresponds to the upper_limit
    size_t upper_limit_bin = 0;
    for (auto i : range(histogram.boundaries.size())) {
        if (histogram.boundaries[i] > upper_limit) {
            upper_limit_bin = i;
            break;
        }
    }

    int total_pixel_count = 0;
    for (auto i : range(upper_limit_bin)) {
        total_pixel_count += histogram.counts[i];
    }

    float weighted_sum = 0;
    for (auto i : range(upper_limit_bin)) {
        weighted_sum += i * histogram.counts[i];
    }

    float background_weighted_sum = 0;
    int background_pixel_count = 0;
    float max_variance = 0;
    float optimal_threshold = 0;

    for (auto i : range(upper_limit_bin)) {
        background_pixel_count += histogram.counts[i];
        if (background_pixel_count == 0) continue;

        int foreground_pixel_count = total_pixel_count - background_pixel_count;
        if (foreground_pixel_count == 0) break;

        background_weighted_sum += static_cast<float>(i) * histogram.counts[i];

        float background_mean = background_weighted_sum / background_pixel_count;
        float foreground_mean = (weighted_sum - background_weighted_sum) / foreground_pixel_count;

        // Calculate Between Class Variance
        float between_class_variance = static_cast<float>(background_pixel_count) *
                                       static_cast<float>(foreground_pixel_count) *
                                       (background_mean - foreground_mean) *
                                       (background_mean - foreground_mean);

        // Check if new maximum found
        if (between_class_variance > max_variance) {
            max_variance = between_class_variance;
            optimal_threshold = i;
        }
    }

    // Convert the bin index to the actual value
    float bin_width = (histogram.max_value - histogram.min_value) /
                      static_cast<float>(histogram.boundaries.size());
    float actual_threshold = histogram.min_value + optimal_threshold * bin_width;

    return actual_threshold;
}


} // namespace image
} // namespace lagrange
