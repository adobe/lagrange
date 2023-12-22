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
#include <lagrange/image/image_sampling.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/range.h>

#include <tbb/parallel_sort.h>
#include <numeric>
#include <random>

namespace lagrange {
namespace image {

void sample_from_density_map(
    const image::ImageView<float>& density_map,
    size_t n_samples,
    lagrange::Vertices2Df& samples)
{
    size_t image_width = density_map.get_view_size()[0];
    size_t image_height = density_map.get_view_size()[1];

    float cdf_val = 0.0f;
    image::ImageView<float> cdf(image_width, image_height, 1);

    for (auto h : range(image_height)) {
        for (auto w : range(image_width)) {
            cdf_val += density_map(w, h);
            cdf(w, h) = cdf_val;
        }
    }

    // The CDF does not sum to 1.0. Sample uniformly between 0 and the CDF's total.
    std::mt19937 mt(13);
    std::uniform_real_distribution<> dist(0.f, cdf_val);
    std::uniform_real_distribution<> perturbation(0.f, 0.1f);

    std::vector<float> random_floats(n_samples, 0.0f);
    for (auto i : range(n_samples)) random_floats[i] = dist(mt);
    tbb::parallel_sort(random_floats.begin(), random_floats.end());

    size_t added_samples = 0;
    samples.resize(n_samples, 2);
    size_t w = 0, h = 0;
    while (added_samples < n_samples && w < image_width && h < image_height) {
        if (random_floats[added_samples] <= cdf(w, h)) {
            float pert_x = perturbation(mt);
            float pert_y = perturbation(mt);
            samples.row(added_samples) << static_cast<float>(w) + pert_x,
                static_cast<float>(h) + pert_y;
            added_samples += 1;
        } else {
            w++;
            if (w >= image_width) {
                w = 0;
                h++;
            }
        }
    }
}

void sample_borders(
    const image::ImageView<float>& density_map,
    size_t n_samples,
    lagrange::Vertices2Df& samples,
    SampleType type)
{
    if (type == SampleType::Regular) {
        regular_sample_borders(density_map, n_samples, samples);
    } else {
        density_sample_borders(density_map, n_samples, samples);
    }
}


void regular_sample_borders(
    const image::ImageView<float>& density_map,
    size_t n_samples,
    lagrange::Vertices2Df& samples)
{
    size_t image_width = density_map.get_view_size()[0];
    size_t image_height = density_map.get_view_size()[1];

    n_samples = std::max(n_samples, size_t{4});
    size_t step = std::max((image_width + image_height) / (n_samples - 3), size_t{1});

    std::vector<Eigen::Vector2f> vec_samples;
    vec_samples.emplace_back(0.0f, 0.0f);
    vec_samples.emplace_back(0.0f, image_height - 1);
    vec_samples.emplace_back(image_width - 1, 0.0f);
    vec_samples.emplace_back(image_width - 1, image_height - 1);

    for (size_t w = step; w < image_width - 1; w += step) {
        vec_samples.emplace_back(w, 0.0f);
        vec_samples.emplace_back(w, image_height - 1);
    }
    for (size_t h = step; h < image_height - 1; h += step) {
        vec_samples.emplace_back(0.0f, h);
        vec_samples.emplace_back(image_width - 1, h);
    }

    samples.resize(vec_samples.size(), 2);
    for (auto i : range(vec_samples.size())) {
        samples.row(i) = vec_samples[i];
    }
}

void density_sample_borders(
    const image::ImageView<float>& density_map,
    size_t n_samples,
    lagrange::Vertices2Df& samples)
{
    la_runtime_assert(n_samples >= 4, "Need at least 4 samples for the corners.");

    size_t image_width = density_map.get_view_size()[0];
    size_t image_height = density_map.get_view_size()[1];

    size_t n_samples_non_corners = n_samples - 4;

    float cdf_val = 0.0f;
    image::ImageView<float> cdf(image_width, image_height, 1);

    // Compute CDF for the border pixels.
    for (auto h : range(image_height)) {
        for (auto w : range(image_width)) {
            // Only consider the border pixels that are not corners.
            // First clause checks if pixel is in borders, the remaining ones check if it is a
            // corner.
            if ((h == 0 || h == image_height - 1 || w == 0 || w == image_width - 1) &&
                !(w == 0 && h == 0) && !(w == 0 && h == image_height - 1) &&
                !(w == image_width - 1 && h == 0) &&
                !(w == image_width - 1 && h == image_height - 1)) {
                cdf_val += density_map(w, h) + 1e-2f; // Add a small constant to avoid zero density.
                cdf(w, h) = cdf_val;
            }
        }
    }

    // The CDF does not sum to 1.0. Sample uniformly between 0 and the CDF's total.
    std::mt19937 mt(13);
    std::uniform_real_distribution<> dist(0.f, cdf_val);
    std::uniform_real_distribution<> perturbation(-0.5f, 0.5f);

    std::vector<float> random_floats(n_samples_non_corners, 0.0f);
    for (auto i : range(n_samples_non_corners)) random_floats[i] = dist(mt);
    tbb::parallel_sort(random_floats.begin(), random_floats.end());

    size_t added_samples = 0;
    samples.resize(n_samples, 2);
    size_t w = 0, h = 0;
    while (added_samples < n_samples_non_corners && h < image_height) {
        // Only consider the border pixels that are not corners.
        // First clause checks if pixel is in borders, the remaining ones check if it is a corner.
        if ((w == 0 || w == image_width - 1 || h == 0 || h == image_height - 1) &&
            !(w == 0 && h == 0) && !(w == 0 && h == image_height - 1) &&
            !(w == image_width - 1 && h == 0) && !(w == image_width - 1 && h == image_height - 1)) {
            if (random_floats[added_samples] <= cdf(w, h)) {
                float pert_x = perturbation(mt);
                float pert_y = perturbation(mt);

                // Carefully constrain the perturbation for the border pixels.
                if (w == 0)
                    pert_x = std::max(0.0f, pert_x);
                else if (w == image_width - 1)
                    pert_x = std::min(0.0f, pert_x);

                if (h == 0)
                    pert_y = std::max(0.0f, pert_y);
                else if (h == image_height - 1)
                    pert_y = std::min(0.0f, pert_y);

                float interpolated_x = static_cast<float>(w) + pert_x;
                float interpolated_y = static_cast<float>(h) + pert_y;

                samples.row(added_samples) << interpolated_x, interpolated_y;
                added_samples += 1;
            }
        }

        // Traverse the image in a clockwise manner starting from the top left corner.
        if (h == 0 && w < image_width - 1) {
            w++;
        } else if (w == image_width - 1 && h < image_height - 1) {
            h++;
        } else if (h == image_height - 1 && w > 0) {
            w--;
        } else if (w == 0 && h > 0) {
            h--;
        }
    }

    // Add the corners.
    samples.row(n_samples - 4) << 0.0f, 0.0f;
    samples.row(n_samples - 3) << static_cast<float>(image_width - 1), 0.0f;
    samples.row(n_samples - 2) << static_cast<float>(image_width - 1),
        static_cast<float>(image_height - 1);
    samples.row(n_samples - 1) << 0.0f, static_cast<float>(image_height - 1);
}


float bilinear_interpolation(const image::ImageView<float>& image, float x, float y)
{
    la_runtime_assert(x >= 0.0f && x < image.get_view_size()[0]);
    la_runtime_assert(y >= 0.0f && y < image.get_view_size()[1]);

    // Clamp x and y to the maximum valid index for interpolation.
    x = std::min(x, static_cast<float>(image.get_view_size()[0] - 2));
    y = std::min(y, static_cast<float>(image.get_view_size()[1] - 2));

    int x1 = std::floor(x);
    int y1 = std::floor(y);
    int x2 = x1 + 1;
    int y2 = y1 + 1;

    float r1 = (x2 - x) * image(x1, y1) + (x - x1) * image(x2, y1);
    float r2 = (x2 - x) * image(x1, y2) + (x - x1) * image(x2, y2);

    return (y2 - y) * r1 + (y - y1) * r2;
}

float nearest_neighbor_interpolation(const image::ImageView<float>& image, float x, float y)
{
    la_runtime_assert(x >= 0.0f && x < image.get_view_size()[0]);
    la_runtime_assert(y >= 0.0f && y < image.get_view_size()[1]);

    // Round the coordinates to nearest integer to find the closest pixel.
    int nearest_x = std::round(x);
    int nearest_y = std::round(y);

    // Clamp to ensure within image bounds.
    nearest_x = std::clamp(nearest_x, 0, (int)image.get_view_size()[0] - 1);
    nearest_y = std::clamp(nearest_y, 0, (int)image.get_view_size()[1] - 1);

    return image(nearest_x, nearest_y);
}

float percentile(const image::ImageView<float>& image, const float x, const int num_bins)
{
    std::vector<int> histogram(num_bins, 0);
    const auto size = image.get_view_size();
    int total_pixels = size[0] * size[1];

    float min_val = image(0, 0);
    float max_val = image(0, 0);

    for (auto i : range(size[0])) {
        for (auto j : range(size[1])) {
            float pixel = image(i, j);
            min_val = std::min(min_val, pixel);
            max_val = std::max(max_val, pixel);
        }
    }

    float bin_width = (max_val - min_val) / num_bins;

    for (auto i : range(size[0])) {
        for (auto j : range(size[1])) {
            int bin = static_cast<int>((image(i, j) - min_val) / bin_width);
            bin = std::clamp(bin, 0, num_bins - 1);
            histogram[bin]++;
        }
    }

    std::vector<int> cumulative_histogram(histogram.size());
    std::partial_sum(histogram.begin(), histogram.end(), cumulative_histogram.begin());

    int target_count = std::round(x * total_pixels);
    auto it =
        std::lower_bound(cumulative_histogram.begin(), cumulative_histogram.end(), target_count);
    int percentile_bin = std::distance(cumulative_histogram.begin(), it);

    // Perform linear interpolation within the bin.
    float bin_min_val = min_val + percentile_bin * bin_width;
    float bin_max_val = bin_min_val + bin_width;
    int bin_count =
        (percentile_bin > 0
             ? cumulative_histogram[percentile_bin] - cumulative_histogram[percentile_bin - 1]
             : cumulative_histogram[0]);
    float bin_ratio =
        static_cast<float>(
            target_count - (percentile_bin > 0 ? cumulative_histogram[percentile_bin - 1] : 0)) /
        bin_count;
    return bin_min_val + bin_ratio * (bin_max_val - bin_min_val);
}

} // namespace image
} // namespace lagrange
