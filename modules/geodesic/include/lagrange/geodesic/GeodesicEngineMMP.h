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
/// Computes surface geodesics using the [MMP] algorithm. This is an exact method, which offers the
/// best accuracy overall, but can be slow for large meshes.
///
/// @tparam     Scalar  Mesh scalar type.
/// @tparam     Index   Mesh index type.
///
/// [MMP]: Mitchell, Joseph SB, David M. Mount, and Christos H. Papadimitriou. "The discrete geodesic
/// problem." SIAM Journal on Computing 16.4 (1987): 647-668.
///
template <typename Scalar, typename Index>
class GeodesicEngineMMP : public GeodesicEngine<Scalar, Index>
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
    explicit GeodesicEngineMMP(Mesh& mesh);

    virtual ~GeodesicEngineMMP();
    GeodesicEngineMMP(GeodesicEngineMMP&&);
    GeodesicEngineMMP& operator=(GeodesicEngineMMP&&);
    GeodesicEngineMMP(const GeodesicEngineMMP&) = delete;
    GeodesicEngineMMP& operator=(const GeodesicEngineMMP&) = delete;

    ///
    /// Compute single source geodesic distances using the MMP algorithm.
    ///
    /// This function only computes a distance, and does not compute polar angles. It is based on
    /// the following paper:
    ///
    /// - Mitchell, Joseph SB, David M. Mount, and Christos H. Papadimitriou. "The discrete geodesic
    ///   problem." SIAM Journal on Computing 16.4 (1987): 647-668.
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
/// Helper function to create a MMP geodesic engine.
///
/// @param      mesh    Input mesh.
///
/// @tparam     Scalar  Mesh scalar type.
/// @tparam     Index   Mesh index type.
///
/// @return     DGPC geodesic engine.
///
template <typename Scalar, typename Index>
GeodesicEngineMMP<Scalar, Index> make_mmp_engine(SurfaceMesh<Scalar, Index>& mesh)
{
    return GeodesicEngineMMP<Scalar, Index>(mesh);
}

} // namespace lagrange::geodesic
