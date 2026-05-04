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
#include <lagrange/geodesic/GeodesicEngineMMP.h>

#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/geodesic/api.h>
#include <lagrange/internal/find_attribute_utils.h>
#include <lagrange/utils/assert.h>
#include <lagrange/views.h>

#include "geometry_central_utils.h"

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <geometrycentral/surface/exact_geodesics.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <memory>
#include <optional>
#include <vector>

namespace lagrange::geodesic {

using gcSurfaceMesh = gc::gcSurfaceMesh;
using gcGeometry = gc::gcGeometry;
using gcSurfacePoint = gc::gcSurfacePoint;

template <typename Scalar, typename Index>
struct GeodesicEngineMMP<Scalar, Index>::Impl
{
    std::unique_ptr<gcSurfaceMesh> m_gc_mesh;
    std::unique_ptr<gcGeometry> m_gc_geom;
    std::optional<geometrycentral::surface::GeodesicAlgorithmExact> m_solver;
};

template <typename Scalar, typename Index>
GeodesicEngineMMP<Scalar, Index>::GeodesicEngineMMP(Mesh& mesh)
    : Super(mesh)
    , m_impl(lagrange::make_value_ptr<Impl>())
{
    auto [gc_mesh, gc_geom] = gc::extract_gc_mesh(this->mesh());
    m_impl->m_gc_mesh = std::move(gc_mesh);
    m_impl->m_gc_geom = std::move(gc_geom);
    m_impl->m_solver.emplace(*m_impl->m_gc_mesh, *m_impl->m_gc_geom);
}

template <typename Scalar, typename Index>
GeodesicEngineMMP<Scalar, Index>::~GeodesicEngineMMP() = default;
template <typename Scalar, typename Index>
GeodesicEngineMMP<Scalar, Index>::GeodesicEngineMMP(GeodesicEngineMMP<Scalar, Index>&&) = default;
template <typename Scalar, typename Index>
GeodesicEngineMMP<Scalar, Index>& GeodesicEngineMMP<Scalar, Index>::operator=(
    GeodesicEngineMMP<Scalar, Index>&&) = default;

template <typename Scalar, typename Index>
SingleSourceGeodesicResult GeodesicEngineMMP<Scalar, Index>::single_source_geodesic(
    const SingleSourceGeodesicOptions& options)
{
    gcSurfacePoint seed_point(
        m_impl->m_gc_mesh->face(options.source_facet_id),
        geometrycentral::Vector3{
            1.0 - options.source_facet_bc[0] - options.source_facet_bc[1],
            options.source_facet_bc[0],
            options.source_facet_bc[1]});

    m_impl->m_solver->propagate(seed_point, options.radius);
    auto gc_distances = m_impl->m_solver->getDistanceFunction();

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

template <typename Scalar, typename Index>
GeodesicPathResult<Scalar, Index> GeodesicEngineMMP<Scalar, Index>::point_to_point_geodesic_path(
    const PointToPointGeodesicPathOptions& options)
{
    // Create source and target surface points
    gcSurfacePoint source_point(
        m_impl->m_gc_mesh->face(options.source_facet_id),
        geometrycentral::Vector3{
            1.0 - options.source_facet_bc[0] - options.source_facet_bc[1],
            options.source_facet_bc[0],
            options.source_facet_bc[1]});

    gcSurfacePoint target_point(
        m_impl->m_gc_mesh->face(options.target_facet_id),
        geometrycentral::Vector3{
            1.0 - options.target_facet_bc[0] - options.target_facet_bc[1],
            options.target_facet_bc[0],
            options.target_facet_bc[1]});

    // Propagate from source
    m_impl->m_solver->propagate(source_point);

    // Trace back path from target to source
    std::vector<gcSurfacePoint> gc_path = m_impl->m_solver->traceBack(target_point);

    GeodesicPathResult<Scalar, Index> result;

    if (gc_path.empty()) {
        return result;
    }

    // Convert geometry-central path to result vectors
    // Note: geometry-central returns path from target to source, so we reverse it
    const size_t n = gc_path.size();
    result.points.resize(n);
    result.facet_ids.resize(n - 1);

    for (size_t i = 0; i < n; ++i) {
        const auto& sp = gc_path[n - 1 - i];
        geometrycentral::Vector3 pos = sp.interpolate(m_impl->m_gc_geom->inputVertexPositions);
        result.points[i] = {
            static_cast<Scalar>(pos.x),
            static_cast<Scalar>(pos.y),
            static_cast<Scalar>(pos.z)};
    }

    // Determine the facet for each path segment using the shared face between consecutive points
    for (size_t i = 0; i + 1 < n; ++i) {
        const auto& sp_a = gc_path[n - 1 - i];
        const auto& sp_b = gc_path[n - 2 - i];
        auto f = geometrycentral::surface::sharedFace(sp_a, sp_b);
        la_runtime_assert(
            f != geometrycentral::surface::Face(),
            "No shared face found for path segment");
        result.facet_ids[i] = static_cast<Index>(f.getIndex());
    }

    return result;
}

#define LA_X_GeodesicEngineMMP(_, Scalar, Index) \
    template class LA_GEODESIC_API GeodesicEngineMMP<Scalar, Index>;
LA_SURFACE_MESH_X(GeodesicEngineMMP, 0)

} // namespace lagrange::geodesic
