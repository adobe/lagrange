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
#pragma once

#include <lagrange/SurfaceMesh.h>

#include <lagrange/solver/DirectSolver.h>

// Include before any ShapeGradientDomain header to override their threadpool implementation.
#include "ThreadPool.h"
#define MULTI_THREADING_INCLUDED
using namespace lagrange::filtering::threadpool;

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <Include/PreProcessor.h>
#include <Include/GradientDomain.h>
#include <Include/CurvatureMetric.h>
#include <Misha/Geometry.h>
#include <Misha/Miscellany.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <string_view>
#include <vector>

namespace lagrange::filtering {

namespace {

using namespace MishaK;

// Using `Point` directly leads to ambiguity with Apple Accelerate types.
template <typename T, unsigned N>
using Vector = MishaK::Point<T, N>;

// The dimension of the manifold.
static const unsigned int K = 2;

// The dimension of the space into which the manifold is embedded.
static const unsigned int Dim = 3;

using Real = double;

using Solver = lagrange::solver::SolverLDLT<Eigen::SparseMatrix<Real>>;

} // namespace

/**
 * Common utility functions for mesh and attribute smoothing
 */
namespace smoothing_utils {

/**
 * Extract triangles from a mesh
 *
 * @tparam Scalar The scalar type used for mesh coordinates
 * @tparam Index The index type used for mesh connectivity
 * @param t_mesh The input mesh
 * @param triangles Output vector of triangles
 */
template <typename Scalar, typename Index>
void get_triangles(
    const SurfaceMesh<Scalar, Index>& t_mesh,
    std::vector<SimplexIndex<K, int>>& triangles);

/**
 * Extract vertices and normals from a mesh
 *
 * @tparam Scalar The scalar type used for mesh coordinates
 * @tparam Index The index type used for mesh connectivity
 * @param t_mesh The input mesh
 * @param vertices Output vector of vertices
 * @param normals Output vector of normals
 * @param normal_id The attribute ID for normals
 */
template <typename Scalar, typename Index>
void get_vertices_and_normals(
    const SurfaceMesh<Scalar, Index>& t_mesh,
    std::vector<Vector<Real, Dim>>& vertices,
    std::vector<Vector<Real, Dim>>& normals,
    AttributeId normal_id);

/**
 * Set vertices in a mesh
 *
 * @tparam Scalar The scalar type used for mesh coordinates
 * @tparam Index The index type used for mesh connectivity
 * @param mesh The mesh to update
 * @param vertices The new vertex positions
 */
template <typename Scalar, typename Index>
void set_vertices(SurfaceMesh<Scalar, Index>& mesh, const std::vector<Vector<Real, Dim>>& vertices);

/**
 * Setup a mesh for smoothing operations
 *
 * This function performs common setup operations for both mesh and attribute smoothing:
 * 1. Triangulates polygonal facets
 * 2. Ensures normals are available and properly formatted
 * 3. Extracts triangles, vertices, and normals
 * 4. Creates and sets up the Riemannian mesh
 *
 * @tparam Scalar The scalar type used for mesh coordinates
 * @tparam Index The index type used for mesh connectivity
 * @param mesh The input mesh
 * @param _mesh Output triangulated mesh
 * @param triangles Output vector of triangles
 * @param vertices Output vector of vertices
 * @param normals Output vector of normals
 * @param solver The solver to use
 * @param original_area Output original mesh area
 *
 * @return The Riemannian mesh
 */
template <typename Scalar, typename Index>
std::unique_ptr<FEM::RiemannianMesh<Real>> setup_for_smoothing(
    SurfaceMesh<Scalar, Index>& mesh,
    SurfaceMesh<Scalar, Index>& _mesh,
    std::vector<SimplexIndex<K, int>>& triangles,
    std::vector<Vector<Real, Dim>>& vertices,
    std::vector<Vector<Real, Dim>>& normals,
    Solver& solver,
    Real& original_area);

/**
 * Adjust the metric based on curvature
 *
 * @param r_mesh The Riemannian mesh
 * @param vertices The mesh vertices
 * @param normals The mesh normals
 * @param original_area The original mesh area
 * @param curvature_weight The curvature weight
 * @param normal_smoothing_weight The normal smoothing weight
 * @param solver The solver to use
 */
void adjust_metric_for_curvature(
    FEM::RiemannianMesh<Real>& r_mesh,
    const std::vector<Vector<Real, Dim>>& vertices,
    const std::vector<Vector<Real, Dim>>& normals,
    Real original_area,
    double curvature_weight,
    double normal_smoothing_weight,
    Solver& solver);

} // namespace smoothing_utils

} // namespace lagrange::filtering
