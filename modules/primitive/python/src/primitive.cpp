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

#include <lagrange/primitive/generate_disc.h>
#include <lagrange/primitive/generate_icosahedron.h>
#include <lagrange/primitive/generate_octahedron.h>
#include <lagrange/primitive/generate_rounded_cone.h>
#include <lagrange/primitive/generate_rounded_cube.h>
#include <lagrange/primitive/generate_rounded_plane.h>
#include <lagrange/primitive/generate_sphere.h>
#include <lagrange/primitive/generate_subdivided_sphere.h>
#include <lagrange/primitive/generate_torus.h>
#include <lagrange/python/binding.h>
#include <lagrange/python/primitive.h>

namespace nb = nanobind;
using namespace nb::literals;

namespace lagrange::python {

void populate_primitive_module(nb::module_& m)
{
    using Scalar = double;
    using Index = uint32_t;
    using MeshType = SurfaceMesh<Scalar, Index>;
    using _Scalar = primitive::PrimitiveOptions::Scalar;

    {
        const primitive::RoundedConeOptions setting;
        m.def(
            "generate_rounded_cone",
            [](_Scalar radius_top,
               _Scalar radius_bottom,
               _Scalar height,
               _Scalar bevel_radius_top,
               _Scalar bevel_radius_bottom,
               Index radial_sections,
               Index bevel_segments_top,
               Index bevel_segments_bottom,
               Index side_segments,
               Index top_segments,
               Index bottom_segments,
               Scalar start_sweep_angle,
               Scalar end_sweep_angle,
               bool with_top_cap,
               bool with_bottom_cap,
               bool with_cross_section,
               bool triangulate,
               bool fixed_uv,
               std::string_view normal_attribute_name,
               std::string_view uv_attribute_name,
               std::string_view semantic_attribute_name,
               _Scalar dist_threshold,
               _Scalar angle_threshold,
               _Scalar epsilon,
               _Scalar uv_padding,
               std::array<_Scalar, 3> center) -> MeshType {
                primitive::RoundedConeOptions _setting;
                _setting.radius_top = radius_top;
                _setting.radius_bottom = radius_bottom;
                _setting.height = height;
                _setting.bevel_radius_top = bevel_radius_top;
                _setting.bevel_radius_bottom = bevel_radius_bottom;
                _setting.radial_sections = radial_sections;
                _setting.bevel_segments_top = bevel_segments_top;
                _setting.bevel_segments_bottom = bevel_segments_bottom;
                _setting.side_segments = side_segments;
                _setting.top_segments = top_segments;
                _setting.bottom_segments = bottom_segments;
                _setting.start_sweep_angle = start_sweep_angle;
                _setting.end_sweep_angle = end_sweep_angle;
                _setting.with_top_cap = with_top_cap;
                _setting.with_bottom_cap = with_bottom_cap;
                _setting.with_cross_section = with_cross_section;
                _setting.triangulate = triangulate;
                _setting.fixed_uv = fixed_uv;
                _setting.normal_attribute_name = normal_attribute_name;
                _setting.uv_attribute_name = uv_attribute_name;
                _setting.semantic_label_attribute_name = semantic_attribute_name;
                _setting.dist_threshold = dist_threshold;
                _setting.angle_threshold = angle_threshold;
                _setting.epsilon = epsilon;
                _setting.uv_padding = uv_padding;
                _setting.center = center;
                return primitive::generate_rounded_cone<Scalar, Index>(std::move(_setting));
            },
            "radius_top"_a = setting.radius_top,
            "radius_bottom"_a = setting.radius_bottom,
            "height"_a = setting.height,
            "bevel_radius_top"_a = setting.bevel_radius_top,
            "bevel_radius_bottom"_a = setting.bevel_radius_bottom,
            "radial_sections"_a = setting.radial_sections,
            "bevel_segments_top"_a = setting.bevel_segments_top,
            "bevel_segments_bottom"_a = setting.bevel_segments_bottom,
            "side_segments"_a = setting.side_segments,
            "top_segments"_a = setting.top_segments,
            "bottom_segments"_a = setting.bottom_segments,
            "start_sweep_angle"_a = setting.start_sweep_angle,
            "end_sweep_angle"_a = setting.end_sweep_angle,
            "with_top_cap"_a = setting.with_top_cap,
            "with_bottom_cap"_a = setting.with_bottom_cap,
            "with_cross_section"_a = setting.with_cross_section,
            "triangulate"_a = setting.triangulate,
            "fixed_uv"_a = setting.fixed_uv,
            "normal_attribute_name"_a = setting.normal_attribute_name,
            "uv_attribute_name"_a = setting.uv_attribute_name,
            "semantic_attribute_name"_a = setting.semantic_label_attribute_name,
            "dist_threshold"_a = setting.dist_threshold,
            "angle_threshold"_a = setting.angle_threshold,
            "epsilon"_a = setting.epsilon,
            "uv_padding"_a = setting.uv_padding,
            "center"_a = setting.center,
            R"(Generate a rounded cone mesh.

:param radius_top: The radius of the top of the cone.
:param radius_bottom: The radius of the bottom of the cone.
:param height: The height of the cone.
:param bevel_radius_top: The radius of the bevel on the top of the cone.
:param bevel_radius_bottom: The radius of the bevel on the bottom of the cone.
:param radial_sections: The number of radial sections of the cone.
:param bevel_segments_top: The number of segments on the bevel on the top of the cone.
:param bevel_segments_bottom: The number of segments on the bevel on the bottom of the cone.
:param side_segments: The number of segments on the side of the cone.
:param top_segments: The number of segments on the top of the cone.
:param bottom_segments: The number of segments on the bottom of the cone.
:param start_sweep_angle: The start sweep angle of the cone.
:param end_sweep_angle: The end sweep angle of the cone.
:param with_top_cap: Whether to include the top cap.
:param with_bottom_cap: Whether to include the bottom cap.
:param with_cross_section: Whether to include the cross section.
:param triangulate: Whether to triangulate the mesh.
:param fixed_uv: Whether to use fixed UVs.
:param normal_attribute_name: The name of the normal attribute.
:param uv_attribute_name: The name of the UV attribute.
:param semantic_attribute_name: The name of the semantic attribute.
:param dist_threshold: The distance threshold for merging vertices.
:param angle_threshold: The angle threshold for merging vertices.
:param epsilon: The epsilon for merging vertices.
:param uv_padding: The padding for the UVs.
:param center: The center of the cone.

:return: The generated mesh.)");
    }

    {
        primitive::SphereOptions setting;
        m.def(
            "generate_sphere",
            [](_Scalar radius,
               _Scalar start_sweep_angle,
               _Scalar end_sweep_angle,
               Index num_longitude_sections,
               Index num_latitude_sections,
               bool triangulate,
               bool fixed_uv,
               std::string_view normal_attribute_name,
               std::string_view uv_attribute_name,
               std::string_view semantic_attribute_name,
               _Scalar dist_threshold,
               _Scalar angle_threshold,
               _Scalar epsilon,
               _Scalar uv_padding,
               std::array<_Scalar, 3> center) -> MeshType {
                primitive::SphereOptions _setting;
                _setting.radius = radius;
                _setting.start_sweep_angle = start_sweep_angle;
                _setting.end_sweep_angle = end_sweep_angle;
                _setting.num_longitude_sections = num_longitude_sections;
                _setting.num_latitude_sections = num_latitude_sections;
                _setting.triangulate = triangulate;
                _setting.fixed_uv = fixed_uv;
                _setting.normal_attribute_name = normal_attribute_name;
                _setting.uv_attribute_name = uv_attribute_name;
                _setting.semantic_label_attribute_name = semantic_attribute_name;
                _setting.dist_threshold = dist_threshold;
                _setting.angle_threshold = angle_threshold;
                _setting.epsilon = epsilon;
                _setting.uv_padding = uv_padding;
                _setting.center = center;
                return primitive::generate_sphere<Scalar, Index>(std::move(_setting));
            },
            "radius"_a = setting.radius,
            "start_sweep_angle"_a = setting.start_sweep_angle,
            "end_sweep_angle"_a = setting.end_sweep_angle,
            "num_longitude_sections"_a = setting.num_longitude_sections,
            "num_latitude_sections"_a = setting.num_latitude_sections,
            "triangulate"_a = setting.triangulate,
            "fixed_uv"_a = setting.fixed_uv,
            "normal_attribute_name"_a = setting.normal_attribute_name,
            "uv_attribute_name"_a = setting.uv_attribute_name,
            "semantic_attribute_name"_a = setting.semantic_label_attribute_name,
            "dist_threshold"_a = setting.dist_threshold,
            "angle_threshold"_a = setting.angle_threshold,
            "epsilon"_a = setting.epsilon,
            "uv_padding"_a = setting.uv_padding,
            "center"_a = setting.center,
            R"(Generate a sphere mesh.

:param radius: The radius of the sphere.
:param start_sweep_angle: The starting sweep angle in radians.
:param end_sweep_angle: The ending sweep angle in radians.
:param num_longitude_sections: The number of sections along the longitude (vertical) direction.
:param num_latitude_sections: The number of sections along the latitude (horizontal) direction.
:param triangulate: Whether to triangulate the mesh.
:param fixed_uv: Whether to use fixed UVs.
:param normal_attribute_name: The name of the normal attribute.
:param uv_attribute_name: The name of the UV attribute.
:param semantic_attribute_name: The name of the semantic attribute.
:param dist_threshold: The distance threshold for merging vertices.
:param angle_threshold: The angle threshold for merging vertices.
:param epsilon: The epsilon for merging vertices.
:param uv_padding: The padding for the UVs.
:param center: The center of the sphere.

:return: The generated mesh.)");
    }

    {
        primitive::OctahedronOptions setting;
        m.def(
            "generate_octahedron",
            [](_Scalar radius,
               bool fixed_uv,
               std::string_view normal_attribute_name,
               std::string_view uv_attribute_name,
               std::string_view semantic_attribute_name,
               _Scalar dist_threshold,
               _Scalar angle_threshold,
               _Scalar epsilon,
               std::array<_Scalar, 3> center) -> MeshType {
                primitive::OctahedronOptions _setting;
                _setting.radius = radius;
                _setting.fixed_uv = fixed_uv;
                _setting.normal_attribute_name = normal_attribute_name;
                _setting.uv_attribute_name = uv_attribute_name;
                _setting.semantic_label_attribute_name = semantic_attribute_name;
                _setting.dist_threshold = dist_threshold;
                _setting.angle_threshold = angle_threshold;
                _setting.epsilon = epsilon;
                _setting.center = center;
                return primitive::generate_octahedron<Scalar, Index>(std::move(_setting));
            },
            "radius"_a = setting.radius,
            "fixed_uv"_a = setting.fixed_uv,
            "normal_attribute_name"_a = setting.normal_attribute_name,
            "uv_attribute_name"_a = setting.uv_attribute_name,
            "semantic_attribute_name"_a = setting.semantic_label_attribute_name,
            "dist_threshold"_a = setting.dist_threshold,
            "angle_threshold"_a = setting.angle_threshold,
            "epsilon"_a = setting.epsilon,
            "center"_a = setting.center,
            R"(Generate an octahedron mesh.

:param radius: The radius of the circumscribed sphere around the octahedron.
:param fixed_uv: Whether to use fixed UVs.
:param normal_attribute_name: The name of the normal attribute.
:param uv_attribute_name: The name of the UV attribute.
:param semantic_attribute_name: The name of the semantic attribute.
:param dist_threshold: The distance threshold for merging vertices.
:param angle_threshold: The angle threshold for merging vertices.
:param epsilon: The epsilon for merging vertices.
:param center: The center of the octahedron.

:return: The generated mesh.)");
    }

    {
        primitive::IcosahedronOptions setting;
        m.def(
            "generate_icosahedron",
            [](_Scalar radius,
               bool fixed_uv,
               std::string_view normal_attribute_name,
               std::string_view uv_attribute_name,
               std::string_view semantic_attribute_name,
               _Scalar angle_threshold,
               std::array<_Scalar, 3> center) -> MeshType {
                primitive::IcosahedronOptions _setting;
                _setting.radius = radius;
                _setting.fixed_uv = fixed_uv;
                _setting.normal_attribute_name = normal_attribute_name;
                _setting.uv_attribute_name = uv_attribute_name;
                _setting.semantic_label_attribute_name = semantic_attribute_name;
                _setting.angle_threshold = angle_threshold;
                _setting.center = center;
                return primitive::generate_icosahedron<Scalar, Index>(std::move(_setting));
            },
            "radius"_a = setting.radius,
            "fixed_uv"_a = setting.fixed_uv,
            "normal_attribute_name"_a = setting.normal_attribute_name,
            "uv_attribute_name"_a = setting.uv_attribute_name,
            "semantic_attribute_name"_a = setting.semantic_label_attribute_name,
            "angle_threshold"_a = setting.angle_threshold,
            "center"_a = setting.center,
            R"(Generate an icosahedron mesh.

:param radius: The radius of the circumscribed sphere around the icosahedron.
:param fixed_uv: Whether to use fixed UVs.
:param normal_attribute_name: The name of the normal attribute.
:param uv_attribute_name: The name of the UV attribute.
:param semantic_attribute_name: The name of the semantic attribute.
:param angle_threshold: The angle threshold for merging vertices.
:param center: The center of the icosahedron.

:return: The generated mesh.)");
    }

    {
        primitive::SubdividedSphereOptions setting;
        m.def(
            "generate_subdivided_sphere",
            [](const MeshType& base_shape,
               _Scalar radius,
               Index subdiv_level,
               bool fixed_uv,
               std::string_view normal_attribute_name,
               std::string_view uv_attribute_name,
               std::string_view semantic_attribute_name,
               _Scalar angle_threshold,
               std::array<_Scalar, 3> center) -> MeshType {
                primitive::SubdividedSphereOptions _setting;
                _setting.radius = radius;
                _setting.subdiv_level = subdiv_level;
                _setting.fixed_uv = fixed_uv;
                _setting.normal_attribute_name = normal_attribute_name;
                _setting.uv_attribute_name = uv_attribute_name;
                _setting.semantic_label_attribute_name = semantic_attribute_name;
                _setting.angle_threshold = angle_threshold;
                _setting.center = center;
                return primitive::generate_subdivided_sphere<Scalar, Index>(
                    base_shape,
                    std::move(_setting));
            },
            "base_shape"_a,
            "radius"_a = setting.radius,
            "subdiv_level"_a = setting.subdiv_level,
            "fixed_uv"_a = setting.fixed_uv,
            "normal_attribute_name"_a = setting.normal_attribute_name,
            "uv_attribute_name"_a = setting.uv_attribute_name,
            "semantic_attribute_name"_a = setting.semantic_label_attribute_name,
            "angle_threshold"_a = setting.angle_threshold,
            "center"_a = setting.center,
            R"(Generate a subdivided sphere mesh from a base shape.

:param base_shape: The base mesh to subdivide and project onto a sphere.
:param radius: The radius of the resulting sphere.
:param subdiv_level: The number of subdivision levels to apply.
:param fixed_uv: Whether to use fixed UVs.
:param normal_attribute_name: The name of the normal attribute.
:param uv_attribute_name: The name of the UV attribute.
:param semantic_attribute_name: The name of the semantic attribute.
:param angle_threshold: The angle threshold for merging vertices.
:param center: The center of the sphere.

:return: The generated subdivided sphere mesh.)");
    }

    {
        primitive::TorusOptions setting;
        m.def(
            "generate_torus",
            [](_Scalar major_radius,
               _Scalar minor_radius,
               Index ring_segments,
               Index pipe_segments,
               _Scalar start_sweep_angle,
               _Scalar end_sweep_angle,
               bool with_top_cap,
               bool with_bottom_cap,
               bool with_cross_section,
               bool triangulate,
               bool fixed_uv,
               std::string_view normal_attribute_name,
               std::string_view uv_attribute_name,
               std::string_view semantic_attribute_name,
               _Scalar dist_threshold,
               _Scalar angle_threshold,
               _Scalar epsilon,
               _Scalar uv_padding,
               std::array<_Scalar, 3> center) -> MeshType {
                primitive::TorusOptions _setting;
                _setting.major_radius = major_radius;
                _setting.minor_radius = minor_radius;
                _setting.ring_segments = ring_segments;
                _setting.pipe_segments = pipe_segments;
                _setting.start_sweep_angle = start_sweep_angle;
                _setting.end_sweep_angle = end_sweep_angle;
                _setting.with_top_cap = with_top_cap;
                _setting.with_bottom_cap = with_bottom_cap;
                _setting.with_cross_section = with_cross_section;
                _setting.triangulate = triangulate;
                _setting.fixed_uv = fixed_uv;
                _setting.normal_attribute_name = normal_attribute_name;
                _setting.uv_attribute_name = uv_attribute_name;
                _setting.semantic_label_attribute_name = semantic_attribute_name;
                _setting.dist_threshold = dist_threshold;
                _setting.angle_threshold = angle_threshold;
                _setting.epsilon = epsilon;
                _setting.uv_padding = uv_padding;
                _setting.center = center;

                return primitive::generate_torus<Scalar, Index>(std::move(_setting));
            },
            "major_radius"_a = setting.major_radius,
            "minor_radius"_a = setting.minor_radius,
            "ring_segments"_a = setting.ring_segments,
            "pipe_segments"_a = setting.pipe_segments,
            "start_sweep_angle"_a = setting.start_sweep_angle,
            "end_sweep_angle"_a = setting.end_sweep_angle,
            "with_top_cap"_a = setting.with_top_cap,
            "with_bottom_cap"_a = setting.with_bottom_cap,
            "with_cross_section"_a = setting.with_cross_section,
            "triangulate"_a = setting.triangulate,
            "fixed_uv"_a = setting.fixed_uv,
            "normal_attribute_name"_a = setting.normal_attribute_name,
            "uv_attribute_name"_a = setting.uv_attribute_name,
            "semantic_attribute_name"_a = setting.semantic_label_attribute_name,
            "dist_threshold"_a = setting.dist_threshold,
            "angle_threshold"_a = setting.angle_threshold,
            "epsilon"_a = setting.epsilon,
            "uv_padding"_a = setting.uv_padding,
            "center"_a = setting.center,
            R"(Generate a torus mesh.

:param major_radius: The major radius of the torus.
:param minor_radius: The minor radius of the torus.
:param ring_segments: The number of segments around the ring of the torus.
:param pipe_segments: The number of segments around the pipe of the torus.
:param start_sweep_angle: The start sweep angle of the torus.
:param end_sweep_angle: The end sweep angle of the torus.
:param with_top_cap: Whether to include the top cap.
:param with_bottom_cap: Whether to include the bottom cap.
:param with_cross_section: Whether to include the cross section.
:param triangulate: Whether to triangulate the mesh.
:param fixed_uv: Whether to use fixed UVs.
:param normal_attribute_name: The name of the normal attribute.
:param uv_attribute_name: The name of the UV attribute.
:param semantic_attribute_name: The name of the semantic attribute.
:param dist_threshold: The distance threshold for merging vertices.
:param angle_threshold: The angle threshold for merging vertices.
:param epsilon: The epsilon for merging vertices.
:param uv_padding: The padding for the UVs.
:param center: The center of the torus.

:return: The generated mesh.)");
    }

    {
        primitive::DiscOptions setting;
        m.def(
            "generate_disc",
            [](_Scalar radius,
               _Scalar start_angle,
               _Scalar end_angle,
               Index radial_sections,
               Index num_rings,
               bool triangulate,
               bool fixed_uv,
               std::string_view normal_attribute_name,
               std::string_view uv_attribute_name,
               std::string_view semantic_attribute_name,
               _Scalar dist_threshold,
               _Scalar angle_threshold,
               _Scalar epsilon,
               _Scalar uv_padding,
               std::array<_Scalar, 3> normal,
               std::array<_Scalar, 3> center) -> MeshType {
                primitive::DiscOptions _setting;
                _setting.radius = radius;
                _setting.start_angle = start_angle;
                _setting.end_angle = end_angle;
                _setting.radial_sections = radial_sections;
                _setting.num_rings = num_rings;
                _setting.triangulate = triangulate;
                _setting.fixed_uv = fixed_uv;
                _setting.normal_attribute_name = normal_attribute_name;
                _setting.uv_attribute_name = uv_attribute_name;
                _setting.semantic_label_attribute_name = semantic_attribute_name;
                _setting.dist_threshold = dist_threshold;
                _setting.angle_threshold = angle_threshold;
                _setting.epsilon = epsilon;
                _setting.uv_padding = uv_padding;
                _setting.normal = normal;
                _setting.center = center;
                return primitive::generate_disc<Scalar, Index>(std::move(_setting));
            },
            "radius"_a = setting.radius,
            "start_angle"_a = setting.start_angle,
            "end_angle"_a = setting.end_angle,
            "radial_sections"_a = setting.radial_sections,
            "num_rings"_a = setting.num_rings,
            "triangulate"_a = setting.triangulate,
            "fixed_uv"_a = setting.fixed_uv,
            "normal_attribute_name"_a = setting.normal_attribute_name,
            "uv_attribute_name"_a = setting.uv_attribute_name,
            "semantic_attribute_name"_a = setting.semantic_label_attribute_name,
            "dist_threshold"_a = setting.dist_threshold,
            "angle_threshold"_a = setting.angle_threshold,
            "epsilon"_a = setting.epsilon,
            "uv_padding"_a = setting.uv_padding,
            "normal"_a = setting.normal,
            "center"_a = setting.center,
            R"(Generate a disc mesh.

:param radius: The radius of the disc.
:param start_angle: The start angle of the disc in radians.
:param end_angle: The end angle of the disc in radians.
:param radial_sections: The number of radial sections (spokes) in the disc.
:param num_rings: The number of concentric rings in the disc.
:param triangulate: Whether to triangulate the mesh.
:param fixed_uv: Whether to use fixed UVs.
:param normal_attribute_name: The name of the normal attribute.
:param uv_attribute_name: The name of the UV attribute.
:param semantic_attribute_name: The name of the semantic attribute.
:param dist_threshold: The distance threshold for merging vertices.
:param angle_threshold: The angle threshold for merging vertices.
:param epsilon: The epsilon for merging vertices.
:param uv_padding: The padding for the UVs.
:param normal: The normal vector of the disc.
:param center: The center of the disc.

:return: The generated mesh.)");
    }

    {
        primitive::RoundedCubeOptions setting;
        m.def(
            "generate_rounded_cube",
            [](_Scalar width,
               _Scalar height,
               _Scalar depth,
               Index width_segments,
               Index height_segments,
               Index depth_segments,
               _Scalar bevel_radius,
               Index bevel_segments,
               bool triangulate,
               bool fixed_uv,
               std::string_view normal_attribute_name,
               std::string_view uv_attribute_name,
               std::string_view semantic_attribute_name,
               _Scalar dist_threshold,
               _Scalar angle_threshold,
               _Scalar epsilon,
               _Scalar uv_padding,
               std::array<_Scalar, 3> center) -> MeshType {
                primitive::RoundedCubeOptions _setting;
                _setting.width = width;
                _setting.height = height;
                _setting.depth = depth;
                _setting.width_segments = width_segments;
                _setting.height_segments = height_segments;
                _setting.depth_segments = depth_segments;
                _setting.bevel_radius = bevel_radius;
                _setting.bevel_segments = bevel_segments;
                _setting.triangulate = triangulate;
                _setting.fixed_uv = fixed_uv;
                _setting.normal_attribute_name = normal_attribute_name;
                _setting.uv_attribute_name = uv_attribute_name;
                _setting.semantic_label_attribute_name = semantic_attribute_name;
                _setting.dist_threshold = dist_threshold;
                _setting.angle_threshold = angle_threshold;
                _setting.epsilon = epsilon;
                _setting.uv_padding = uv_padding;
                _setting.center = center;
                return primitive::generate_rounded_cube<Scalar, Index>(std::move(_setting));
            },
            "width"_a = setting.width,
            "height"_a = setting.height,
            "depth"_a = setting.depth,
            "width_segments"_a = setting.width_segments,
            "height_segments"_a = setting.height_segments,
            "depth_segments"_a = setting.depth_segments,
            "bevel_radius"_a = setting.bevel_radius,
            "bevel_segments"_a = setting.bevel_segments,
            "triangulate"_a = setting.triangulate,
            "fixed_uv"_a = setting.fixed_uv,
            "normal_attribute_name"_a = setting.normal_attribute_name,
            "uv_attribute_name"_a = setting.uv_attribute_name,
            "semantic_attribute_name"_a = setting.semantic_label_attribute_name,
            "dist_threshold"_a = setting.dist_threshold,
            "angle_threshold"_a = setting.angle_threshold,
            "epsilon"_a = setting.epsilon,
            "uv_padding"_a = setting.uv_padding,
            "center"_a = setting.center,
            R"(Generate a rounded cube mesh.

:param width: The width of the cube.
:param height: The height of the cube.
:param depth: The depth of the cube.
:param width_segments: The number of segments along the width.
:param height_segments: The number of segments along the height.
:param depth_segments: The number of segments along the depth.
:param bevel_radius: The radius of the bevel on the edges.
:param bevel_segments: The number of segments for the bevel.
:param triangulate: Whether to triangulate the mesh.
:param fixed_uv: Whether to use fixed UVs.
:param normal_attribute_name: The name of the normal attribute.
:param uv_attribute_name: The name of the UV attribute.
:param semantic_attribute_name: The name of the semantic attribute.
:param dist_threshold: The distance threshold for merging vertices.
:param angle_threshold: The angle threshold for merging vertices.
:param epsilon: The epsilon for merging vertices.
:param uv_padding: The padding for the UVs.
:param center: The center of the cube.

:return: The generated mesh.)");
    }

    {
        primitive::RoundedPlaneOptions setting;
        m.def(
            "generate_rounded_plane",
            [](_Scalar width,
               _Scalar height,
               _Scalar bevel_radius,
               Index width_segments,
               Index height_segments,
               Index bevel_segments,
               bool triangulate,
               bool fixed_uv,
               std::string_view normal_attribute_name,
               std::string_view uv_attribute_name,
               std::string_view semantic_attribute_name,
               _Scalar epsilon,
               std::array<_Scalar, 3> normal,
               std::array<_Scalar, 3> center) -> MeshType {
                primitive::RoundedPlaneOptions _setting;
                _setting.width = width;
                _setting.height = height;
                _setting.bevel_radius = bevel_radius;
                _setting.width_segments = width_segments;
                _setting.height_segments = height_segments;
                _setting.bevel_segments = bevel_segments;
                _setting.triangulate = triangulate;
                _setting.fixed_uv = fixed_uv;
                _setting.normal_attribute_name = normal_attribute_name;
                _setting.uv_attribute_name = uv_attribute_name;
                _setting.semantic_label_attribute_name = semantic_attribute_name;
                _setting.epsilon = epsilon;
                _setting.normal = normal;
                _setting.center = center;
                return primitive::generate_rounded_plane<Scalar, Index>(std::move(_setting));
            },
            "width"_a = setting.width,
            "height"_a = setting.height,
            "bevel_radius"_a = setting.bevel_radius,
            "width_segments"_a = setting.width_segments,
            "height_segments"_a = setting.height_segments,
            "bevel_segments"_a = setting.bevel_segments,
            "triangulate"_a = setting.triangulate,
            "fixed_uv"_a = setting.fixed_uv,
            "normal_attribute_name"_a = setting.normal_attribute_name,
            "uv_attribute_name"_a = setting.uv_attribute_name,
            "semantic_attribute_name"_a = setting.semantic_label_attribute_name,
            "epsilon"_a = setting.epsilon,
            "normal"_a = setting.normal,
            "center"_a = setting.center,
            R"(Generate a rounded plane mesh.

:param width: The width of the plane.
:param height: The height of the plane.
:param bevel_radius: The radius of the bevel on the edges.
:param width_segments: The number of segments along the width.
:param height_segments: The number of segments along the height.
:param bevel_segments: The number of segments for the bevel.
:param triangulate: Whether to triangulate the mesh.
:param fixed_uv: Whether to use fixed UVs.
:param normal_attribute_name: The name of the normal attribute.
:param uv_attribute_name: The name of the UV attribute.
:param semantic_attribute_name: The name of the semantic attribute.
:param epsilon: The epsilon for merging vertices.
:param normal: The unit normal vector for the plane.
:param center: The center of the plane.

:return: The generated mesh.)");
    }
}

} // namespace lagrange::python
