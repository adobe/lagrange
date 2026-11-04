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
#include <lagrange/Mesh.h>
#include <lagrange/create_mesh.h>
#include <lagrange/io/save_mesh.h>
#include <lagrange/sampling/random_sample_density_blue_noise.h>

#include <CLI/CLI.hpp>

#include <pcg_random.hpp>
#include <random>

int main(int argc, char* argv[])
{
    using Cloud = lagrange::Vertices3D;
    using Index = lagrange::Triangles::Scalar;
    using Indices = Eigen::Matrix<Index, Eigen::Dynamic, 1>;
    using Densities = lagrange::TriangleMesh3D::AttributeArray;

    int number_of_sample = 4 * 1024;

    CLI::App app(argv[0]);
    app.option_defaults()->always_capture_default();
    app.add_option(
        "number-of-sample,-n,--number-of-sample",
        number_of_sample,
        "Number of sample generated.");

    CLI11_PARSE(app, argc, argv);

    lagrange::logger().info("number_of_sample {}", number_of_sample);

    const auto input = lagrange::create_quad(false);

    pcg32 rng;

    const auto ret = lagrange::sampling::random_sample_density_blue_noise(
        *input,
        number_of_sample,
        [](const Cloud& cloud, const Indices& /*indices*/) -> Densities {
            const auto mask = (cloud.rowwise().norm().array() > .75).cast<double>();
            return 1 + mask;
        },
        rng);

    const auto& status = std::get<0>(ret);
    const auto& cloud = std::get<1>(ret);
    const auto& densities = std::get<2>(ret);
    (void)status;
    (void)densities;
    assert(status == lagrange::sampling::DensityStatus::Valid);
    assert(cloud.rows() == number_of_sample);
    assert(cloud.cols() == 3);
    assert(densities.rows() == number_of_sample);
    assert(densities.cols() == 1);

    const auto output = lagrange::create_mesh(cloud, lagrange::Triangles());
    lagrange::io::save_mesh("quad_samples.obj", *output);

    return 0;
}
