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

#include <lagrange/geodesic/GeodesicEngineDGPC.h>
#include <lagrange/geodesic/GeodesicEngineHeat.h>
#include <lagrange/geodesic/GeodesicEngineMMP.h>
#include <lagrange/python/binding.h>
#include <lagrange/python/geodesic.h>

#include <array>
#include <string>
#include <variant>

namespace nb = nanobind;
using namespace nb::literals;

namespace lagrange::python {

void populate_geodesic_module(nb::module_& m)
{
    using namespace lagrange::geodesic;

    using Scalar = double;
    using Index = uint32_t;

    auto def_point_to_point_geodesic = [](auto& cls) {
        using EngineType = typename std::decay_t<decltype(cls)>::Type;
        cls.def(
            "point_to_point_geodesic",
            [](EngineType& self,
               size_t source_facet_id,
               size_t target_facet_id,
               std::array<double, 2> source_facet_bc,
               std::array<double, 2> target_facet_bc) {
                geodesic::PointToPointGeodesicOptions options;
                options.source_facet_id = source_facet_id;
                options.target_facet_id = target_facet_id;
                options.source_facet_bc = source_facet_bc;
                options.target_facet_bc = target_facet_bc;
                return self.point_to_point_geodesic(options);
            },
            "source_facet_id"_a,
            "target_facet_id"_a,
            "source_facet_bc"_a,
            "target_facet_bc"_a,
            R"(Compute geodesic distance between two points on the mesh.

:param source_facet_id: Facet containing the source point.
:param target_facet_id: Facet containing the target point.
:param source_facet_bc: Barycentric coordinates of the source point within the source facet. Given a triangle (p1, p2, p3), the barycentric coordinates (u, v) are such that the surface point is represented by p = (1 - u - v) * p1 + u * p2 + v * p3.
:param target_facet_bc: Barycentric coordinates of the target point within the target facet. Given a triangle (p1, p2, p3), the barycentric coordinates (u, v) are such that the surface point is represented by p = (1 - u - v) * p1 + u * p2 + v * p3.

:returns: The geodesic distance between the two points.)");
    };

    // DGPC engine
    nb::class_<GeodesicEngineDGPC<Scalar, Index>> cls_dgpc(m, "GeodesicEngineDGPC");
    cls_dgpc.def(nb::init<lagrange::SurfaceMesh<Scalar, Index>&>())
        .def(
            "single_source_geodesic",
            [](GeodesicEngineDGPC<Scalar, Index>& self,
               size_t source_facet_id,
               std::array<double, 2> source_facet_bc,
               std::array<double, 3> ref_dir,
               std::array<double, 3> second_ref_dir,
               double radius,
               std::string_view output_geodesic_attribute_name,
               std::string_view output_polar_angle_attribute_name) {
                geodesic::SingleSourceGeodesicOptions options;
                options.source_facet_id = source_facet_id;
                options.source_facet_bc = source_facet_bc;
                options.ref_dir = ref_dir;
                options.second_ref_dir = second_ref_dir;
                options.radius = radius;
                options.output_geodesic_attribute_name = output_geodesic_attribute_name;
                options.output_polar_angle_attribute_name = output_polar_angle_attribute_name;
                auto result = self.single_source_geodesic(options);
                return std::make_tuple(result.geodesic_distance_id, result.polar_angle_id);
            },
            "source_facet_id"_a,
            "source_facet_bc"_a,
            "ref_dir"_a = geodesic::SingleSourceGeodesicOptions().ref_dir,
            "second_ref_dir"_a = geodesic::SingleSourceGeodesicOptions().second_ref_dir,
            "radius"_a = geodesic::SingleSourceGeodesicOptions().radius,
            "output_geodesic_attribute_name"_a =
                geodesic::SingleSourceGeodesicOptions().output_geodesic_attribute_name,
            "output_polar_angle_attribute_name"_a =
                geodesic::SingleSourceGeodesicOptions().output_polar_angle_attribute_name,
            R"(Compute single-source geodesic distances on the mesh.

:param source_facet_id: The facet ID of the seed facet.
:param source_facet_bc: The barycentric coordinates of the seed facet. Given a triangle (p1, p2, p3), the barycentric coordinates (u, v) are such that the surface point is represented by p = (1 - u - v) * p1 + u * p2 + v * p3.
:param ref_dir: The reference up direction for the geodesic polar coordinates.
:param second_ref_dir: The secondary reference up direction for the geodesic polar coordinates
:param radius: The maximum geodesic distance from the seed point to consider. Negative value means no limit.
:param output_geodesic_attribute_name: The name of the output attribute to store the geodesic distance.
:param output_polar_angle_attribute_name: The name of the output attribute to store the geodesic polar coordinates.

:returns: The attribute IDs of the computed geodesic distance and polar angle attributes.)");
    def_point_to_point_geodesic(cls_dgpc);

    // Heat engine
    nb::class_<GeodesicEngineHeat<Scalar, Index>> cls_heat(m, "GeodesicEngineHeat");
    cls_heat.def(nb::init<lagrange::SurfaceMesh<Scalar, Index>&>())
        .def(
            "single_source_geodesic",
            [](GeodesicEngineHeat<Scalar, Index>& self,
               size_t source_facet_id,
               std::array<double, 2> source_facet_bc,
               std::string_view output_geodesic_attribute_name) {
                geodesic::SingleSourceGeodesicOptions options;
                options.source_facet_id = source_facet_id;
                options.source_facet_bc = source_facet_bc;
                options.output_geodesic_attribute_name = output_geodesic_attribute_name;
                return self.single_source_geodesic(options).geodesic_distance_id;
            },
            "source_facet_id"_a,
            "source_facet_bc"_a,
            "output_geodesic_attribute_name"_a =
                geodesic::SingleSourceGeodesicOptions().output_geodesic_attribute_name,
            R"(Compute single-source geodesic distances on the mesh.

:param source_facet_id: The facet ID of the seed facet.
:param source_facet_bc: The barycentric coordinates of the seed facet. Given a triangle (p1, p2, p3), the barycentric coordinates (u, v) are such that the surface point is represented by p = (1 - u - v) * p1 + u * p2 + v * p3.
:param output_geodesic_attribute_name: The name of the output attribute to store the geodesic distance.

:returns: The attribute ID of the computed geodesic distance attributes.)");
    def_point_to_point_geodesic(cls_heat);

    // MMP engine
    nb::class_<GeodesicEngineMMP<Scalar, Index>> cls_mmp(m, "GeodesicEngineMMP");
    cls_mmp.def(nb::init<lagrange::SurfaceMesh<Scalar, Index>&>())
        .def(
            "single_source_geodesic",
            [](GeodesicEngineMMP<Scalar, Index>& self,
               size_t source_facet_id,
               std::array<double, 2> source_facet_bc,
               double radius,
               std::string_view output_geodesic_attribute_name) {
                geodesic::SingleSourceGeodesicOptions options;
                options.source_facet_id = source_facet_id;
                options.source_facet_bc = source_facet_bc;
                options.radius = radius;
                options.output_geodesic_attribute_name = output_geodesic_attribute_name;
                return self.single_source_geodesic(options).geodesic_distance_id;
            },
            "source_facet_id"_a,
            "source_facet_bc"_a,
            "radius"_a = geodesic::SingleSourceGeodesicOptions().radius,
            "output_geodesic_attribute_name"_a =
                geodesic::SingleSourceGeodesicOptions().output_geodesic_attribute_name,
            R"(Compute single-source geodesic distances on the mesh.

:param source_facet_id: The facet ID of the seed facet.
:param source_facet_bc: The barycentric coordinates of the seed facet. Given a triangle (p1, p2, p3), the barycentric coordinates (u, v) are such that the surface point is represented by p = (1 - u - v) * p1 + u * p2 + v * p3.
:param radius: The maximum geodesic distance from the seed point to consider. Negative value means no limit.
:param output_geodesic_attribute_name: The name of the output attribute to store the geodesic distance.

:returns: The attribute ID of the computed geodesic distance attributes.)");
    def_point_to_point_geodesic(cls_mmp);
}

} // namespace lagrange::python
