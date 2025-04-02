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
#include <lagrange/testing/common.h>

#include <igl/readDMAT.h>
#include <igl/writeDMAT.h>
#include <lagrange/common.h>
#include <lagrange/image/image_sampling.h>
#include <lagrange/utils/range.h>

namespace {
template <typename MatrixA, typename MatrixB>
void is_same(const MatrixA& A, const MatrixB& B)
{
    REQUIRE(A.rows() == B.rows());
    REQUIRE(A.cols() == B.cols());
    REQUIRE(A == B);
}
} // namespace

TEST_CASE("Sample from density map", "[image]" LA_CORP_FLAG)
{
    // Creates uniform density map
    lagrange::image::ImageView<float> density_map(200, 200, 1);
    for (auto i : lagrange::range(200)) {
        for (auto j : lagrange::range(200)) {
            density_map(i, j) = 1.f / (200.f * 200.f);
        }
    }

    lagrange::Vertices2Df samples;
    lagrange::image::sample_from_density_map(density_map, 1000, samples);

    lagrange::fs::path gt_samples_path =
        lagrange::testing::get_data_path("corp/displacement/uniform_samples1k.dmat");

    lagrange::Vertices2Df gt_samples;

    bool load_success = igl::readDMAT(gt_samples_path.string(), gt_samples);
    REQUIRE(load_success);

    is_same(samples, gt_samples);
}

TEST_CASE("Sample borders (regularly)", "[image]" LA_CORP_FLAG)
{
    // Creates uniform density map
    lagrange::image::ImageView<float> density_map(200, 200, 1);
    for (auto i : lagrange::range(200)) {
        for (auto j : lagrange::range(200)) {
            density_map(i, j) = 1.0f;
        }
    }

    lagrange::Vertices2Df samples;
    lagrange::image::sample_borders(density_map, 1000, samples);

    lagrange::fs::path gt_samples_path =
        lagrange::testing::get_data_path("corp/displacement/border_samples1k.dmat");

    lagrange::Vertices2Df gt_samples;

    bool load_success = igl::readDMAT(gt_samples_path.string(), gt_samples);
    REQUIRE(load_success);

    is_same(samples, gt_samples);
}

TEST_CASE("Sample borders (density)", "[image]" LA_CORP_FLAG)
{
    // Creates uniform density map
    lagrange::image::ImageView<float> density_map(200, 200, 1);
    for (auto i : lagrange::range(200)) {
        for (auto j : lagrange::range(200)) {
            density_map(i, j) = 1.0f;
        }
    }

    lagrange::Vertices2Df samples;
    lagrange::image::sample_borders(
        density_map,
        1000,
        samples,
        lagrange::image::SampleType::Density);

    lagrange::fs::path gt_samples_path =
        lagrange::testing::get_data_path("corp/displacement/density_border_samples1k.dmat");

    lagrange::Vertices2Df gt_samples;

    bool load_success = igl::readDMAT(gt_samples_path.string(), gt_samples);
    REQUIRE(load_success);

    is_same(samples, gt_samples);
}
