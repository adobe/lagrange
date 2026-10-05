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
#pragma once

#ifndef LAGRANGE_GEODESIC_WITH_GEOMETRYCENTRAL
    #error "GeodesicEngineFlip requires the lagrange::geodesic::geometrycentral component"
#endif

#include <lagrange/geodesic/GeodesicEngine.h>

namespace lagrange::geodesic {

///
/// Computes surface geodesics using the edge-flip method. This method finds geodesic paths by
/// iteratively flipping edges to straighten an initial Dijkstra path, producing high-quality
/// approximate geodesics efficiently.
///
/// @tparam     Scalar  Mesh scalar type.
/// @tparam     Index   Mesh index type.
///
/// Based on the following paper:
///
/// Sharp, Nicholas, and Keenan Crane. "You can find geodesic paths in triangle meshes by just
/// flipping edges." ACM Transactions on Graphics (SIGGRAPH Asia 2020).
///
template <typename Scalar, typename Index>
class GeodesicEngineFlip : public GeodesicEngine<Scalar, Index>
{
public:
    using Super = GeodesicEngine<Scalar, Index>; ///< Parent class type.
    using Mesh = typename Super::Mesh; ///< The mesh type.

    /// Options for the flip geodesic engine.
    struct Options
    {
        /// Maximum number of iterations for the iterative shortening procedure.
        /// If 0, uses the geometry-central default (unlimited).
        size_t max_iterations = 0;

        /// Fraction of the initial path length below which shortening stops.
        /// If 0, length-based early stopping is disabled.
        double max_relative_length_decrease = 0.0;
    };

public:
    ///
    /// Precompute any data required for repeated geodesic path computation.
    ///
    /// @param      mesh     Reference to the input mesh.
    /// @param      options  Options for the flip geodesic engine.
    ///
    explicit GeodesicEngineFlip(Mesh& mesh, const Options& options = {});

    virtual ~GeodesicEngineFlip();
    GeodesicEngineFlip(GeodesicEngineFlip&&);
    GeodesicEngineFlip& operator=(GeodesicEngineFlip&&);
    GeodesicEngineFlip(const GeodesicEngineFlip&) = delete;
    GeodesicEngineFlip& operator=(const GeodesicEngineFlip&) = delete;

    ///
    /// @note Single source geodesic is not supported by this engine. Use GeodesicEngineHeat or
    /// GeodesicEngineMMP instead.
    ///
    /// @throws lagrange::Error always.
    ///
    SingleSourceGeodesicResult single_source_geodesic(
        const SingleSourceGeodesicOptions& options) override;

    ///
    /// Compute the geodesic distance between two points using the edge-flip method.
    ///
    /// @note The source and target points are snapped to the nearest mesh vertex before computing
    /// the geodesic. This is a limitation of the flip-based algorithm.
    ///
    /// @param      options  The options for the computation.
    ///
    /// @return     The geodesic distance between the source and target points.
    ///
    Scalar point_to_point_geodesic(const PointToPointGeodesicOptions& options) override;

    ///
    /// Compute the geodesic path between two points using the edge-flip method.
    ///
    /// This function finds a geodesic path by first computing a Dijkstra path between the two
    /// closest mesh vertices, then iteratively shortening it via edge flips until the path is
    /// locally shortest.
    ///
    /// @note The source and target points are snapped to the nearest mesh vertex before computing
    /// the geodesic. This is a limitation of the flip-based algorithm.
    ///
    /// @param      options  The options for the path computation.
    ///
    /// @return     A GeodesicPathResult containing the ordered path points and segment facet
    ///             indices. Returns an empty result if no path exists.
    ///
    GeodesicPathResult<Scalar, Index> point_to_point_geodesic_path(
        const PointToPointGeodesicPathOptions& options) override;

protected:
    struct Impl;
    lagrange::value_ptr<Impl> m_impl;
};

///
/// Helper function to create a Flip geodesic engine.
///
/// @param      mesh     Input mesh.
/// @param      options  Options for the flip geodesic engine.
///
/// @tparam     Scalar   Mesh scalar type.
/// @tparam     Index    Mesh index type.
///
/// @return     Flip geodesic engine.
///
template <typename Scalar, typename Index>
GeodesicEngineFlip<Scalar, Index> make_flip_engine(
    SurfaceMesh<Scalar, Index>& mesh,
    const typename GeodesicEngineFlip<Scalar, Index>::Options& options = {})
{
    return GeodesicEngineFlip<Scalar, Index>(mesh, options);
}

} // namespace lagrange::geodesic
