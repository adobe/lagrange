/*
 * Copyright 2026 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */

#include "../examples/io_helpers.h"
#include "../shared/shared_utils.h"
#include "../src/mesh_utils.h"

#include <lagrange/image/Array3D.h>
#include <lagrange/io/load_scene.h>
#include <lagrange/solver/DirectSolver.h>
#include <lagrange/texproc/TextureRasterizer.h>
#include <lagrange/texproc/texture_compositing.h>
#include <lagrange/utils/build.h>
#include <lagrange/utils/range.h>

#include <lagrange/testing/common.h>
#include <lagrange/utils/fmt/format.h>

#include <tbb/parallel_for.h>
#include <Eigen/Sparse>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <tuple>

////////////////////////////////////////////////////////////////////////////////

namespace {

using Scalar = float;
using Index = uint32_t;

std::tuple<lagrange::texproc::mesh_utils::MeshWrapper<Scalar, Index>, unsigned int, unsigned int>
make_pumpkin_wrapper(const size_t width, const size_t height)
{
    auto scene_options = lagrange::io::LoadOptions();
    scene_options.stitch_vertices = true;
    const auto scene = lagrange::io::load_scene<lagrange::scene::Scene32f>(
        lagrange::testing::get_data_path("corp/texproc/prepared/pumpkin.glb"),
        scene_options);

    auto [mesh, _] = lagrange::scene::internal::single_mesh_from_scene(scene);

    auto wrapper = lagrange::texproc::mesh_utils::create_mesh_wrapper(
        mesh,
        lagrange::texproc::RequiresIndexedTexcoords::Yes,
        lagrange::texproc::CheckFlippedUV::Yes);

    unsigned int padded_width = static_cast<unsigned int>(width);
    unsigned int padded_height = static_cast<unsigned int>(height);
    lagrange::texproc::mesh_utils::jitter_texture(
        wrapper.texcoords,
        padded_width,
        padded_height,
        1e-4);
    auto padding =
        lagrange::texproc::mesh_utils::create_padding(wrapper, padded_width, padded_height);
    padded_width += padding.width();
    padded_height += padding.height();

    return {
        std::move(wrapper),
        padded_width,
        padded_height,
    };
}

std::tuple<lagrange::SurfaceMesh<Scalar, Index>, std::vector<std::pair<Array3Df, Array3Df>>>
rasterize_pumpkin_views(const size_t width, const size_t height)
{
    auto scene_options = lagrange::io::LoadOptions();
    scene_options.stitch_vertices = true;
    const auto scene = lagrange::io::load_scene<lagrange::scene::Scene32f>(
        lagrange::testing::get_data_path("corp/texproc/prepared/pumpkin.glb"),
        scene_options);

    auto [mesh, _] = lagrange::scene::internal::single_mesh_from_scene(scene);
    const auto cameras = lagrange::scene::internal::camera_transforms_from_scene(scene);
    REQUIRE(cameras.size() == 16);

    std::vector<Array3Df> views;
    for (const auto kk : lagrange::range(cameras.size())) {
        const auto path = lagrange::testing::get_data_path(
            lagrange::format("corp/texproc/prepared/view_{:02d}.png", kk));
        const auto view = load_image(path);
        if (!views.empty()) {
            REQUIRE(view.extent(0) == views.front().extent(0));
            REQUIRE(view.extent(1) == views.front().extent(1));
        }
        views.emplace_back(std::move(view));
    }
    REQUIRE(cameras.size() == views.size());

    auto rasterizer_options = lagrange::texproc::TextureRasterizerOptions();
    rasterizer_options.width = width;
    rasterizer_options.height = height;
    const auto rasterizer = lagrange::texproc::TextureRasterizer(mesh, rasterizer_options);

    std::vector<std::pair<Array3Df, Array3Df>> colors_and_weights(cameras.size());
    tbb::parallel_for(size_t(0), cameras.size(), [&](size_t i) {
        colors_and_weights[i] = rasterizer.weighted_texture_from_render(views[i], cameras[i]);
    });

    return {
        std::move(mesh),
        std::move(colors_and_weights),
    };
}

lagrange::image::experimental::Array3D<float> composite_with_options(
    const lagrange::SurfaceMesh<Scalar, Index>& mesh,
    const std::vector<std::pair<Array3Df, Array3Df>>& colors_and_weights,
    const lagrange::texproc::CompositingOptions& options)
{
    using WeightedTexture = lagrange::texproc::ConstWeightedTextureView<float>;
    std::vector<WeightedTexture> weighted_textures;
    weighted_textures.reserve(colors_and_weights.size());
    for (const auto& [color, weight] : colors_and_weights) {
        weighted_textures.emplace_back(
            WeightedTexture{
                color.to_mdspan(),
                weight.to_mdspan(),
            });
    }
    return lagrange::texproc::texture_compositing(mesh, weighted_textures, options);
}

void require_solver_approx(
    lagrange::span<const float> aa,
    lagrange::span<const float> bb,
    float eps_rel,
    float eps_abs)
{
    REQUIRE(aa.size() == bb.size());
    double squared_error = 0;
    double squared_reference = 0;
    float max_absolute_error = 0;
    float max_relative_error = 0;
    constexpr float relative_scale_floor = 5e-2f;
    for (size_t i = 0; i < aa.size(); ++i) {
        const float absolute_error = std::abs(aa[i] - bb[i]);
        const float scale = std::max({std::abs(aa[i]), std::abs(bb[i]), relative_scale_floor});
        max_absolute_error = std::max(max_absolute_error, absolute_error);
        max_relative_error = std::max(max_relative_error, absolute_error / scale);
        squared_error += static_cast<double>(absolute_error) * absolute_error;
        squared_reference += static_cast<double>(bb[i]) * bb[i];
    }
    const double root_mean_squared_error = std::sqrt(squared_error / aa.size());
    const double relative_l2_error = std::sqrt(squared_error / squared_reference);
    CAPTURE(root_mean_squared_error, relative_l2_error, max_absolute_error, max_relative_error);
    REQUIRE(root_mean_squared_error < eps_abs);
    REQUIRE(relative_l2_error < eps_rel);
    REQUIRE(max_absolute_error < 7e-1f);
    REQUIRE(max_relative_error < 9e-1f);
}

} // namespace

////////////////////////////////////////////////////////////////////////////////

TEST_CASE(
    "Check GradientDomain operators",
    "[texproc][regularization]" LA_SLOW_DEBUG_FLAG LA_CORP_FLAG)
{
    using namespace MishaK::TSP;

    const auto pumpkin = make_pumpkin_wrapper(128, 128);
    const auto& wrapper = std::get<0>(pumpkin);
    const auto width = std::get<1>(pumpkin);
    const auto height = std::get<2>(pumpkin);

    auto surface_corner = [&](size_t t, unsigned int k) { return wrapper.vertex_index(t, k); };
    auto surface_vertex = [&](size_t v) { return wrapper.vertex(v); };
    auto texture_corner = [&](size_t t, unsigned int k) { return wrapper.texture_index(t, k); };
    auto texture_vertex = [&](size_t v) { return wrapper.texcoord(v); };

    GradientDomain<double> gd(
        /*quadrature=*/6,
        wrapper.num_simplices(),
        wrapper.num_vertices(),
        wrapper.num_texcoords(),
        surface_corner,
        surface_vertex,
        texture_corner,
        texture_vertex,
        width,
        height,
        /*normalize=*/true,
        /*sanityCheck=*/false,
        /*regularizationWeight=*/0.0);

    const double reg_weight = 1e-3;

    Eigen::SparseMatrix<double> S0 = gd.stiffness();
    gd.applyLaplacianRegularization(reg_weight);
    Eigen::SparseMatrix<double> Sw = gd.stiffness();
    Eigen::SparseMatrix<double> Sref =
        lagrange::texproc::mesh_utils::laplacian_regularization(S0, reg_weight);

    REQUIRE(Sw.rows() == Sref.rows());
    REQUIRE(Sw.cols() == Sref.cols());

    const double error = (Sw - Sref).norm();
    CAPTURE(error);
    REQUIRE(error < 1e-7);
}

TEST_CASE(
    "Check HierarchicalGradientDomain operators",
    "[texproc][regularization]" LA_SLOW_DEBUG_FLAG LA_CORP_FLAG)
{
    using namespace MishaK::TSP;
    using Data3 = lagrange::texproc::Vector<double, 3>;
    using Solver = lagrange::solver::SolverLDLT<Eigen::SparseMatrix<double>>;

    const auto pumpkin = make_pumpkin_wrapper(128, 128);
    const auto& wrapper = std::get<0>(pumpkin);
    const auto width = std::get<1>(pumpkin);
    const auto height = std::get<2>(pumpkin);

    auto surface_corner = [&](size_t t, unsigned int k) { return wrapper.vertex_index(t, k); };
    auto surface_vertex = [&](size_t v) { return wrapper.vertex(v); };
    auto texture_corner = [&](size_t t, unsigned int k) { return wrapper.texture_index(t, k); };
    auto texture_vertex = [&](size_t v) { return wrapper.texcoord(v); };

    HierarchicalGradientDomain<double, Solver, Data3> hgd(
        /*quadrature=*/6,
        wrapper.num_simplices(),
        wrapper.num_vertices(),
        wrapper.num_texcoords(),
        surface_corner,
        surface_vertex,
        texture_corner,
        texture_vertex,
        width,
        height,
        /*levels=*/2u,
        /*normalize=*/true,
        /*sanityCheck=*/false,
        /*regularizationWeight=*/0.0);

    const double reg_weight = 1e-3;

    Eigen::SparseMatrix<double> S0 = hgd.stiffness();
    hgd.applyLaplacianRegularization(reg_weight);
    Eigen::SparseMatrix<double> Sw = hgd.stiffness();
    Eigen::SparseMatrix<double> Sref =
        lagrange::texproc::mesh_utils::laplacian_regularization(S0, reg_weight);

    REQUIRE(Sw.rows() == S0.rows());
    REQUIRE(Sw.cols() == S0.cols());
    REQUIRE(Sref.rows() == S0.rows());
    REQUIRE(Sref.cols() == S0.cols());

    const double error = (Sw - Sref).norm();
    CAPTURE(error);
    REQUIRE(error < 1e-7);
}

TEST_CASE(
    "Benchmark texture compositing solvers",
    "[texproc][regularization][!benchmark]" LA_CORP_FLAG)
{
    lagrange::ScopedLogLevel _(spdlog::level::critical);

    for (const size_t texture_size : {128, 256, 512}) {
        const auto pumpkin_views = rasterize_pumpkin_views(texture_size, texture_size);
        const auto& mesh = std::get<0>(pumpkin_views);
        const auto& colors_and_weights = std::get<1>(pumpkin_views);

        const lagrange::texproc::CompositingOptions default_options;
        auto direct_options = default_options;
        direct_options.use_direct_solver = true;

        BENCHMARK(lagrange::format("multigrid solver ({}x{})", texture_size, texture_size))
        {
            return composite_with_options(mesh, colors_and_weights, default_options);
        };

        BENCHMARK(lagrange::format("direct solver ({}x{})", texture_size, texture_size))
        {
            return composite_with_options(mesh, colors_and_weights, direct_options);
        };
    }
}

TEST_CASE(
    "Test multigrid solver options",
    "[texproc][regularization]" LA_SLOW_DEBUG_FLAG LA_CORP_FLAG)
{
    const auto& [mesh, colors_and_weights] = rasterize_pumpkin_views(128, 128);

    lagrange::texproc::CompositingOptions options_direct;
    options_direct.stiffness_regularization_weight = 1e-3;
    options_direct.use_direct_solver = true;

    lagrange::texproc::CompositingOptions options_multigrid = options_direct;
    REQUIRE(options_multigrid.stiffness_regularization_weight == 1e-3);
    options_multigrid.use_direct_solver = false;

    constexpr float eps_rel = 2e-2f;
    constexpr float eps_abs = 1e-2f;

    auto out_direct = composite_with_options(mesh, colors_and_weights, options_direct);
    REQUIRE(out_direct.extent(0) == 128);
    REQUIRE(out_direct.extent(1) == 128);

    SECTION("Low V-cycles")
    {
        options_multigrid.solver.num_v_cycles = 2;

        auto out_multigrid = composite_with_options(mesh, colors_and_weights, options_multigrid);
        REQUIRE(out_multigrid.extent(0) == 128);
        REQUIRE(out_multigrid.extent(1) == 128);

        require_solver_approx(
            lagrange::span<const float>(out_direct.data(), out_direct.size()),
            lagrange::span<const float>(out_multigrid.data(), out_multigrid.size()),
            eps_rel,
            eps_abs);
    }

    SECTION("Defaults")
    {
        REQUIRE(options_multigrid.solver.num_v_cycles == 4);

        auto out_multigrid = composite_with_options(mesh, colors_and_weights, options_multigrid);
        REQUIRE(out_multigrid.extent(0) == 128);
        REQUIRE(out_multigrid.extent(1) == 128);

        require_solver_approx(
            lagrange::span<const float>(out_direct.data(), out_direct.size()),
            lagrange::span<const float>(out_multigrid.data(), out_multigrid.size()),
            eps_rel,
            eps_abs);
    }

    SECTION("High V-cycles")
    {
        options_multigrid.solver.num_v_cycles = 6;

        auto out_multigrid = composite_with_options(mesh, colors_and_weights, options_multigrid);
        REQUIRE(out_multigrid.extent(0) == 128);
        REQUIRE(out_multigrid.extent(1) == 128);

        require_solver_approx(
            lagrange::span<const float>(out_direct.data(), out_direct.size()),
            lagrange::span<const float>(out_multigrid.data(), out_multigrid.size()),
            eps_rel,
            eps_abs);
    }
}
