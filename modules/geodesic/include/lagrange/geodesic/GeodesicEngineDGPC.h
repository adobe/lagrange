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
/// Computes surface geodesics using the Discrete Geodesic Polar Coordinates (DGPC) method. DGPC is
/// fast and relatively accurate near the source point, but accuracy degrades with distance from the
/// source.
///
/// @tparam     Scalar  Mesh scalar type.
/// @tparam     Index   Mesh index type.
///
template <typename Scalar, typename Index>
class GeodesicEngineDGPC : public GeodesicEngine<Scalar, Index>
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
    explicit GeodesicEngineDGPC(Mesh& mesh);

    ///
    /// Destructor.
    ///
    virtual ~GeodesicEngineDGPC() = default;

    ///
    /// Compute discrete geodesic polar coordinates for each vertex within the local neighborhood of
    /// seed point.
    ///
    /// This function will create two new attributes in the mesh:
    ///
    /// - `options.output_geodesic_attribute_name`: The geodesic distance from the seed point to each
    ///   vertex.
    /// - `options.output_polar_angle_attribute_name`: The geodesic polar coordinates of each vertex.
    ///
    /// Together, they define a logarithmic map of the mesh around the seed point.
    ///
    /// This function is roughly based on the following paper:
    ///
    /// - Melvær, Eivind Lyche, and Martin Reimers. "Geodesic polar coordinates on polygonal meshes."
    ///   Computer Graphics Forum. Vol. 31. No. 8. Oxford, UK: Blackwell Publishing Ltd, 2012.
    ///
    /// @param      options  The options for the computation.
    ///
    /// @return     The attribute ids of the geodesic distance and polar angle attributes.
    ///
    SingleSourceGeodesicResult single_source_geodesic(
        const SingleSourceGeodesicOptions& options) override;

protected:
    AttributeId m_facet_normal_attr_id = invalid_attribute_id(); ///< Facet normal attribute id.
};

///
/// Helper function to create a DGPC geodesic engine.
///
/// @param      mesh    Input mesh.
///
/// @tparam     Scalar  Mesh scalar type.
/// @tparam     Index   Mesh index type.
///
/// @return     DGPC geodesic engine.
///
template <typename Scalar, typename Index>
GeodesicEngineDGPC<Scalar, Index> make_dgpc_engine(SurfaceMesh<Scalar, Index>& mesh)
{
    return GeodesicEngineDGPC<Scalar, Index>(mesh);
}

} // namespace lagrange::geodesic
