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

#include <lagrange/SurfaceMesh.h>
#include <lagrange/geodesic/api.h>

#include <functional>

namespace lagrange::geodesic {

///
/// General options for one-to-many geodesic computations.
///
struct LA_GEODESIC_API SingleSourceGeodesicOptions
{
    /// The facet id of the seed facet.
    size_t source_facet_id = 0;

    /// The barycentric coordinates of the seed facet. Given a triangle (p1, p2, p3), the
    /// barycentric coordinates (u, v) are such that the surface point is represented by p = (1 - u -
    /// v) * p1 + u * p2 + v * p3.
    std::array<double, 2> source_facet_bc = {0.0f, 0.0f};

    /// The reference up direction for the geodesic polar coordinates.
    ///
    /// @note The projection of the reference up direction onto the tangent plane of the seed
    /// point will be used as the actual up tangent direction.
    std::array<double, 3> ref_dir = {0.0f, 1.0f, 0.0f};

    /// The secondary reference up direction for the geodesic polar coordinates.
    ///
    /// This direction will only be used as reference direction if `ref_dir` is perpendicular to the
    /// the seed facet.
    std::array<double, 3> second_ref_dir = {1.0f, 0.0f, 0.0f};

    /// The maximum geodesic distance from the seed point to consider.
    ///
    /// @note Negative value means there is no limit, and the entire mesh will be considered.
    ///
    /// @note Regions outside this distance will have `invalid<Scalar>` as geodesic distance and
    /// polar angle.
    double radius = -1.0f;

    /// The name of the output attribute to store the geodesic distance.
    std::string_view output_geodesic_attribute_name = "@geodesic_distance";

    /// The name of the output attribute to store the geodesic polar coordinates.
    std::string_view output_polar_angle_attribute_name = "@polar_angle";
};

///
/// Result of a single source geodesic computation.
///
struct LA_GEODESIC_API SingleSourceGeodesicResult
{
    /// The attribute id of the geodesic distance attribute.
    AttributeId geodesic_distance_id = invalid_attribute_id();

    /// The attribute id of the polar angle attribute, if available.
    AttributeId polar_angle_id = invalid_attribute_id();
};

///
/// General options for point-to-point geodesic computations.
///
struct LA_GEODESIC_API PointToPointGeodesicOptions
{
    /// Facet containing the source point.
    size_t source_facet_id = 0;

    /// Facets containing the target point.
    size_t target_facet_id = 0;

    /// Barycentric coordinates of the source point within the source facet. Given a triangle (p1,
    /// p2, p3), the barycentric coordinates (u, v) are such that the surface point is represented
    /// by p = (1 - u - v) * p1 + u * p2 + v * p3.
    std::array<double, 2> source_facet_bc = {0.0f, 0.0f};

    /// Barycentric coordinates of the target point within the target facet. Given a triangle (p1,
    /// p2, p3), the barycentric coordinates (u, v) are such that the surface point is represented
    /// by p = (1 - u - v) * p1 + u * p2 + v * p3.
    std::array<double, 2> target_facet_bc = {0.0f, 0.0f};
};

///
/// Engine that is used to compute geodesic distances on a surface mesh.
///
/// @tparam     Scalar  Mesh scalar type.
/// @tparam     Index   Mesh index type.
///
template <typename Scalar, typename Index>
class GeodesicEngine
{
public:
    /// The mesh type.
    using Mesh = SurfaceMesh<Scalar, Index>;

public:
public:
    ///
    /// Base class constructor.
    ///
    /// @param      mesh  Reference to the input mesh.
    ///
    explicit GeodesicEngine(Mesh& mesh);

    ///
    /// Base class destructor.
    ///
    virtual ~GeodesicEngine() = default;

    ///
    /// Computes geodesic distance from one source to each vertex on the mesh.
    ///
    /// @param[in]  options  Input options for single source geodesic computation.
    ///
    /// @return     The attribute id of the computed geodesic distance attribute.
    ///
    virtual SingleSourceGeodesicResult single_source_geodesic(
        const SingleSourceGeodesicOptions& options) = 0;

    ///
    /// Computes the geodesic distance between two points on the mesh.
    ///
    /// @param[in]  options  Input options for point-to-point geodesic computation.
    ///
    /// @return     The geodesic distance between the source and target points.
    ///
    virtual Scalar point_to_point_geodesic(const PointToPointGeodesicOptions& options);

protected:
    const Mesh& mesh() const { return m_mesh.get(); }
    Mesh& mesh() { return m_mesh.get(); }

private:
    std::reference_wrapper<Mesh> m_mesh; ///< Reference to the input mesh.
};

} // namespace lagrange::geodesic
