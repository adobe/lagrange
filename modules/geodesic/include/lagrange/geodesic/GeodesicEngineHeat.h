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

#include <lagrange/geodesic/GeodesicEngine.h>

namespace lagrange::geodesic {

///
/// Computes surface geodesics using the heat method. The heat method offers fast geodesic
/// computation for all points on the mesh, at the expense of some accuracy compared to exact
/// methods.
///
/// @tparam     Scalar  Mesh scalar type.
/// @tparam     Index   Mesh index type.
///
template <typename Scalar, typename Index>
class GeodesicEngineHeat : public GeodesicEngine<Scalar, Index>
{
public:
    using Super = GeodesicEngine<Scalar, Index>; ///< Parent class type.
    using Mesh = typename Super::Mesh; ///< The mesh type.

public:
    ///
    /// Precompute any data required for repeated geodesic distance computation.
    ///
    /// @param      mesh  Reference to the input mesh.
    ///
    explicit GeodesicEngineHeat(Mesh& mesh);

    virtual ~GeodesicEngineHeat();
    GeodesicEngineHeat(GeodesicEngineHeat&&);
    GeodesicEngineHeat& operator=(GeodesicEngineHeat&&);
    GeodesicEngineHeat(const GeodesicEngineHeat&) = delete;
    GeodesicEngineHeat& operator=(const GeodesicEngineHeat&) = delete;

    ///
    /// Compute single source geodesic distances using the heat method.
    ///
    /// This function only computes a distance, and does not compute polar angles. It is based on
    /// the following paper:
    ///
    /// - Crane, Keenan, Clarisse Weischedel, and Max Wardetzky. "Geodesics in heat: A new approach
    ///   to computing distance based on heat flow." ACM Transactions on Graphics (TOG) 32.5 (2013):
    ///   1-11.
    ///
    /// @param      options  The options for the computation.
    ///
    /// @return     The attribute ids of the geodesic distance (id for polar angle will be invalid).
    ///
    SingleSourceGeodesicResult single_source_geodesic(
        const SingleSourceGeodesicOptions& options) override;

protected:
    struct Impl;
    lagrange::value_ptr<Impl> m_impl;
};

///
/// Helper function to create a Heat geodesic engine.
///
/// @param      mesh    Input mesh.
///
/// @tparam     Scalar  Mesh scalar type.
/// @tparam     Index   Mesh index type.
///
/// @return     DGPC geodesic engine.
///
template <typename Scalar, typename Index>
GeodesicEngineHeat<Scalar, Index> make_heat_engine(SurfaceMesh<Scalar, Index>& mesh)
{
    return GeodesicEngineHeat<Scalar, Index>(mesh);
}

} // namespace lagrange::geodesic
