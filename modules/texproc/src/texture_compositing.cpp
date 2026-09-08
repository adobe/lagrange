/*
 * Copyright 2025 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */

#include <lagrange/texproc/texture_compositing.h>

#include "mesh_utils.h"

#include <lagrange/solver/DirectSolver.h>
#include <lagrange/utils/build.h>
#include <lagrange/utils/range.h>
#include <lagrange/utils/timing.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <Src/PreProcessing.h>
#include <Src/GradientDomain.h>
#include <lagrange/utils/warnon.h>
#include <lagrange/utils/fmt/format.h>
// clang-format on

#include <Eigen/Sparse>

namespace lagrange::texproc {

namespace {

using namespace MishaK::TSP;
using Solver = lagrange::solver::SolverLDLT<Eigen::SparseMatrix<double>>;

template <typename Scalar>
bool is_exactly_zero(Scalar x)
{
    return std::abs(x) < std::numeric_limits<Scalar>::denorm_min();
}

template <unsigned int NumChannels>
struct InputData
{
    RegularGrid<K, Vector<double, NumChannels>> texture;
    RegularGrid<K, Vector<double, 1>> weights;
};

// Load input textures/weights into RegularGrids and apply jitter and padding.
template <unsigned int NumChannels, typename Scalar, typename Index, typename ValueType>
std::tuple<std::vector<InputData<NumChannels>>, Padding, unsigned int, unsigned int>
copy_jitter_and_pad(
    const std::vector<ConstWeightedTextureView<ValueType>>& textures,
    mesh_utils::MeshWrapper<Scalar, Index>& wrapper,
    double jitter_epsilon)
{
    la_runtime_assert(!textures.empty());

    std::vector<InputData<NumChannels>> in(textures.size());
    auto width = static_cast<unsigned int>(textures[0].texture.extent(0));
    auto height = static_cast<unsigned int>(textures[0].texture.extent(1));
    for (size_t i = 0; i < textures.size(); i++) {
        la_debug_assert(textures[i].texture.extent(0) == width);
        la_debug_assert(textures[i].texture.extent(1) == height);
        la_debug_assert(textures[i].texture.extent(2) == NumChannels);
        la_debug_assert(textures[i].weights.extent(0) == width);
        la_debug_assert(textures[i].weights.extent(1) == height);
        la_debug_assert(textures[i].weights.extent(2) == 1);
        mesh_utils::set_grid(textures[i].texture, in[i].texture);
        mesh_utils::set_grid(textures[i].weights, in[i].weights);
    }

    mesh_utils::jitter_texture(wrapper.texcoords, width, height, jitter_epsilon);

    Padding padding = mesh_utils::create_padding(wrapper, width, height);
    tbb::parallel_for(size_t(0), textures.size(), [&](size_t i) {
        padding.pad(in[i].texture);
        padding.pad(in[i].weights);
    });
    width += padding.width();
    height += padding.height();

    return {
        std::move(in),
        padding,
        width,
        height,
    };
}

// Compute the weighted-average value target at each node,
// fill unobserved texels with the average observed color.
// Returns (weight_colors, weight_sums) where weight_sums[n] = Σ_i weights_i(n).
template <unsigned int NumChannels>
std::pair<std::vector<Vector<double, NumChannels>>, std::vector<double>> compute_value_target(
    const GradientDomain<double>& gd,
    const std::vector<InputData<NumChannels>>& in)
{
    // Compute weighted texel values.
    std::vector<double> weight_sums(gd.numNodes());
    std::vector<Vector<double, NumChannels>> weight_colors(gd.numNodes());
    for (const auto nn : range(gd.numNodes())) {
        auto [row, col] = gd.node(nn);
        double weight_sum = 0;
        for (const auto& ii : in) {
            weight_sum += ii.weights(row, col)[0];
        }
        weight_sums[nn] = weight_sum;
        const double scale = weight_sum > 0 ? 1. / weight_sum : 1.;
        for (const auto& ii : in) {
            weight_colors[nn] += ii.texture(row, col) * ii.weights(row, col)[0] * scale;
        }
    }

    // Compute the average observed color.
    Vector<double, NumChannels> mean_observed_color;
    size_t num_observed_nodes = 0;
    for (const auto nn : range(gd.numNodes())) {
        if (!is_exactly_zero(weight_sums[nn])) {
            mean_observed_color += weight_colors[nn];
            num_observed_nodes++;
        }
    }
    if (num_observed_nodes > 0) {
        mean_observed_color /= static_cast<double>(num_observed_nodes);
    }
    la_debug_assert(num_observed_nodes != 0 || mean_observed_color.squareNorm() == 0.0);

    // Set unobserved texels to the average observed color.
    if (num_observed_nodes < gd.numNodes()) {
        logger().warn(
            "Found {} unobserved texels. "
            "Setting target values to the average observed color.",
            gd.numNodes() - num_observed_nodes);
        for (const auto nn : range(gd.numNodes())) {
            if (is_exactly_zero(weight_sums[nn])) {
                weight_colors[nn] = mean_observed_color;
            }
        }
    }

    return {
        std::move(weight_colors),
        std::move(weight_sums),
    };
}

// Compute the gradient constraints using the selected normalization mode.
template <unsigned int NumChannels>
std::vector<Vector<double, NumChannels>> compute_edge_differences(
    const GradientDomain<double>& gd,
    const std::vector<InputData<NumChannels>>& in,
    const std::vector<double>& weight_sums,
    const GradientNormalization mode,
    const bool smooth_low_weight_areas)
{
    std::vector<Vector<double, NumChannels>> edge_differences(gd.numEdges());
    std::vector<double> edge_weight_sums(gd.numEdges(), 0.0);

    auto compute_node_scale = [&](size_t n) {
        if (smooth_low_weight_areas && weight_sums[n] < 1.0) {
            return 1.0;
        }
        return weight_sums[n] > 0 ? 1.0 / weight_sums[n] : 1.0;
    };

    auto compute_edge_weight = [&](size_t n1, size_t n2, double w1, double w2) -> double {
        switch (mode) {
        case GradientNormalization::PerEdge: return w1 * w2; break;
        case GradientNormalization::PerTexelSqrt:
            return std::sqrt(w1 * compute_node_scale(n1) * w2 * compute_node_scale(n2));
            break;
        default:
            la_debug_assert(false);
            return 0.0;
            break;
        }
    };

    for (const auto ee : range(gd.numEdges())) {
        const std::pair<size_t, size_t> end_points = gd.edge(ee);
        const auto [row1, col1] = gd.node(end_points.first);
        const auto [row2, col2] = gd.node(end_points.second);
        for (const auto ii : range(in.size())) {
            const double w1 = in[ii].weights(row1, col1)[0];
            const double w2 = in[ii].weights(row2, col2)[0];
            const double edge_weight =
                compute_edge_weight(end_points.first, end_points.second, w1, w2);
            if (edge_weight > 0) {
                edge_differences[ee] +=
                    (in[ii].texture(row2, col2) - in[ii].texture(row1, col1)) * edge_weight;
                if (mode == GradientNormalization::PerEdge) {
                    edge_weight_sums[ee] += edge_weight;
                }
            }
        }
    }

    if (mode == GradientNormalization::PerEdge) {
        for (const auto ee : range(gd.numEdges())) {
            if (edge_weight_sums[ee] > 0) edge_differences[ee] /= edge_weight_sums[ee];
        }
    }

    return edge_differences;
}

// Direct LDLT solve with Laplacian regularization of the stiffness matrix.
// Modifies xx in place.
template <unsigned int NumChannels>
void solve_direct(
    const GradientDomain<double>& gd,
    const std::vector<Vector<double, NumChannels>>& edge_differences,
    const double value_weight,
    const double gradient_weight,
    const double regularization_weight,
    std::vector<Vector<double, NumChannels>>& xx)
{
    la_runtime_assert(gd.numNodes() > 0);
    la_runtime_assert(edge_differences.size() == gd.numEdges());

    const Eigen::SparseMatrix<double> M = gd.mass();
    const Eigen::SparseMatrix<double> S_reg =
        mesh_utils::laplacian_regularization(gd.stiffness(), regularization_weight);
    const Eigen::SparseMatrix<double> A = value_weight * M + gradient_weight * S_reg;

    Solver solver(A);
    la_runtime_assert(solver.info() == Eigen::Success, "Failed to factor system matrix");

    std::vector<Vector<double, NumChannels>> gradient_b(gd.numNodes());
    gd.divergence(edge_differences.data(), gradient_b.data());

    Eigen::VectorXd x_c(gd.numNodes());
    Eigen::VectorXd gradient_b_c(gd.numNodes());
    for (const auto cc : range(NumChannels)) {
        for (const auto nn : range(gd.numNodes())) {
            x_c[nn] = xx[nn][cc];
            gradient_b_c[nn] = gradient_b[nn][cc];
        }
        Eigen::VectorXd b_c = value_weight * (M * x_c) + gradient_weight * gradient_b_c;
        Eigen::VectorXd sol = solver.solve(b_c);
        for (const auto nn : range(gd.numNodes())) {
            xx[nn][cc] = sol[nn];
        }
    }
}

// Multigrid v-cycle solve.
// Modifies xx in place.
template <unsigned int NumChannels>
void solve_multigrid(
    HierarchicalGradientDomain<double, Solver, Vector<double, NumChannels>>& hgd,
    const std::vector<Vector<double, NumChannels>>& edge_differences,
    const double value_weight,
    const double gradient_weight,
    const CompositingOptions::SolverOptions& options,
    std::vector<Vector<double, NumChannels>>& xx)
{
    la_runtime_assert(options.num_gauss_seidel_iterations > 0);

    la_runtime_assert(hgd.numNodes() > 0);
    la_runtime_assert(edge_differences.size() == hgd.numEdges());

    span<Vector<double, NumChannels>> hx{hgd.x(), hgd.numNodes()};
    span<Vector<double, NumChannels>> hb{hgd.b(), hgd.numNodes()};
    for (const auto nn : range(hgd.numNodes())) {
        hx[nn] = xx[nn];
    }

    std::vector<Vector<double, NumChannels>> value_b(hgd.numNodes());
    std::vector<Vector<double, NumChannels>> gradient_b(hgd.numNodes());

    hgd.mass(hx.data(), value_b.data());
    hgd.divergence(edge_differences.data(), gradient_b.data());
    for (const auto nn : range(hgd.numNodes())) {
        hb[nn] = value_weight * value_b[nn] + gradient_weight * gradient_b[nn];
    }

    hgd.updateSystem(value_weight, gradient_weight);
    for ([[maybe_unused]] const auto vv : range(options.num_v_cycles)) {
        // The upstream code alternates between boundary and interior updates, so it needs an even
        // number of iterations to converge:
        // https://github.com/mkazhdan/TextureSignalProcessing/blob/dd7ab66cc0e75bec1e6eb6b704d240e8780f46e7/include/Src/IterativeSolvers.inl#L447
        hgd.vCycle(options.num_gauss_seidel_iterations * 2);
    }

    for (const auto nn : range(hgd.numNodes())) {
        xx[nn] = hx[nn];
    }
}

// Write the solved texel values back into an Array3D output, undoing padding.
template <unsigned int NumChannels, typename ValueType>
image::experimental::Array3D<ValueType> unpad_and_copy(
    const GradientDomain<double>& gd,
    const std::vector<Vector<double, NumChannels>>& x,
    const Padding padding,
    const unsigned int width,
    const unsigned int height)
{
    RegularGrid<K, Vector<double, NumChannels>> grid;
    grid.resize(width, height);
    for (const auto nn : range(gd.numNodes())) {
        const auto [row, col] = gd.node(nn);
        grid(row, col) = x[nn];
    }
    padding.unpad(grid);

    const unsigned int width_ = width - padding.width();
    const unsigned int height_ = height - padding.height();
    la_debug_assert(grid.res(0) == width_);
    la_debug_assert(grid.res(1) == height_);
    auto composite = image::experimental::create_image<ValueType>(width_, height_, NumChannels);
    // TODO: The two grids are transposed in memory. Measure and check if tiling the copy over 32x32
    // blocks is faster.
    for (const auto ii : range(width_)) {
        for (const auto jj : range(height_)) {
            for (const auto cc : range(NumChannels)) {
                composite(ii, jj, cc) = grid(ii, jj)[cc];
            }
        }
    }

    return composite;
}

template <unsigned int NumChannels, typename Scalar, typename Index, typename ValueType>
image::experimental::Array3D<ValueType> texture_compositing(
    const SurfaceMesh<Scalar, Index>& mesh,
    std::vector<ConstWeightedTextureView<ValueType>> textures,
    const CompositingOptions& options)
{
    VerboseTimer timer("[compositing] ");

    timer.tick();
    auto wrapper =
        mesh_utils::create_mesh_wrapper(mesh, RequiresIndexedTexcoords::Yes, CheckFlippedUV::Yes);
    auto [in, padding, width, height] =
        copy_jitter_and_pad<NumChannels>(textures, wrapper, options.jitter_epsilon);
    timer.tock("preprocessing");

    // HierarchicalGradientDomain inherits from GradientDomain — construct only one to avoid
    // duplicating the expensive operator initialization.
    const bool normalize = true;
#if LAGRANGE_TARGET_BUILD_TYPE(DEBUG)
    const bool sanity_check = options.sanity_check.value_or(true);
#else
    const bool sanity_check = options.sanity_check.value_or(false);
#endif
    auto surface_corner = [&](size_t t, unsigned int k) { return wrapper.vertex_index(t, k); };
    auto surface_vertex = [&](size_t v) { return wrapper.vertex(v); };
    auto texture_corner = [&](size_t t, unsigned int k) { return wrapper.texture_index(t, k); };
    auto texture_vertex = [&](size_t v) { return wrapper.texcoord(v); };

    timer.tick();
    using GradDomain = GradientDomain<double>;
    using HierGradDomain = HierarchicalGradientDomain<double, Solver, Vector<double, NumChannels>>;
    std::optional<GradDomain> gd_storage;
    std::optional<HierGradDomain> hgd_storage;
    if (options.use_direct_solver) {
        gd_storage.emplace(GradDomain(
            options.quadrature_samples,
            wrapper.num_simplices(),
            wrapper.num_vertices(),
            wrapper.num_texcoords(),
            surface_corner,
            surface_vertex,
            texture_corner,
            texture_vertex,
            width,
            height,
            normalize,
            sanity_check));
    } else {
        hgd_storage.emplace(HierGradDomain(
            options.quadrature_samples,
            wrapper.num_simplices(),
            wrapper.num_vertices(),
            wrapper.num_texcoords(),
            surface_corner,
            surface_vertex,
            texture_corner,
            texture_vertex,
            width,
            height,
            options.solver.num_multigrid_levels,
            normalize,
            sanity_check));
    }
    GradDomain& gd = options.use_direct_solver ? static_cast<GradDomain&>(*gd_storage)
                                               : static_cast<GradDomain&>(*hgd_storage);
    timer.tock("solver construction");

    timer.tick();
    logger().debug(
        "[compositing] mesh: {} triangles, {} vertices, {} texcoords",
        wrapper.num_simplices(),
        wrapper.num_vertices(),
        wrapper.num_texcoords());
    logger().debug(
        "[compositing] texture: {}x{} ({} channels), {} views, direct {}",
        width,
        height,
        NumChannels,
        textures.size(),
        options.use_direct_solver);
    auto [x, weight_sums] = compute_value_target<NumChannels>(gd, in);
    timer.tock("value target");

    timer.tick();
    auto edge_differences = compute_edge_differences<NumChannels>(
        gd,
        in,
        weight_sums,
        options.gradient_normalization,
        options.smooth_low_weight_areas);
    timer.tock("edge differences");

    timer.tick();
    const double gradient_weight = 1.0;
    if (options.use_direct_solver) {
        solve_direct<NumChannels>(
            gd,
            edge_differences,
            options.value_weight,
            gradient_weight,
            options.stiffness_regularization_weight,
            x);
    } else {
        hgd_storage->applyLaplacianRegularization(options.stiffness_regularization_weight);
        solve_multigrid<NumChannels>(
            *hgd_storage,
            edge_differences,
            options.value_weight,
            gradient_weight,
            options.solver,
            x);
    }
    timer.tock("solve");

    timer.tick();
    if (options.clamp_to_range.has_value()) {
        mesh_utils::clamp_out_of_range<NumChannels>(x, gd, options.clamp_to_range.value());
    }
    auto composite = unpad_and_copy<NumChannels, ValueType>(gd, x, padding, width, height);
    timer.tock("postprocessing");

    return composite;
}

} // namespace

template <typename Scalar, typename Index, typename ValueType>
image::experimental::Array3D<ValueType> texture_compositing(
    const SurfaceMesh<Scalar, Index>& mesh,
    std::vector<ConstWeightedTextureView<ValueType>> textures,
    const CompositingOptions& options)
{
    // Input sanity checks
    la_runtime_assert(!textures.empty(), "No textures to composite");
    for (auto& texture : textures) {
        if (texture.texture.extent(0) != textures[0].texture.extent(0) ||
            texture.texture.extent(1) != textures[0].texture.extent(1) ||
            texture.texture.extent(2) != textures[0].texture.extent(2)) {
            throw std::runtime_error(format(
                "All textures must have the same dimensions: {}x{}x{} vs {}x{}x{}",
                texture.texture.extent(0),
                texture.texture.extent(1),
                texture.texture.extent(2),
                textures[0].texture.extent(0),
                textures[0].texture.extent(1),
                textures[0].texture.extent(2)));
        }
        if (texture.weights.extent(0) != textures[0].weights.extent(0) ||
            texture.weights.extent(1) != textures[0].weights.extent(1)) {
            throw std::runtime_error("All weights must have the same dimensions");
        }
        if (texture.weights.extent(2) != 1) {
            throw std::runtime_error("Weights must have 1 channel");
        }
        if (texture.weights.extent(0) != texture.texture.extent(0) ||
            texture.weights.extent(1) != texture.texture.extent(1)) {
            throw std::runtime_error("Weights must have the same dimensions as the texture");
        }
    }
    unsigned int num_channels = static_cast<unsigned int>(textures[0].texture.extent(2));
    image::experimental::Array3D<ValueType> output;

    switch (num_channels) {
    case 1: output = texture_compositing<1>(mesh, textures, options); break;
    case 2: output = texture_compositing<2>(mesh, textures, options); break;
    case 3: output = texture_compositing<3>(mesh, textures, options); break;
    case 4: output = texture_compositing<4>(mesh, textures, options); break;
    default: la_debug_assert("Only 1, 2, 3, or 4 channels supported");
    }

    return output;
}

#define LA_X_texture_compositing(ValueType, Scalar, Index)                \
    template image::experimental::Array3D<ValueType> texture_compositing( \
        const SurfaceMesh<Scalar, Index>& mesh,                           \
        std::vector<ConstWeightedTextureView<ValueType>> textures,        \
        const CompositingOptions& options);
#define LA_X_texture_compositing_aux(_, ValueType) LA_SURFACE_MESH_X(texture_compositing, ValueType)
LA_ATTRIBUTE_X(texture_compositing_aux, 0)

} // namespace lagrange::texproc
