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

#include "smoothing_utils.h"

#include <lagrange/Attribute.h>
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/cast_attribute.h>
#include <lagrange/compute_vertex_normal.h>
#include <lagrange/find_matching_attributes.h>
#include <lagrange/map_attribute.h>
#include <lagrange/triangulate_polygonal_facets.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/build.h>
#include <lagrange/utils/scope_guard.h>
#include <lagrange/utils/timing.h>
#include <lagrange/views.h>

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

namespace smoothing_utils {

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

template <typename Scalar, typename Index>
std::unique_ptr<FEM::RiemannianMesh<Real>> setup_for_smoothing(
    SurfaceMesh<Scalar, Index>& mesh,
    SurfaceMesh<Scalar, Index>& _mesh,
    std::vector<SimplexIndex<K, int>>& triangles,
    std::vector<Vector<Real, Dim>>& vertices,
    std::vector<Vector<Real, Dim>>& normals,
    Solver& solver,
    Real& original_area)
{
    _mesh = mesh;
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
        logger().warn(
            "Input normals do not have the same scalar type as the input points. Casting "
            "attribute.");
        normal_id = cast_attribute_in_place<Scalar>(_mesh, normal_id);
    }

    // Make sure the normals are associated with the vertices
    if (_mesh.get_attribute_base(normal_id).get_element_type() != AttributeElement::Vertex) {
        normal_id = map_attribute(_mesh, normal_id, "new_normal", AttributeElement::Vertex);
    }

    get_triangles(_mesh, triangles);
    get_vertices_and_normals(_mesh, vertices, normals, normal_id);
    logger().debug("Source Vertices / Triangles: {} / {}", vertices.size(), triangles.size());

    // Set the Riemannian mesh
    VerboseTimer r_mesh_timer("├── Set Riemannian mesh");
    r_mesh_timer.tick();
    original_area = 0;
    auto r_mesh =
        std::make_unique<FEM::RiemannianMesh<Real>>(GetPointer(triangles), triangles.size());
    {
        // Create the embedded metric and normalize to have unit area
        unsigned int count = r_mesh->template setMetricFromEmbedding<Dim>(
            [&](unsigned int i) { return vertices[i]; },
            false);
        if (count) {
            logger().warn("Found poorly formed triangles: {}", count);
        }
        original_area = r_mesh->area();
        r_mesh->makeUnitArea();
    }
    r_mesh_timer.tock();

    // System matrix symbolic factorization
    VerboseTimer factorization_timer("├── Symbolic factorization");
    factorization_timer.tick();
    solver.analyzePattern(r_mesh->template stiffnessMatrix<FEM::BASIS_0_WHITNEY, true>());
    factorization_timer.tock();

    return r_mesh;
}

void adjust_metric_for_curvature(
    FEM::RiemannianMesh<Real>& r_mesh,
    const std::vector<Vector<Real, Dim>>& vertices,
    const std::vector<Vector<Real, Dim>>& normals,
    Real original_area,
    double curvature_weight,
    double normal_smoothing_weight,
    Solver& solver)
{
    VerboseTimer metric_timer("├── Adjust metric");
    metric_timer.tick();

    auto curvature_normals = normals;

    // Curvature normal smoothing
    if (normal_smoothing_weight > 0) {
        VerboseTimer normal_timer("│   ├── Normal smoothing");
        normal_timer.tick();
        curvature_normals = GradientDomain::ProcessVertexVertex<Solver, Point3D<Real>, Real>(
            solver,
            r_mesh,
            (Real)1.,
            normal_smoothing_weight,
            [&](unsigned int v) { return curvature_normals[v]; },
            [](unsigned int) { return Vector<Real, 3>(); });

        ThreadPool::ParallelFor(0, curvature_normals.size(), [&](unsigned int, size_t i) {
            curvature_normals[i] /= Vector<Real, 3>::Length(curvature_normals[i]);
        });

        normal_timer.tock();
    }

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
            return Vector<Real, 2>(Real(1.f), Real(1.f)) + p_curvatures * curvature_weight;
        };

        Real s = static_cast<Real>(1. / std::sqrt(original_area));
        CurvatureMetric::SetCurvatureMetric(
            r_mesh,
            [&](unsigned int idx) { return vertices[idx] * s; },
            [&](unsigned int idx) { return curvature_normals[idx]; },
            PrincipalCurvatureFunctor);

        metric_update_timer.tock();
    }

    metric_timer.tock();
}

} // namespace smoothing_utils

// Explicit template instantiations
#define LA_X_smoothing_utils(_, Scalar, Index)                              \
    template void smoothing_utils::get_triangles<Scalar, Index>(            \
        const SurfaceMesh<Scalar, Index>& t_mesh,                           \
        std::vector<SimplexIndex<K, int>>& triangles);                      \
    template void smoothing_utils::get_vertices_and_normals<Scalar, Index>( \
        const SurfaceMesh<Scalar, Index>& t_mesh,                           \
        std::vector<Vector<Real, Dim>>& vertices,                           \
        std::vector<Vector<Real, Dim>>& normals,                            \
        AttributeId normal_id);                                             \
    template void smoothing_utils::set_vertices<Scalar, Index>(             \
        SurfaceMesh<Scalar, Index> & mesh,                                  \
        const std::vector<Vector<Real, Dim>>& vertices);                    \
    template std::unique_ptr<FEM::RiemannianMesh<Real>>                     \
        smoothing_utils::setup_for_smoothing<Scalar, Index>(                \
            SurfaceMesh<Scalar, Index> & mesh,                              \
            SurfaceMesh<Scalar, Index> & _mesh,                             \
            std::vector<SimplexIndex<K, int>> & triangles,                  \
            std::vector<Vector<Real, Dim>> & vertices,                      \
            std::vector<Vector<Real, Dim>> & normals,                       \
            Solver & solver,                                                \
            Real & original_area);
LA_SURFACE_MESH_X(smoothing_utils, 0)

} // namespace lagrange::filtering
