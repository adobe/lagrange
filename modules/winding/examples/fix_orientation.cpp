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

#include <lagrange/Logger.h>
#include <lagrange/io/load_mesh.h>
#include <lagrange/io/save_mesh.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/range.h>
#include <lagrange/views.h>
#include <lagrange/winding/FastWindingNumber.h>

#include <Eigen/Geometry>

#include <nlohmann/json.hpp>

#include <CLI/CLI.hpp>

int main(int argc, char** argv)
{
    using Scalar = float;
    using Index = uint32_t;
    using Mesh = lagrange::SurfaceMesh<Scalar, Index>;
    using lagrange::fs::path;

    struct
    {
        path input_path;
        path reference_path;
        path output_path;
        float epsilon = 1e-2f;
        float threshold = 8e-1f;
        int log_level = 2;
        path criterion_json_path;
    } options;

    // clang-format off
    CLI::App app{argv[0]};
    app.option_defaults()->always_capture_default();
    app.add_option("input", options.input_path, "Input mesh.")->required()->check(CLI::ExistingFile);
    app.add_option("reference", options.reference_path, "Reference mesh.")->required()->check(CLI::ExistingFile);
    app.add_option("output", options.output_path, "Output mesh.");
    app.add_option("--epsilon,-e", options.epsilon, "Sampling distance.");
    app.add_option("--threshold,-t", options.threshold, "Solid angle threshold.");
    app.add_option("--log-level,-l", options.log_level, "Log level.");
    app.add_option("--criterion-json", options.criterion_json_path, "Criterion json path.");

    CLI11_PARSE(app, argc, argv)
    // clang-format on

    auto& logger = lagrange::logger();
    logger.set_level(static_cast<spdlog::level::level_enum>(options.log_level));

    logger.info("loading input mesh \"{}\"", options.input_path.string());
    const auto input_mesh = lagrange::io::load_mesh<Mesh>(options.input_path);
    logger.info("input {}v {}f", input_mesh.get_num_vertices(), input_mesh.get_num_facets());

    logger.info("loading reference mesh \"{}\"", options.reference_path.string());
    const auto reference_mesh = lagrange::io::load_mesh<Mesh>(options.reference_path);
    logger.info(
        "reference {}v {}f",
        reference_mesh.get_num_vertices(),
        reference_mesh.get_num_facets());

    logger.info("creating fast winding number engine");
    const lagrange::winding::FastWindingNumber engine(reference_mesh);

    logger.info("checking orientation");
    logger.info("epsilon {}", options.epsilon);
    logger.info("threshold {}", options.threshold);
    auto output_mesh = input_mesh;
    size_t count_positive = 0;
    size_t count_negative = 0;
    size_t count_total = 0;
    const auto& vvs = lagrange::vertex_view(input_mesh);
    auto jj_criterions = nlohmann::json::array();
    for (const auto& ff : lagrange::range(input_mesh.get_num_facets())) {
        la_runtime_assert(input_mesh.get_facet_size(ff) == 3);
        const Eigen::Vector3f aa = vvs.row(input_mesh.get_facet_vertex(ff, 0));
        const Eigen::Vector3f bb = vvs.row(input_mesh.get_facet_vertex(ff, 1));
        const Eigen::Vector3f cc = vvs.row(input_mesh.get_facet_vertex(ff, 2));
        const auto normal = (bb - aa).cross(cc - aa).normalized();
        const auto barycenter = (aa + bb + cc) / 3;
        // logger.debug("normal [{}]", fmt::join(normal, ", "));
        // logger.debug("barycenter [{}]", fmt::join(barycenter, ", "));

        const auto pp_ = barycenter + options.epsilon * normal;
        const auto qq_ = barycenter - options.epsilon * normal;
        const std::array<float, 3> pp{pp_(0), pp_(1), pp_(2)};
        const std::array<float, 3> qq{qq_(0), qq_(1), qq_(2)};
        const auto criterion = (engine.solid_angle(pp) - engine.solid_angle(qq)) / (4.f * M_PI); // / 2 / options.epsilon;
        logger.debug("criterion {}", criterion);

        if (criterion > options.threshold) {
            const auto tt = output_mesh.ref_facet_vertices(ff);
            la_runtime_assert(tt.size() == 3);
            std::swap(tt[0], tt[1]);
        }

        if (criterion > options.threshold) count_positive++;
        if (criterion < options.threshold) count_negative++;
        count_total++;

        jj_criterions.emplace_back(criterion);
    }

    logger.info("pos {} neg {} tot {}", count_positive, count_negative, count_total);

    if (!options.criterion_json_path.empty()) {
        logger.info("saving criterion json \"{}\"", options.criterion_json_path.string());
        const auto jj = nlohmann::json{
            {"criterions", jj_criterions},
            {"epsilon", options.epsilon},
            {"threshold", options.threshold},
        };
        std::ofstream handle(options.criterion_json_path.string());
        handle << jj;
    }

    if (!options.output_path.empty()) {
        logger.info("saving mesh \"{}\"", options.output_path.string());
        lagrange::io::save_mesh(options.output_path, output_mesh);
    }

    return 0;
}
