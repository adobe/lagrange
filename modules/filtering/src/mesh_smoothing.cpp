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

#include <lagrange/filtering/mesh_smoothing.h>

#include <lagrange/Attribute.h>
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/cast_attribute.h>
#include <lagrange/compute_vertex_normal.h>
#include <lagrange/find_matching_attributes.h>
#include <lagrange/internal/visit_attribute.h>
#include <lagrange/map_attribute.h>
#include <lagrange/triangulate_polygonal_facets.h>
#include <lagrange/utils/Error.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/build.h>
#include <lagrange/utils/scope_guard.h>
#include <lagrange/utils/timing.h>
#include <lagrange/views.h>

#ifdef LA_SOLVER_ACCELERATE
    #include "AccelerateSupport.h"
#else
    #if LAGRANGE_TARGET_OS(WASM)
        #include <Eigen/SparseCholesky>
    #else
        #include <Eigen/PardisoSupport>
    #endif
#endif

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

namespace lagrange::filtering {

namespace {

using namespace MishaK;
using namespace MishaK::Geometry;
using namespace MishaK::Array;

// Using `Point` directly leads to ambiguity with Apple Accelerate types.
template <typename T, unsigned N>
using Vector = MishaK::Geometry::Point<T, N>;

// The dimension of the manifold.
static const unsigned int K = 2;

// The dimension of the space into which the manifold is embedded.
static const unsigned int Dim = 3;

using Real = double;

#ifdef LA_SOLVER_ACCELERATE
using Solver = AccelerateLDLT<Eigen::SparseMatrix<Real>>;
#else
    #if LAGRANGE_TARGET_OS(WASM)
using Solver = Eigen::SimplicialLDLT<Eigen::SparseMatrix<Real>>;
    #else
using Solver = Eigen::PardisoLDLT<Eigen::SparseMatrix<Real>>;
    #endif
#endif

template <typename Scalar, typename Index>
void get_triangles(
    const SurfaceMesh<Scalar, Index>& t_mesh,
    std::vector<SimplexIndex<K, int>>& triangles)
{
    triangles.resize(t_mesh.get_num_facets());
    auto vertex_indices = t_mesh.get_corner_to_vertex().get_all();
    for (unsigned int i = 0; i < t_mesh.get_num_facets(); i++) {
        for (unsigned int k = 0; k <= K; k++) {
            triangles[i][k] = static_cast<int>(vertex_indices[i * (K + 1) + k]);
        }
    }
}

template <typename Scalar, typename Index>
void get_vertices_and_normals(
    const SurfaceMesh<Scalar, Index>& t_mesh,
    std::vector<Vector<Real, Dim>>& vertices,
    std::vector<Vector<Real, Dim>>& normals,
    AttributeId normal_id)
{
    vertices.resize(t_mesh.get_num_vertices());
    normals.resize(t_mesh.get_num_vertices());

    // Retrieve input vertex buffer
    auto& input_coords = t_mesh.get_vertex_to_position();
    la_runtime_assert(
        input_coords.get_num_elements() == t_mesh.get_num_vertices(),
        "Number of vertices should match number of vertices");

    auto& input_normals = t_mesh.template get_attribute<Scalar>(normal_id);
    la_runtime_assert(
        input_normals.get_num_channels() == 3,
        "Input normals should only have 3 channels");
    la_runtime_assert(
        input_normals.get_num_elements() == t_mesh.get_num_vertices(),
        "Number of normals should match number of vertices");

    auto _input_coords = input_coords.get_all();
    auto _input_normals = input_normals.get_all();

    for (unsigned int i = 0; i < t_mesh.get_num_vertices(); i++) {
        for (unsigned int k = 0; k <= K; k++) {
            vertices[i][k] = static_cast<Real>(_input_coords[i * (K + 1) + k]);
            normals[i][k] = static_cast<Real>(_input_normals[i * (K + 1) + k]);
        }
        normals[i] /= Vector<Real, Dim>::Length(normals[i]);
    }
}

template <typename Scalar, typename Index>
void set_vertices(SurfaceMesh<Scalar, Index>& mesh, const std::vector<Vector<Real, Dim>>& vertices)
{
    la_runtime_assert(
        vertices.size() == mesh.get_num_vertices(),
        "Number of vertices should match number of vertices");

    // Retrieve input vertex buffer
    auto input_coords = mesh.ref_vertex_to_position().ref_all();

    for (unsigned int i = 0; i < mesh.get_num_vertices(); i++) {
        for (unsigned int k = 0; k <= K; k++) {
            input_coords[i * (K + 1) + k] = static_cast<Scalar>(vertices[i][k]);
        }
    }
}

} // namespace

template <typename Scalar, typename Index>
void mesh_smoothing(SurfaceMesh<Scalar, Index>& mesh, const SmoothingOptions& options)
{
    VerboseTimer timer("mesh_smoothing");
    timer.tick();

    SurfaceMesh<Scalar, Index> _mesh = mesh;
    triangulate_polygonal_facets(_mesh);

    // Get the normal id (and set the normals if they weren't already)
    AttributeId normal_id;

    // If the mesh comes with normals
    if (auto res = find_matching_attribute(_mesh, AttributeUsage::Normal)) {
        normal_id = res.value();
    } else {
        // Generate per-vertex normals
        normal_id = compute_vertex_normal(_mesh);
    }
    // Make sure the normal coordinate type is the same as that of the vertices
    if (!_mesh.template is_attribute_type<Scalar>(normal_id)) {
        logger().warn("Input normals do not have the same scalar type as the input points. Casting "
                      "attribute.");
        normal_id = cast_attribute_in_place<Scalar>(_mesh, normal_id);
    }

    // Make sure the normals are associated with the vertices
    if (_mesh.get_attribute_base(normal_id).get_element_type() != AttributeElement::Vertex) {
        normal_id = map_attribute(_mesh, normal_id, "new_normal", AttributeElement::Vertex);
    }

    Solver solver;

    std::vector<SimplexIndex<K, int>> triangles;
    get_triangles(_mesh, triangles);

    std::vector<Vector<Real, Dim>> vertices, normals;
    get_vertices_and_normals(_mesh, vertices, normals, normal_id);
    logger().debug("Source Vertices / Triangles: {} / {}", vertices.size(), triangles.size());

    //////////////////////////
    // Set the Riemannian mesh
    VerboseTimer r_mesh_timer("├── Set Riemannian mesh");
    r_mesh_timer.tick();
    Real original_area = 0;
    FEM::RiemannianMesh<Real> r_mesh(GetPointer(triangles), triangles.size());
    {
        // Create the embedded metric and normalize to have unit area
        unsigned int count = r_mesh.template setMetricFromEmbedding<Dim>(
            [&](unsigned int i) { return vertices[i]; },
            false);
        if (count) {
            logger().warn("Found poorly formed triangles: {}", count);
        }
        original_area = r_mesh.area();
        r_mesh.makeUnitArea();
    }
    r_mesh_timer.tock();
    // Set the Riemannian mesh
    //////////////////////////


    ///////////////////////////////////////
    // System matrix symbolic factorization
    VerboseTimer factorization_timer("├── Symbolic factorization");
    factorization_timer.tick();
    solver.analyzePattern(r_mesh.template stiffnessMatrix<FEM::BASIS_0_WHITNEY, true>());
    factorization_timer.tock();
    ///////////////////////////////////////

    ///////////////////////////////////////////////////////
    // Adjust the metric to take into account the curvature
    if (options.curvature_weight > 0) {
        VerboseTimer metric_timer("├── Adjust metric");
        metric_timer.tick();

        std::vector<Vector<Real, Dim>> curvature_normals = normals;

        /////////////////////////////
        // Curvature normal smoothing
        if (options.normal_smoothing_weight > 0) {
            VerboseTimer normal_timer("│   ├── Normal smoothing");
            normal_timer.tick();
            curvature_normals = GradientDomain::ProcessVertexVertex<Solver, Point3D<Real>, Real>(
                solver,
                r_mesh,
                (Real)1.,
                options.normal_smoothing_weight,
                [&](unsigned int v) { return curvature_normals[v]; },
                [](unsigned int) { return Vector<Real, 3>(); });

            ThreadPool::ParallelFor(0, curvature_normals.size(), [&](unsigned int, size_t i) {
                curvature_normals[i] /= Vector<Real, 3>::Length(curvature_normals[i]);
            });

            normal_timer.tock();
        }
        // Curvature normal smoothing
        /////////////////////////////

        ////////////////////////////////////
        // Adapt the metric to the curvature
        {
            VerboseTimer metric_update_timer("│   └── Metric update");
            metric_update_timer.tick();

            // Input: Principal curvature values
            // Output: Positive entries of the diagonal matrix describing the scaling along the
            // principal curvature directions
            //         Outputting the identity matrix reproduces the embedding metric
            auto PrincipalCurvatureFunctor = [&](unsigned int, Vector<Real, 2> p_curvatures) {
                p_curvatures[0] = p_curvatures[1] = p_curvatures.squareNorm() / 2;
                return Vector<Real, 2>(Real(1.f), Real(1.f)) +
                       p_curvatures * options.curvature_weight;
            };

            Real s = static_cast<Real>(1. / std::sqrt(original_area));
            CurvatureMetric::SetCurvatureMetric(
                r_mesh,
                [&](unsigned int idx) { return vertices[idx] * s; },
                [&](unsigned int idx) { return curvature_normals[idx]; },
                PrincipalCurvatureFunctor);

            metric_update_timer.tock();
        }
        // Adapt the metric to the curvature
        ////////////////////////////////////

        metric_timer.tock();
    }
    // Adjust the metric to take into account the curvature
    ///////////////////////////////////////////////////////


    /////////////////////
    // Smooth the normals
    if (options.gradient_weight > 0 && options.gradient_modulation_scale != 1) {
        VerboseTimer smooth_timer("├── Smooth normals");
        smooth_timer.tick();

        // Low frequencies described in terms of values at vertices
        auto LowFrequencyVertexValues = [&](unsigned int v) { return normals[v]; };

        // High frequencies described in terms of scaled values at vertices
        auto HighFrequencyVertexValues = [&](unsigned int v) {
            la_debug_assert(v < normals.size(), "Vertex index out of bounds");
            return normals[v] * static_cast<Real>(options.gradient_modulation_scale);
        };

        normals = GradientDomain::ProcessVertexVertex<Solver, Point3D<Real>, Real>(
            solver,
            r_mesh,
            Real(1.f),
            static_cast<Real>(options.gradient_weight),
            LowFrequencyVertexValues,
            HighFrequencyVertexValues);

        smooth_timer.tock();
    }
    // Smooth the normals
    /////////////////////


    ///////////////////////////////////////////
    // Fit the geometry to the smoothed normals
    {
        VerboseTimer fit_timer("└── Fit geometry");
        fit_timer.tick();

        vertices = GradientDomain::FitToNormals<Solver>(
            solver,
            r_mesh,
            Real(1.f),
            static_cast<Real>(options.normal_projection_weight),
            [&](unsigned int v) { return vertices[v]; },
            [&](unsigned int v) { return normals[v]; });

        fit_timer.tock();
    }
    // Fit the geometry to the smoothed normals
    ///////////////////////////////////////////

    set_vertices(mesh, vertices);
    timer.tock();
}

#define LA_X_mesh_reconstruction(_, Scalar, Index) \
    template void mesh_smoothing(SurfaceMesh<Scalar, Index>& mesh, const SmoothingOptions& options);
LA_SURFACE_MESH_X(mesh_reconstruction, 0)

} // namespace lagrange::filtering
