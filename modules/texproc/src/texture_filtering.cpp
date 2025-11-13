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

#include <lagrange/texproc/texture_filtering.h>

#include <lagrange/AttributeTypes.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/utils/build.h>

#include "mesh_utils.h"

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <Src/PreProcessing.h>
#include <Src/GradientDomain.h>
#include <lagrange/utils/warnon.h>
// clang-format on

namespace lagrange::texproc {

namespace {

using namespace MishaK::TSP;

template <unsigned int NumChannels, typename Scalar, typename Index, typename ValueType>
void texture_gradient_modulation(
    const SurfaceMesh<Scalar, Index>& mesh,
    image::experimental::View3D<ValueType> texture,
    double value_weight,
    double gradient_weight,
    double gradient_scale,
    unsigned int quadrature_samples,
    double jitter_epsilon)
{
    auto wrapper =
        mesh_utils::create_mesh_wrapper(mesh, RequiresIndexedTexcoords::Yes, CheckFlippedUV::Yes);
    RegularGrid<K, Vector<double, NumChannels>> grid;

    mesh_utils::set_grid(texture, grid);

    mesh_utils::jitter_texture(wrapper.texcoords, grid.res(0), grid.res(1), jitter_epsilon);

    Padding padding = mesh_utils::create_padding(wrapper, grid.res(0), grid.res(1));
    padding.pad(grid);

    // Construct the gradient-domain object
    // TODO: Switch to multi-grid solver
    const bool normalize = true;
#if LAGRANGE_TARGET_BUILD_TYPE(DEBUG)
    const bool sanity_check = true;
#else
    const bool sanity_check = false;
#endif
    GradientDomain<double> gd(
        quadrature_samples,
        wrapper.num_simplices(),
        wrapper.num_vertices(),
        wrapper.num_texcoords(),
        [&](size_t t, unsigned int k) { return wrapper.vertex_index(t, k); },
        [&](size_t v) { return wrapper.vertex(v); },
        [&](size_t t, unsigned int k) { return wrapper.texture_index(t, k); },
        [&](size_t v) { return wrapper.texcoord(v); }, // solver internally flips the v coordinate
        grid.res(0),
        grid.res(1),
        normalize,
        sanity_check);

    std::vector<Vector<double, NumChannels>> x(gd.numNodes()), b(gd.numNodes());

    // Copy the texture values into the vector
    for (size_t n = 0; n < gd.numNodes(); n++) {
        std::pair<unsigned int, unsigned int> coords = gd.node(n);
        x[n] = grid(coords.first, coords.second);
    }

    // Construct the constraints
    {
        std::vector<Vector<double, NumChannels>> mass_b(gd.numNodes()), stiffness_b(gd.numNodes());

        // Get the constraints from the values
        gd.mass(&x[0], &mass_b[0]);

        // Get the constraints from the gradients
        gd.stiffness(&x[0], &stiffness_b[0]);

        // Combine the constraints
        for (size_t n = 0; n < gd.numNodes(); n++)
            b[n] = mass_b[n] * value_weight + stiffness_b[n] * gradient_weight * gradient_scale;
    }

    // Compute the system matrix
    Eigen::SparseMatrix<double> M = gd.mass() * value_weight + gd.stiffness() * gradient_weight;

    // Construct/factor the solver
    Solver solver(M);
    switch (solver.info()) {
    case Eigen::Success: break;
    case Eigen::NumericalIssue: la_debug_assert("Failed to factor matrix (numerical issue)"); break;
    case Eigen::NoConvergence: la_debug_assert("Failed to factor matrix (no convergence)"); break;
    case Eigen::InvalidInput: la_debug_assert("Failed to factor matrix (invalid input)"); break;
    default: la_debug_assert("Failed to factor matrix");
    }

    // Solve the system per channel
    for (unsigned int c = 0; c < NumChannels; c++) {
        Eigen::VectorXd _b(gd.numNodes());
        for (size_t n = 0; n < gd.numNodes(); n++) {
            _b[n] = b[n][c];
        }
        Eigen::VectorXd _x = solver.solve(_b);
        for (size_t n = 0; n < gd.numNodes(); n++) {
            x[n][c] = _x[n];
        }
    }

    // Put the texel values back into the texture
    for (size_t n = 0; n < gd.numNodes(); n++) {
        std::pair<unsigned int, unsigned int> coords = gd.node(n);
        grid(coords.first, coords.second) = x[n];
    }

    // Undo padding
    padding.unpad(grid);

    // Copy the texture grid data back into the texture
    for (unsigned int j = 0; j < grid.res(1); j++) {
        for (unsigned int i = 0; i < grid.res(0); i++) {
            for (unsigned int c = 0; c < NumChannels; c++) {
                texture(i, j, c) = grid(i, j)[c];
            }
        }
    }
}

} // namespace

template <typename Scalar, typename Index, typename ValueType>
void texture_filtering(
    const SurfaceMesh<Scalar, Index>& mesh,
    image::experimental::View3D<ValueType> texture,
    const FilteringOptions& options)
{
    unsigned int num_channels = static_cast<unsigned int>(texture.extent(2));
    switch (num_channels) {
    case 1:
        texture_gradient_modulation<1>(
            mesh,
            texture,
            options.value_weight,
            options.gradient_weight,
            options.gradient_scale,
            options.quadrature_samples,
            options.jitter_epsilon);
        break;
    case 2:
        texture_gradient_modulation<2>(
            mesh,
            texture,
            options.value_weight,
            options.gradient_weight,
            options.gradient_scale,
            options.quadrature_samples,
            options.jitter_epsilon);
        break;
    case 3:
        texture_gradient_modulation<3>(
            mesh,
            texture,
            options.value_weight,
            options.gradient_weight,
            options.gradient_scale,
            options.quadrature_samples,
            options.jitter_epsilon);
        break;
    case 4:
        texture_gradient_modulation<4>(
            mesh,
            texture,
            options.value_weight,
            options.gradient_weight,
            options.gradient_scale,
            options.quadrature_samples,
            options.jitter_epsilon);
        break;
    default: la_debug_assert("Only 1, 2, 3, or 4 channels supported");
    }
}

#define LA_X_texture_filtering(ValueType, Scalar, Index) \
    template void texture_filtering(                     \
        const SurfaceMesh<Scalar, Index>& mesh,          \
        image::experimental::View3D<ValueType> texture,  \
        const FilteringOptions& options);
#define LA_X_texture_filtering_aux(_, ValueType) LA_SURFACE_MESH_X(texture_filtering, ValueType)
LA_ATTRIBUTE_X(texture_filtering_aux, 0)

} // namespace lagrange::texproc
