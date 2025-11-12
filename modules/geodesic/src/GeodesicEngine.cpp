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
#include <lagrange/geodesic/GeodesicEngine.h>

#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/views.h>

namespace lagrange::geodesic {

template <typename Scalar, typename Index>
GeodesicEngine<Scalar, Index>::GeodesicEngine(Mesh& mesh)
    : m_mesh(mesh)
{
    // TODO: Add support for 2D meshes
    if (mesh.get_dimension() != 3) {
        throw std::runtime_error("Input mesh must be 3D mesh.");
    }
    if (!mesh.is_triangle_mesh()) {
        throw std::runtime_error("Input mesh must be triangle mesh.");
    }
}

template <typename Scalar, typename Index>
Scalar GeodesicEngine<Scalar, Index>::point_to_point_geodesic(
    const PointToPointGeodesicOptions& options)
{
    // TODO: Override to perform early-stopping for DGPC and MMP algorithms.

    SingleSourceGeodesicOptions s_options;
    s_options.source_facet_id = options.source_facet_id;
    s_options.source_facet_bc = options.source_facet_bc;

    auto result = single_source_geodesic(s_options);
    auto geo_dists = attribute_vector_view<Scalar>(mesh(), result.geodesic_distance_id);

    auto facets = facet_view(mesh());
    return geo_dists(facets(options.target_facet_id, 0)) *
               (1.0 - options.target_facet_bc[0] - options.target_facet_bc[1]) +
           geo_dists(facets(options.target_facet_id, 1)) * options.target_facet_bc[0] +
           geo_dists(facets(options.target_facet_id, 2)) * options.target_facet_bc[1];
}

#define LA_X_GeodesicEngine(_, Scalar, Index) \
    template class LA_GEODESIC_API GeodesicEngine<Scalar, Index>;
LA_SURFACE_MESH_X(GeodesicEngine, 0)

} // namespace lagrange::geodesic
