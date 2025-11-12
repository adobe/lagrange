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
#include <lagrange/geodesic/GeodesicEngineHeat.h>

#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/geodesic/api.h>
#include <lagrange/internal/find_attribute_utils.h>
#include <lagrange/views.h>

#include "geometry_central_utils.h"

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <geometrycentral/surface/vector_heat_method.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <memory>
#include <optional>

namespace lagrange::geodesic {

using gcSurfaceMesh = gc::gcSurfaceMesh;
using gcGeometry = gc::gcGeometry;
using gcSurfacePoint = gc::gcSurfacePoint;

template <typename Scalar, typename Index>
struct GeodesicEngineHeat<Scalar, Index>::Impl
{
    std::unique_ptr<gcSurfaceMesh> m_gc_mesh;
    std::unique_ptr<gcGeometry> m_gc_geom;
    std::optional<geometrycentral::surface::HeatMethodDistanceSolver> m_solver;
};

template <typename Scalar, typename Index>
GeodesicEngineHeat<Scalar, Index>::GeodesicEngineHeat(Mesh& mesh)
    : Super(mesh)
    , m_impl(lagrange::make_value_ptr<Impl>())
{
    auto [gc_mesh, gc_geom] = gc::extract_gc_mesh(this->mesh());
    m_impl->m_gc_mesh = std::move(gc_mesh);
    m_impl->m_gc_geom = std::move(gc_geom);
    m_impl->m_solver.emplace(*m_impl->m_gc_geom);
}

template <typename Scalar, typename Index>
GeodesicEngineHeat<Scalar, Index>::~GeodesicEngineHeat() = default;
template <typename Scalar, typename Index>
GeodesicEngineHeat<Scalar, Index>::GeodesicEngineHeat(GeodesicEngineHeat<Scalar, Index>&&) =
    default;
template <typename Scalar, typename Index>
GeodesicEngineHeat<Scalar, Index>& GeodesicEngineHeat<Scalar, Index>::operator=(
    GeodesicEngineHeat<Scalar, Index>&&) = default;

template <typename Scalar, typename Index>
SingleSourceGeodesicResult GeodesicEngineHeat<Scalar, Index>::single_source_geodesic(
    const SingleSourceGeodesicOptions& options)
{
    gcSurfacePoint seed_point(
        m_impl->m_gc_mesh->face(options.source_facet_id),
        geometrycentral::Vector3{
            1.0 - options.source_facet_bc[0] - options.source_facet_bc[1],
            options.source_facet_bc[0],
            options.source_facet_bc[1]});

    auto gc_distances = m_impl->m_solver->computeDistance(seed_point);

    auto geodesic_distance_id = internal::find_or_create_attribute<Scalar>(
        this->mesh(),
        options.output_geodesic_attribute_name,
        AttributeElement::Vertex,
        AttributeUsage::Scalar,
        1,
        internal::ResetToDefault::No);

    auto la_distance = attribute_vector_ref<Scalar>(this->mesh(), geodesic_distance_id);
    Index count = 0;
    for (auto v : m_impl->m_gc_mesh->vertices()) {
        la_distance[count] = static_cast<Scalar>(gc_distances[v]);
        count++;
    }

    return {geodesic_distance_id, invalid_attribute_id()};
}

#define LA_X_GeodesicEngineHeat(_, Scalar, Index) \
    template class LA_GEODESIC_API GeodesicEngineHeat<Scalar, Index>;
LA_SURFACE_MESH_X(GeodesicEngineHeat, 0)

} // namespace lagrange::geodesic
