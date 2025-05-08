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

#include <lagrange/filtering/attribute_smoothing.h>

#include "smoothing_utils.h"

#include <lagrange/Attribute.h>
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/foreach_attribute.h>
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
void scalar_attribute_smoothing(
    SurfaceMesh<Scalar, Index>& mesh,
    std::string_view attribute_name,
    const AttributeSmoothingOptions& options)
{
    VerboseTimer timer("attribute_smoothing");
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
    // Smooth the scalar field
    if (options.gradient_weight > 0 && options.gradient_modulation_scale != 1) {
        VerboseTimer smooth_timer("├── Smooth scalar field");
        smooth_timer.tick();

        par_foreach_named_attribute_write<
            AttributeElement::Vertex>(mesh, [&](std::string_view attr_name, auto& attr) {
            using AttributeType = std::decay_t<decltype(attr)>;
            using ValueType = typename AttributeType::ValueType;

            if (attribute_name == "" || attr_name != attribute_name) return;
            if (attr.get_usage() != AttributeUsage::Scalar) return;
            if (attr.get_num_channels() != 1) return;

            // Only smooth float or double attributes.
            if constexpr (std::is_same_v<ValueType, float> || std::is_same_v<ValueType, double>) {
                auto scalar_field = vector_ref(attr);

                // Low frequencies described in terms of values at vertices
                auto LowFrequencyVertexValues = [&](unsigned int v) { return scalar_field[v]; };

                // High frequencies described in terms of scaled values at vertices
                auto HighFrequencyVertexValues = [&](unsigned int v) {
                    la_debug_assert(v < vertices.size(), "Vertex index out of bounds");
                    return scalar_field[v] *
                           static_cast<ValueType>(options.gradient_modulation_scale);
                };

                auto smoothed_scalar_field =
                    GradientDomain::ProcessVertexVertex<Solver, ValueType, Real>(
                        solver,
                        *r_mesh,
                        Real(1.f),
                        static_cast<Real>(options.gradient_weight),
                        LowFrequencyVertexValues,
                        HighFrequencyVertexValues);

                for (size_t i = 0; i < static_cast<size_t>(scalar_field.size()); i++) {
                    scalar_field[i] = smoothed_scalar_field[i];
                }
            } else {
                if (attr_name == attribute_name) {
                    logger().warn(
                        "Attribute {} is not a float/double valued attribute. Skipping smoothing.",
                        attr_name);
                }
            }
        });

        smooth_timer.tock();
    }
    // Smooth the scalar field
    ///////////////////////////////

    timer.tock();
}

#define LA_X_scalar_attribute_smoothing(_, Scalar, Index)    \
    template void scalar_attribute_smoothing<Scalar, Index>( \
        SurfaceMesh<Scalar, Index> & mesh,                   \
        std::string_view,                                    \
        const AttributeSmoothingOptions& options);
LA_SURFACE_MESH_X(scalar_attribute_smoothing, 0)

} // namespace lagrange::filtering
