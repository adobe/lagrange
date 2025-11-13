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

#include <lagrange/utils/build.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <Src/PreProcessing.h>
#include <Src/GradientDomain.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <Eigen/Sparse>

namespace lagrange::texproc {

namespace {

using namespace MishaK::TSP;

template <typename Scalar>
bool is_exactly_zero(Scalar x)
{
    return std::abs(x) < std::numeric_limits<Scalar>::denorm_min();
}

template <unsigned int NumChannels, typename Scalar, typename Index, typename ValueType>
image::experimental::Array3D<ValueType> texture_compositing(
    const SurfaceMesh<Scalar, Index>& mesh,
    std::vector<ConstWeightedTextureView<ValueType>> textures,
    const CompositingOptions& options)
{
    struct InputData
    {
        RegularGrid<K, Vector<double, NumChannels>> texture;
        RegularGrid<K, Vector<double, 1>> weights;
    };

    auto wrapper =
        mesh_utils::create_mesh_wrapper(mesh, RequiresIndexedTexcoords::Yes, CheckFlippedUV::Yes);
    std::vector<InputData> in(textures.size());
    RegularGrid<K, Vector<double, NumChannels>> out;

    for (size_t i = 0; i < textures.size(); i++) {
        mesh_utils::set_grid(textures[i].texture, in[i].texture);
        mesh_utils::set_grid(textures[i].weights, in[i].weights);
    }

    unsigned int width = in[0].texture.res(0);
    unsigned int height = in[0].texture.res(1);

    mesh_utils::jitter_texture(wrapper.texcoords, width, height, options.jitter_epsilon);

    Padding padding = mesh_utils::create_padding(wrapper, width, height);
    tbb::parallel_for(size_t(0), textures.size(), [&](size_t i) {
        padding.pad(in[i].texture);
        padding.pad(in[i].weights);
    });
    width += padding.width();
    height += padding.height();

    out.resize(width, height);

    // Construct the hierarchical gradient domain object
    // TODO: Compute number of levels based on texture size
    const bool normalize = true;
#if LAGRANGE_TARGET_BUILD_TYPE(DEBUG)
    const bool sanity_check = true;
#else
    const bool sanity_check = false;
#endif
    HierarchicalGradientDomain<double, Solver, Vector<double, NumChannels>> hgd(
        options.quadrature_samples,
        wrapper.num_simplices(),
        wrapper.num_vertices(),
        wrapper.num_texcoords(),
        [&](size_t t, unsigned int k) { return wrapper.vertex_index(t, k); },
        [&](size_t v) { return wrapper.vertex(v); },
        [&](size_t t, unsigned int k) { return wrapper.texture_index(t, k); },
        [&](size_t v) { return wrapper.texcoord(v); }, // solver internally flips the v coordinate
        width,
        height,
        options.solver.num_multigrid_levels,
        normalize,
        sanity_check);

    // Get the pointers to the solver constraints and solution
    span<Vector<double, NumChannels>> x{hgd.x(), hgd.numNodes()};
    span<Vector<double, NumChannels>> b{hgd.b(), hgd.numNodes()};

    // Compute the sum of weights for texel n
    auto compute_weight_sum = [&](unsigned int row, unsigned int col) {
        double weight_sum = 0;
        for (unsigned int i = 0; i < in.size(); i++) {
            weight_sum += in[i].weights(row, col)[0];
        }
        return weight_sum;
    };

    // Normalization factor for texel n
    auto normalization_weight = [&](unsigned int row, unsigned int col, bool is_grad) -> double {
        const double weight_sum = compute_weight_sum(row, col);
        if (is_grad && options.smooth_low_weight_areas && weight_sum < 1.) {
            // Do not normalize gradients in low-confidence areas. This reduces the importance of
            // the gradient terms and smooths the resulting texture in those areas.
            return 1.;
        } else {
            return weight_sum > 0 ? 1. / weight_sum : 1.;
        }
    };

    // Compute the weighted sum of texture values
    for (size_t n = 0; n < hgd.numNodes(); n++) {
        auto [row, col] = hgd.node(n);

        const double scale = normalization_weight(row, col, false);
        for (unsigned int i = 0; i < in.size(); i++) {
            x[n] += in[i].texture(row, col) * in[i].weights(row, col)[0] * scale;
        }
    }

    // Set unobserved texels to the average observed color
    {
        Vector<double, NumChannels> avg_observed_color;
        size_t num_observed_texels = 0;
        size_t num_unobserved_texels = 0;
        for (size_t n = 0; n < hgd.numNodes(); n++) {
            auto [row, col] = hgd.node(n);
            if (!is_exactly_zero(compute_weight_sum(row, col))) {
                avg_observed_color += x[n];
                num_observed_texels++;
            } else {
                num_unobserved_texels++;
            }
        }
        if (num_unobserved_texels > 0) {
            logger().warn(
                "Found {} unobserved texels. Setting target values to the average observed color.",
                num_unobserved_texels);
        }
        if (num_observed_texels > 0) {
            avg_observed_color /= static_cast<double>(num_observed_texels);
        }
        for (size_t n = 0; n < hgd.numNodes(); n++) {
            auto [row, col] = hgd.node(n);
            if (is_exactly_zero(compute_weight_sum(row, col))) {
                x[n] = avg_observed_color;
            }
        }
    }

    // Construct the constraints
    {
        std::vector<Vector<double, NumChannels>> value_b(hgd.numNodes()),
            gradient_b(hgd.numNodes());

        // Get the constraints from the values
        hgd.mass(&x[0], &value_b[0]);

        // Get the constraints from the gradients
        {
            // Compute the edge differences
            std::vector<Vector<double, NumChannels>> edge_differences(hgd.numEdges());
            for (size_t e = 0; e < hgd.numEdges(); e++) {
                std::pair<size_t, size_t> end_points = hgd.edge(e);

                auto [row1, col1] = hgd.node(end_points.first);
                auto [row2, col2] = hgd.node(end_points.second);

                const double scale1 = normalization_weight(row1, col1, true);
                const double scale2 = normalization_weight(row2, col2, true);
                for (unsigned int i = 0; i < in.size(); i++) {
                    double weight = in[i].weights(row1, col1)[0] * scale1 *
                                    in[i].weights(row2, col2)[0] * scale2;
                    if (weight > 0) {
                        weight = sqrt(weight);
                        edge_differences[e] +=
                            (in[i].texture(row2, col2) - in[i].texture(row1, col1)) * weight;
                    }
                }
            }

            // Compute the associated divergence
            hgd.divergence(&edge_differences[0], &gradient_b[0]);
        }

        // Combine the constraints
        for (size_t n = 0; n < hgd.numNodes(); n++) {
            b[n] = value_b[n] * options.value_weight + gradient_b[n];
        }
    }

    // Compute the system matrix
    const double gradient_weight = 1.0;
    hgd.updateSystem(options.value_weight, gradient_weight);

    // Relax the solution
    for (unsigned int v = 0; v < options.solver.num_v_cycles; ++v) {
        hgd.vCycle(options.solver.num_gauss_seidel_iterations);
    }

    // Put the texel values back into the texture
    for (size_t n = 0; n < hgd.numNodes(); n++) {
        std::pair<unsigned int, unsigned int> coords = hgd.node(n);
        out(coords.first, coords.second) = x[n];
    }

    // Undo padding
    padding.unpad(out);

    image::experimental::Array3D<ValueType> composite =
        image::experimental::create_image<ValueType>(
            textures[0].texture.extent(0),
            textures[0].texture.extent(1),
            textures[0].texture.extent(2));

    // Copy the texture grid data back into the texture
    for (unsigned int j = 0; j < out.res(1); j++) {
        for (unsigned int i = 0; i < out.res(0); i++) {
            for (unsigned int c = 0; c < NumChannels; c++) {
                composite(i, j, c) = out(i, j)[c];
            }
        }
    }

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
            throw std::runtime_error(
                fmt::format(
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
    if (!textures.size()) throw std::runtime_error("Expected at least one texture");


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
