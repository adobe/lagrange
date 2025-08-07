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

#include "smoothing_utils.h"

#include <lagrange/Attribute.h>
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/internal/visit_attribute.h>
#include <lagrange/triangulate_polygonal_facets.h>
#include <lagrange/utils/assert.h>
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

template <typename Scalar, typename Index>
void mesh_smoothing(SurfaceMesh<Scalar, Index>& mesh, const SmoothingOptions& options)
{
    VerboseTimer timer("mesh_smoothing");
    timer.tick();

    SurfaceMesh<Scalar, Index> _mesh;
    std::vector<SimplexIndex<K, int>> triangles;
    std::vector<Vector<Real, Dim>> vertices, normals;
    Solver solver;
    Real original_area;

    // Setup for smoothing
    auto r_mesh = smoothing_utils::setup_for_smoothing(
        mesh,
        _mesh,
        triangles,
        vertices,
        normals,
        solver,
        original_area);

    // Adjust the metric to take into account the curvature
    if (options.curvature_weight > 0) {
        smoothing_utils::adjust_metric_for_curvature(
            *r_mesh,
            vertices,
            normals,
            original_area,
            options.curvature_weight,
            options.normal_smoothing_weight,
            solver);
    }

    ///////////////////////////////
    // Smooth the positions/normals
    if (options.gradient_weight > 0 && options.gradient_modulation_scale != 1) {
        if (options.filter_method == SmoothingOptions::FilterMethod::VertexSmoothing) {
            VerboseTimer smooth_timer("├── Smooth positions");
            smooth_timer.tick();

            // Low frequencies described in terms of values at vertices
            auto LowFrequencyVertexValues = [&](unsigned int v) { return vertices[v]; };

            // High frequencies described in terms of scaled values at vertices
            auto HighFrequencyVertexValues = [&](unsigned int v) {
                la_debug_assert(v < vertices.size(), "Vertex index out of bounds");
                return vertices[v] * static_cast<Real>(options.gradient_modulation_scale);
            };

            vertices = GradientDomain::ProcessVertexVertex<Solver, Point3D<Real>, Real>(
                solver,
                *r_mesh,
                Real(1.f),
                static_cast<Real>(options.gradient_weight),
                LowFrequencyVertexValues,
                HighFrequencyVertexValues);

            smooth_timer.tock();
        } else if (options.filter_method == SmoothingOptions::FilterMethod::NormalSmoothing) {
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
                *r_mesh,
                Real(1.f),
                static_cast<Real>(options.gradient_weight),
                LowFrequencyVertexValues,
                HighFrequencyVertexValues);

            smooth_timer.tock();
        } else {
            throw std::runtime_error("Unsupported filter method");
        }
    }
    // Smooth the positions/normals
    ///////////////////////////////

    ///////////////////////////////////////////
    // Fit the geometry to the smoothed normals
    if (options.filter_method == SmoothingOptions::FilterMethod::NormalSmoothing) {
        VerboseTimer fit_timer("└── Fit geometry");
        fit_timer.tick();

        vertices = GradientDomain::FitToNormals<Solver>(
            solver,
            *r_mesh,
            Real(1.f),
            static_cast<Real>(options.normal_projection_weight),
            [&](unsigned int v) { return vertices[v]; },
            [&](unsigned int v) { return normals[v]; });

        fit_timer.tock();
    }
    // Fit the geometry to the smoothed normals
    ///////////////////////////////////////////

    smoothing_utils::set_vertices(mesh, vertices);
    timer.tock();
}

#define LA_X_mesh_reconstruction(_, Scalar, Index)                \
    template LA_FILTERING_API void mesh_smoothing<Scalar, Index>( \
        SurfaceMesh<Scalar, Index> & mesh,                        \
        const SmoothingOptions& options);
LA_SURFACE_MESH_X(mesh_reconstruction, 0)

} // namespace lagrange::filtering
