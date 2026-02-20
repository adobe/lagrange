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
#include <lagrange/io/save_mesh.h>
#include <lagrange/primitive/generate_octahedron.h>
#include <lagrange/primitive/generate_subdivided_sphere.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/range.h>

#include <CLI/CLI.hpp>

int main(int argc, char** argv)
{
    using Scalar = float;
    using Index = uint32_t;
    struct
    {
        std::string output;
        Scalar radius = 1;
        Index num_subdivisions = 0;
    } args;

    CLI::App app{argv[0]};
    app.option_defaults()->always_capture_default();
    app.add_option("output", args.output, "Output mesh.")->required();
    app.add_option("-r,--radius", args.radius, "Radius.");
    app.add_option("-n,--num-subdivisions", args.num_subdivisions, "Number of subdivisions.");
    CLI11_PARSE(app, argc, argv)

    lagrange::primitive::OctahedronOptions setting;
    setting.radius = args.radius;
    auto mesh = lagrange::primitive::generate_octahedron<Scalar, Index>(setting);

    if (args.num_subdivisions > 0) {
        lagrange::primitive::SubdividedSphereOptions subdiv_settings;
        subdiv_settings.radius = args.radius;
        subdiv_settings.subdiv_level = args.num_subdivisions;

        mesh = lagrange::primitive::generate_subdivided_sphere(mesh, subdiv_settings);
    }

    lagrange::io::save_mesh(args.output, mesh);

    return 0;
}
