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

#include <lagrange/texproc/texture_stitching.h>

#include "mesh_utils.h"

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <Src/PreProcessing.h>
#include <Src/GradientDomain.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <random>
#include <unordered_set>

namespace lagrange::texproc {

namespace {

template <unsigned int NumChannels, typename Scalar, typename Index, typename ValueType>
void texture_stitching(
    const SurfaceMesh<Scalar, Index>& mesh,
    image::experimental::View3D<ValueType> texture,
    bool exterior_only,
    bool randomize,
    unsigned int quadrature_samples,
    double jitter_epsilon)
{
    auto wrapper = mesh_utils::create_mesh_wrapper(mesh);
    RegularGrid<K, Vector<double, NumChannels>> grid;

    mesh_utils::set_grid(texture, grid);

    mesh_utils::jitter_texture(wrapper.texcoords, grid.res(0), grid.res(1), jitter_epsilon);

    Padding padding = mesh_utils::create_padding(wrapper, grid.res(0), grid.res(1));
    padding.pad(grid);

    // Construct the gradient-domain object
    GradientDomain<double> gd(
        quadrature_samples,
        wrapper.num_simplices(),
        wrapper.num_vertices(),
        wrapper.num_texcoords(),
        [&](size_t t, unsigned int k) { return wrapper.vertex_index(t, k); },
        [&](size_t v) { return wrapper.vertex(v); },
        [&](size_t t, unsigned int k) { return wrapper.texture_index(t, k); },
        [&](size_t v) { return wrapper.texcoord(v); },
        grid.res(0),
        grid.res(1));

    // Compute the prolongation matrix from the degrees of freedom to texels
    std::unordered_set<size_t> dof_set;
    Eigen::SparseMatrix<double> P, Pt;
    {
        std::vector<Eigen::Triplet<double>> triplets;
        {
            if (exterior_only) {
                for (size_t n = 0; n < gd.numNodes(); n++) {
                    if (!gd.isCovered(n)) {
                        dof_set.insert(n);
                    }
                }
            } else {
                for (size_t e = 0; e < gd.numEdges(); e++) {
                    if (gd.isChartCrossing(e)) {
                        std::pair<size_t, size_t> edge = gd.edge(e);
                        dof_set.insert(edge.first);
                        dof_set.insert(edge.second);
                    }
                }
            }
            if (dof_set.empty()) {
                // Nothing to stitch
                logger().warn("No seam to stitch.");
                return;
            }
            triplets.reserve(dof_set.size());
            size_t idx = 0;
            for (auto iter = dof_set.begin(); iter != dof_set.end(); iter++) {
                triplets.emplace_back(
                    static_cast<typename Eigen::SparseMatrix<double>::StorageIndex>(*iter),
                    static_cast<typename Eigen::SparseMatrix<double>::StorageIndex>(idx++),
                    1.);
            }
        }
        P.resize(gd.numNodes(), triplets.size());
        P.setFromTriplets(triplets.begin(), triplets.end());
        Pt = P.transpose();
    }

    std::vector<Vector<double, NumChannels>> x(gd.numNodes()), b(gd.numNodes());

    // Copy the texture values into the vector
    for (size_t n = 0; n < gd.numNodes(); n++) {
        std::pair<unsigned int, unsigned int> coords = gd.node(n);
        x[n] = grid(coords.first, coords.second);
    }

    if (randomize) {
        std::mt19937 gen;
        std::uniform_real_distribution<double> dist(0., 255.);
        for (size_t n = 0; n < gd.numNodes(); n++) {
            if (dof_set.find(n) != dof_set.end()) {
                for (unsigned int c = 0; c < NumChannels; c++) {
                    x[n][c] = dist(gen);
                }
            }
        }
    }

    // Construct the constraints
    gd.stiffness(&x[0], &b[0]);

    // Compute the system matrix
    Eigen::SparseMatrix<double> M = Pt * gd.stiffness() * P;

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
        _b = Pt * _b;
        Eigen::VectorXd _x = P * solver.solve(_b);
        for (size_t n = 0; n < gd.numNodes(); n++) {
            x[n][c] -= _x[n];
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
void texture_stitching(
    const SurfaceMesh<Scalar, Index>& mesh,
    image::experimental::View3D<ValueType> texture,
    const StitchingOptions& options)
{
    unsigned int num_channels = static_cast<unsigned int>(texture.extent(2));
    switch (num_channels) {
    case 1:
        texture_stitching<1>(
            mesh,
            texture,
            options.exterior_only,
            options.__randomize,
            options.quadrature_samples,
            options.jitter_epsilon);
        break;
    case 2:
        texture_stitching<2>(
            mesh,
            texture,
            options.exterior_only,
            options.__randomize,
            options.quadrature_samples,
            options.jitter_epsilon);
        break;
    case 3:
        texture_stitching<3>(
            mesh,
            texture,
            options.exterior_only,
            options.__randomize,
            options.quadrature_samples,
            options.jitter_epsilon);
        break;
    case 4:
        texture_stitching<4>(
            mesh,
            texture,
            options.exterior_only,
            options.__randomize,
            options.quadrature_samples,
            options.jitter_epsilon);
        break;
    default: la_debug_assert("Only 1, 2, 3, or 4 channels supported");
    }
}

// TODO: implement this weighted version.
///
/// Stitch the seams of a texture associated with a mesh.
///
/// @param[in]     mesh       Input mesh with UV attributes.
/// @param[in,out] texture    Texture to stitch.
/// @param[in]     weights    Weights for each texel (0 for fixed, 1 for free).
/// @param[in]     options    Stitching options.
///
/// @tparam        Scalar     Mesh scalar type.
/// @tparam        Index      Mesh index type.
/// @tparam        ValueType  Texture value type.
///
// template <typename Scalar, typename Index, typename ValueType>
// void texture_stitching(
//     const SurfaceMesh<Scalar, Index>& mesh,
//     image::experimental::View3D<ValueType> texture,
//     image::experimental::View3D<const float> weights,
//     const StitchingOptions& options);

#define LA_X_texture_stitching(ValueType, Scalar, Index) \
    template void texture_stitching(                     \
        const SurfaceMesh<Scalar, Index>& mesh,          \
        image::experimental::View3D<ValueType> texture,               \
        const StitchingOptions& options);
#define LA_X_texture_stitching_aux(_, ValueType) LA_SURFACE_MESH_X(texture_stitching, ValueType)
LA_ATTRIBUTE_X(texture_stitching_aux, 0)

} // namespace lagrange::texproc
