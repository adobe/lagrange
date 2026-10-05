/*
 * Copyright 2026 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/geodesic/GeodesicEngineFlip.h>

#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/geodesic/api.h>
#include <lagrange/utils/Error.h>
#include <lagrange/utils/assert.h>
#include <lagrange/views.h>

#include "geometry_central_utils.h"

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <geometrycentral/surface/flip_geodesics.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <memory>

namespace lagrange::geodesic {

using gcManifoldMesh = gc::gcManifoldMesh;
using gcGeometry = gc::gcGeometry;
using gcSurfacePoint = gc::gcSurfacePoint;

template <typename Scalar, typename Index>
struct GeodesicEngineFlip<Scalar, Index>::Impl
{
    std::unique_ptr<gcManifoldMesh> m_gc_mesh;
    std::unique_ptr<gcGeometry> m_gc_geom;
    Options m_options;
};

template <typename Scalar, typename Index>
GeodesicEngineFlip<Scalar, Index>::GeodesicEngineFlip(Mesh& mesh, const Options& options)
    : Super(mesh)
    , m_impl(lagrange::make_value_ptr<Impl>())
{
    auto [gc_mesh, gc_geom] = gc::extract_gc_manifold_mesh(this->mesh());
    m_impl->m_gc_mesh = std::move(gc_mesh);
    m_impl->m_gc_geom = std::move(gc_geom);
    m_impl->m_options = options;
}

/// @cond LA_INTERNAL_DOCS
template <typename Scalar, typename Index>
GeodesicEngineFlip<Scalar, Index>::~GeodesicEngineFlip() = default;
template <typename Scalar, typename Index>
GeodesicEngineFlip<Scalar, Index>::GeodesicEngineFlip(GeodesicEngineFlip<Scalar, Index>&&) =
    default;
template <typename Scalar, typename Index>
GeodesicEngineFlip<Scalar, Index>& GeodesicEngineFlip<Scalar, Index>::operator=(
    GeodesicEngineFlip<Scalar, Index>&&) = default;
/// @endcond

template <typename Scalar, typename Index>
SingleSourceGeodesicResult GeodesicEngineFlip<Scalar, Index>::single_source_geodesic(
    const SingleSourceGeodesicOptions& /*options*/)
{
    throw Error(
        "Single source geodesic is not supported by GeodesicEngineFlip. "
        "Use GeodesicEngineHeat or GeodesicEngineMMP instead.");
}

namespace {

/// Find the nearest vertex to a surface point specified by facet and barycentric coordinates.
geometrycentral::surface::Vertex
find_nearest_vertex(gcManifoldMesh& gc_mesh, size_t facet_id, const std::array<double, 2>& facet_bc)
{
    gcSurfacePoint sp(
        gc_mesh.face(facet_id),
        geometrycentral::Vector3{1.0 - facet_bc[0] - facet_bc[1], facet_bc[0], facet_bc[1]});
    return sp.nearestVertex();
}

} // namespace

template <typename Scalar, typename Index>
Scalar GeodesicEngineFlip<Scalar, Index>::point_to_point_geodesic(
    const PointToPointGeodesicOptions& options)
{
    auto source_vertex =
        find_nearest_vertex(*m_impl->m_gc_mesh, options.source_facet_id, options.source_facet_bc);
    auto target_vertex =
        find_nearest_vertex(*m_impl->m_gc_mesh, options.target_facet_id, options.target_facet_bc);

    if (source_vertex == target_vertex) {
        return Scalar(0);
    }

    auto network = geometrycentral::surface::FlipEdgeNetwork::constructFromDijkstraPath(
        *m_impl->m_gc_mesh,
        *m_impl->m_gc_geom,
        source_vertex,
        target_vertex);

    la_runtime_assert(network != nullptr, "Failed to construct flip edge network");

    network->iterativeShorten(
        m_impl->m_options.max_iterations == 0 ? geometrycentral::INVALID_IND
                                              : m_impl->m_options.max_iterations,
        m_impl->m_options.max_relative_length_decrease);

    return static_cast<Scalar>(network->length());
}

template <typename Scalar, typename Index>
GeodesicPathResult<Scalar, Index> GeodesicEngineFlip<Scalar, Index>::point_to_point_geodesic_path(
    const PointToPointGeodesicPathOptions& options)
{
    auto source_vertex =
        find_nearest_vertex(*m_impl->m_gc_mesh, options.source_facet_id, options.source_facet_bc);
    auto target_vertex =
        find_nearest_vertex(*m_impl->m_gc_mesh, options.target_facet_id, options.target_facet_bc);

    GeodesicPathResult<Scalar, Index> result;

    // Handle degenerate case: source and target are the same vertex
    if (source_vertex == target_vertex) {
        geometrycentral::Vector3 pos = m_impl->m_gc_geom->inputVertexPositions[source_vertex];
        result.points.push_back(
            {static_cast<Scalar>(pos.x), static_cast<Scalar>(pos.y), static_cast<Scalar>(pos.z)});
        return result;
    }

    // Construct a Dijkstra path and iteratively shorten it via edge flips
    auto network = geometrycentral::surface::FlipEdgeNetwork::constructFromDijkstraPath(
        *m_impl->m_gc_mesh,
        *m_impl->m_gc_geom,
        source_vertex,
        target_vertex);

    la_runtime_assert(network != nullptr, "Failed to construct flip edge network");

    // Iteratively shorten the path via edge flips until locally shortest
    network->iterativeShorten(
        m_impl->m_options.max_iterations == 0 ? geometrycentral::INVALID_IND
                                              : m_impl->m_options.max_iterations,
        m_impl->m_options.max_relative_length_decrease);

    // Extract the path as surface points on the original mesh
    auto polylines = network->getPathPolyline();
    la_runtime_assert(!polylines.empty(), "Flip geodesic produced no paths");

    // We expect a single path
    const auto& path_points = polylines[0];

    if (path_points.empty()) {
        return result;
    }

    const size_t n = path_points.size();
    result.points.resize(n);
    result.facet_ids.resize(n > 1 ? n - 1 : 0);

    for (size_t i = 0; i < n; ++i) {
        geometrycentral::Vector3 pos =
            path_points[i].interpolate(m_impl->m_gc_geom->inputVertexPositions);
        result.points[i] = {
            static_cast<Scalar>(pos.x),
            static_cast<Scalar>(pos.y),
            static_cast<Scalar>(pos.z)};
    }

    // Determine the facet for each path segment
    for (size_t i = 0; i + 1 < n; ++i) {
        auto f = geometrycentral::surface::sharedFace(path_points[i], path_points[i + 1]);
        la_runtime_assert(
            f != geometrycentral::surface::Face(),
            "No shared face found for path segment");
        result.facet_ids[i] = static_cast<Index>(f.getIndex());
    }

    return result;
}

#define LA_X_GeodesicEngineFlip(_, Scalar, Index) \
    template class LA_GEODESIC_API GeodesicEngineFlip<Scalar, Index>;
LA_SURFACE_MESH_X(GeodesicEngineFlip, 0)

} // namespace lagrange::geodesic
