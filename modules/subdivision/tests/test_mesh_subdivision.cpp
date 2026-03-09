/*
 * Copyright 2020 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/Attribute.h>
#include <lagrange/AttributeValueType.h>
#include <lagrange/IndexedAttribute.h>
#include <lagrange/cast_attribute.h>
#include <lagrange/compute_area.h>
#include <lagrange/compute_normal.h>
#include <lagrange/create_mesh.h>
#include <lagrange/filter_attributes.h>
#include <lagrange/find_matching_attributes.h>
#include <lagrange/foreach_attribute.h>
#include <lagrange/internal/constants.h>
#include <lagrange/io/load_mesh.h>
#include <lagrange/io/save_mesh.h>
#include <lagrange/subdivision/compute_sharpness.h>
#include <lagrange/subdivision/mesh_subdivision.h>
#include <lagrange/subdivision/midpoint_subdivision.h>
#include <lagrange/subdivision/sqrt_subdivision.h>
#include <lagrange/topology.h>
#include <lagrange/unify_index_buffer.h>
#include <lagrange/utils/fmt_eigen.h>
#include <lagrange/utils/safe_cast.h>
#include <lagrange/views.h>

#include <lagrange/testing/common.h>

#include <catch2/matchers/catch_matchers_floating_point.hpp>

namespace {

template <typename Scalar, typename Index>
void validate_catmull_clark_subdivision(
    const lagrange::SurfaceMesh<Scalar, Index>& mesh,
    const lagrange::SurfaceMesh<Scalar, Index>& subdiv_mesh,
    int num_subdivisions,
    lagrange::subdivision::RefinementType refinement)
{
    bool was_quads = subdiv_mesh.is_quad_mesh();
    auto original_num_facets = mesh.get_num_facets();

    if (refinement == lagrange::subdivision::RefinementType::Uniform) {
        if (num_subdivisions == 0) {
            // Shouldn't change
            REQUIRE(subdiv_mesh.get_num_facets() == mesh.get_num_facets());
            REQUIRE(subdiv_mesh.get_num_vertices() == mesh.get_num_vertices());
        } else {
            REQUIRE(
                subdiv_mesh.get_num_facets() ==
                mesh.get_num_corners() * std::pow(4, num_subdivisions - 1) * (was_quads ? 1 : 2));
        }

        if (num_subdivisions == 1) {
            // Higher subdivisions require connectivity data from previous mesh
            // Checking num vertices only for the lowest level of subdivision.

            auto copy = mesh;
            copy.initialize_edges();
            auto original_num_vertices = mesh.get_num_vertices();
            auto original_num_edges = copy.get_num_edges();

            REQUIRE(
                subdiv_mesh.get_num_vertices() ==
                lagrange::safe_cast<Index>(
                    original_num_vertices + original_num_facets + original_num_edges));
        }
    }

    // Check Euler characteristic
    auto euler_src = lagrange::compute_euler(mesh);
    auto euler_dst = lagrange::compute_euler(subdiv_mesh);
    REQUIRE(euler_dst == euler_src);
}

template <typename Scalar, typename Index>
void validate_loop_subdivision(
    const lagrange::SurfaceMesh<Scalar, Index>& mesh,
    const lagrange::SurfaceMesh<Scalar, Index>& subdiv_mesh,
    int num_subdivisions,
    lagrange::subdivision::RefinementType refinement)
{
    // Output mesh is a triangle mesh.
    REQUIRE(subdiv_mesh.is_triangle_mesh());

    if (refinement == lagrange::subdivision::RefinementType::Uniform) {
        auto original_num_facets = mesh.get_num_facets();
        // auto original_num_vertices = mesh.get_num_vertices();

        REQUIRE(
            subdiv_mesh.get_num_facets() == std::pow(4, num_subdivisions) * original_num_facets);
        // REQUIRE(subdiv_mesh.get_num_vertices()...
    }

    // Check Euler characteristic
    auto euler_src = lagrange::compute_euler(mesh);
    auto euler_dst = lagrange::compute_euler(subdiv_mesh);
    REQUIRE(euler_dst == euler_src);
}

template <typename Scalar, typename Index>
void validate_subdivision(
    const lagrange::SurfaceMesh<Scalar, Index>& mesh,
    const lagrange::SurfaceMesh<Scalar, Index>& subdiv_mesh,
    int num_subdivisions,
    lagrange::subdivision::SchemeType scheme,
    lagrange::subdivision::RefinementType refinement)
{
    if (scheme == lagrange::subdivision::SchemeType::Loop) {
        validate_loop_subdivision(mesh, subdiv_mesh, num_subdivisions, refinement);
    } else {
        validate_catmull_clark_subdivision(mesh, subdiv_mesh, num_subdivisions, refinement);
    }
}

} // namespace

TEST_CASE("mesh_subdivision", "[mesh][subdivision]" LA_SLOW_DEBUG_FLAG)
{
    std::vector<std::string> filenames = {
        "open/core/simple/cube.obj",
        "open/core/simple/octahedron.obj",
        "open/core/simple/quad_meshes/cube.obj",
        "open/core/poly/mixedFaring.obj",
        "open/subdivision/cube.obj",
        "open/subdivision/catmark_fvar_bound1.obj",
        "open/subdivision/grid_3x3.obj",
        "open/core/topology/nonmanifold_edge.obj",
        "open/core/topology/nonmanifold_vertex.obj",
        "open/core/topology/nonoriented_edge.obj",
    };

    std::vector<unsigned> levels = {
        0,
        1,
        3,
    };

    auto schemes = {
        lagrange::subdivision::SchemeType::Loop,
        lagrange::subdivision::SchemeType::CatmullClark,
    };

    auto refinements = {
        lagrange::subdivision::RefinementType::Uniform,
        lagrange::subdivision::RefinementType::EdgeAdaptive,
    };

    for (auto scheme : schemes) {
        for (auto filename : filenames) {
            for (auto level : levels) {
                for (auto refinement : refinements) {
                    lagrange::subdivision::SubdivisionOptions options;
                    options.scheme = scheme;
                    options.refinement = refinement;
                    options.num_levels = level;
                    options.validate_topology = true;

                    auto mesh = lagrange::testing::load_surface_mesh<double, uint32_t>(filename);
                    if (scheme == lagrange::subdivision::SchemeType::Loop &&
                        !mesh.is_triangle_mesh()) {
                        LA_REQUIRE_THROWS(lagrange::subdivision::subdivide_mesh(mesh, options));
                    } else {
                        auto subdivided_mesh = lagrange::subdivision::subdivide_mesh(mesh, options);
                        validate_subdivision(
                            mesh,
                            subdivided_mesh,
                            options.num_levels,
                            *options.scheme,
                            options.refinement);
                    }
                }
            }
        }
    }
}

TEST_CASE("mesh_subdivision_with_uv", "[mesh][subdivision]" LA_SLOW_DEBUG_FLAG)
{
    auto refinements = {
        lagrange::subdivision::RefinementType::Uniform,
        lagrange::subdivision::RefinementType::EdgeAdaptive,
    };
    for (auto refinement : refinements) {
        using Scalar = double;
        lagrange::SurfaceMesh32d mesh(2);
        mesh.add_vertices(4, {0, 0, 1, 0, 1, 1, 0, 1});
        mesh.add_quad(0, 1, 2, 3);

        auto uv_id = mesh.create_attribute<Scalar>(
            "uv",
            lagrange::AttributeElement::Indexed,
            lagrange::AttributeUsage::UV,
            2,
            mesh.get_vertex_to_position().get_all(),
            mesh.get_corner_to_vertex().get_all());

        lagrange::subdivision::SubdivisionOptions options;
        options.scheme = lagrange::subdivision::SchemeType::CatmullClark;
        options.num_levels = 2;
        options.refinement = refinement;
        if (refinement == lagrange::subdivision::RefinementType::EdgeAdaptive) {
            options.use_limit_surface = true;
        }
        options.interpolated_attributes.set_selected({uv_id});

        auto subdivided_mesh = lagrange::subdivision::subdivide_mesh(mesh, options);
        validate_subdivision(
            mesh,
            subdivided_mesh,
            options.num_levels,
            *options.scheme,
            options.refinement);

        auto uv_mesh = [&] {
            lagrange::SurfaceMesh32d uv_mesh_(2);
            const auto& uv_attr = subdivided_mesh.get_indexed_attribute<Scalar>("uv");
            uv_mesh_.wrap_as_const_vertices(
                uv_attr.values().get_all(),
                uv_attr.values().get_num_elements());
            uv_mesh_.wrap_as_const_facets(
                uv_attr.indices().get_all(),
                subdivided_mesh.get_num_facets(),
                subdivided_mesh.get_vertex_per_facet());
            return uv_mesh_;
        }();

        auto area = lagrange::compute_mesh_area(subdivided_mesh);
        auto uv_area = lagrange::compute_mesh_area(uv_mesh);

        const Scalar eps = Scalar(1e-6);
        REQUIRE_THAT(uv_area, Catch::Matchers::WithinRel(area, eps));
    }
}

namespace {

template <typename Derived>
bool all_unit_axis(const Eigen::MatrixBase<Derived>& M, bool approx)
{
    using Scalar = typename Derived::Scalar;
    using Vec3s = Eigen::RowVector3<Scalar>;
    std::array<Vec3s, 6> unit_axes = {
        Vec3s::UnitX(),
        -Vec3s::UnitX(),
        Vec3s::UnitY(),
        -Vec3s::UnitY(),
        Vec3s::UnitZ(),
        -Vec3s::UnitZ(),
    };
    for (const auto& p : M.rowwise()) {
        bool ok = false;
        for (const auto& axis : unit_axes) {
            if (approx ? axis.isApprox(p) : axis == p) {
                ok = true;
            }
        }
        if (!ok) {
            return false;
        }
    }
    return true;
}

} // namespace

TEST_CASE("mesh_subdivision_limit_uniform", "[mesh][subdivision]" LA_SLOW_DEBUG_FLAG)
{
    using Scalar = double;
    using Index = uint32_t;
    auto mesh = lagrange::testing::load_surface_mesh<double, uint32_t>("open/subdivision/cube.obj");
    auto nrm_id = lagrange::compute_normal(mesh, lagrange::internal::pi * 0.5);
    auto nrm_name = mesh.get_attribute_name(nrm_id);

    auto N_coarse = lagrange::matrix_view(mesh.get_indexed_attribute<Scalar>(nrm_id).values());
    REQUIRE(all_unit_axis(N_coarse, false));

    lagrange::subdivision::SubdivisionOptions options;
    options.scheme = lagrange::subdivision::SchemeType::CatmullClark;
    options.num_levels = 2;

    options.interpolated_attributes.set_selected({nrm_id});
    auto refined_mesh = lagrange::subdivision::subdivide_mesh(mesh, options);

    options.use_limit_surface = true;
    options.interpolated_attributes.set_none();
    options.output_limit_normals = "normal";
    options.output_limit_tangents = "tangent";
    options.output_limit_bitangents = "bitangent";
    auto limit_mesh = lagrange::subdivision::subdivide_mesh(mesh, options);
    lagrange::io::save_mesh("limit_uniform.obj", limit_mesh);

    // Check limit positions
    auto V_refined = vertex_view(refined_mesh);
    auto V_limit = vertex_view(limit_mesh);
    for (Index v = 0; v < refined_mesh.get_num_vertices(); ++v) {
        Scalar l_refined = V_refined.row(v).norm();
        Scalar l_limit = V_limit.row(v).norm();
        REQUIRE(l_limit < l_refined); // limit surface has more shrinkage
    }

    // Interpolated normals should be among [±X, ±Y, ±Z]
    auto N_refined =
        lagrange::matrix_view(refined_mesh.get_indexed_attribute<Scalar>(nrm_name).values());
    REQUIRE(all_unit_axis(N_refined, false));

    // Limit normals should be smooth and point roughly in the same direction as the vertex pos
    auto N_limit = lagrange::attribute_matrix_view<Scalar>(limit_mesh, "normal");
    for (Index v = 0; v < refined_mesh.get_num_vertices(); ++v) {
        auto pos = V_limit.row(v).stableNormalized();
        auto nrm = N_limit.row(v);
        const Scalar eps = Scalar(1.5e-1);
        for (int k = 0; k < 3; ++k) {
            REQUIRE_THAT(
                pos[k],
                Catch::Matchers::WithinRel(nrm[k], eps) ||
                    (Catch::Matchers::WithinAbs(nrm[k], eps) &&
                     Catch::Matchers::WithinAbs(0, eps)));
        }
    }

    // Limit normals can be obtained from the limit tangent/bitangent
    auto T_limit = lagrange::attribute_matrix_view<Scalar>(limit_mesh, "tangent");
    auto B_limit = lagrange::attribute_matrix_view<Scalar>(limit_mesh, "bitangent");
    for (Index v = 0; v < refined_mesh.get_num_vertices(); ++v) {
        Eigen::RowVector3<Scalar> du = T_limit.row(v);
        Eigen::RowVector3<Scalar> dv = B_limit.row(v);
        Eigen::RowVector3<Scalar> normal = du.cross(dv).stableNormalized();
        auto nrm = N_limit.row(v);
        const Scalar eps = Scalar(1e-8);
        for (int k = 0; k < 3; ++k) {
            REQUIRE_THAT(
                normal[k],
                Catch::Matchers::WithinRel(nrm[k], eps) ||
                    (Catch::Matchers::WithinAbs(nrm[k], eps) &&
                     Catch::Matchers::WithinAbs(0, eps)));
        }
    }
}

TEST_CASE("mesh_subdivision_limit_adaptive", "[mesh][subdivision]" LA_SLOW_DEBUG_FLAG)
{
    using Scalar = double;
    using Index = uint32_t;
    auto mesh = lagrange::testing::load_surface_mesh<double, uint32_t>("open/subdivision/cube.obj");
    auto nrm_id = lagrange::compute_normal(mesh, lagrange::internal::pi * 0.5);
    auto nrm_name = mesh.get_attribute_name(nrm_id);

    auto N_coarse = lagrange::matrix_view(mesh.get_indexed_attribute<Scalar>(nrm_id).values());
    REQUIRE(all_unit_axis(N_coarse, false));

    lagrange::subdivision::SubdivisionOptions options;
    options.scheme = lagrange::subdivision::SchemeType::CatmullClark;
    options.num_levels = 10;

    options.use_limit_surface = true;
    options.interpolated_attributes.set_selected({nrm_id});
    options.refinement = lagrange::subdivision::RefinementType::EdgeAdaptive;
    options.output_limit_normals = "normal";
    options.output_limit_tangents = "tangent";
    options.output_limit_bitangents = "bitangent";
    auto limit_mesh = lagrange::subdivision::subdivide_mesh(mesh, options);
    limit_mesh = lagrange::unify_named_index_buffer(limit_mesh, {"normal", "tangent", "bitangent"});

    // Interpolated normals should be among [±X, ±Y, ±Z]
    auto N_refined =
        lagrange::matrix_view(limit_mesh.get_indexed_attribute<Scalar>(nrm_name).values());
    REQUIRE(all_unit_axis(N_refined, true));

    // Limit normals should be smooth and point roughly in the same direction as the vertex pos
    auto V_limit = vertex_view(limit_mesh);
    auto N_limit = lagrange::attribute_matrix_view<Scalar>(limit_mesh, "normal");
    for (Index v = 0; v < limit_mesh.get_num_vertices(); ++v) {
        auto pos = V_limit.row(v).stableNormalized();
        auto nrm = N_limit.row(v);
        const Scalar eps = Scalar(1.5e-1);
        for (int k = 0; k < 3; ++k) {
            REQUIRE_THAT(
                pos[k],
                Catch::Matchers::WithinRel(nrm[k], eps) ||
                    (Catch::Matchers::WithinAbs(nrm[k], eps) &&
                     Catch::Matchers::WithinAbs(0, eps)));
        }
    }

    // Limit normals can be obtained from the limit tangent/bitangent
    auto T_limit = lagrange::attribute_matrix_view<Scalar>(limit_mesh, "tangent");
    auto B_limit = lagrange::attribute_matrix_view<Scalar>(limit_mesh, "bitangent");
    for (Index v = 0; v < limit_mesh.get_num_vertices(); ++v) {
        Eigen::RowVector3<Scalar> du = T_limit.row(v);
        Eigen::RowVector3<Scalar> dv = B_limit.row(v);
        Eigen::RowVector3<Scalar> normal = du.cross(dv).stableNormalized();
        auto nrm = N_limit.row(v);
        const Scalar eps = Scalar(1e-8);
        for (int k = 0; k < 3; ++k) {
            REQUIRE_THAT(
                normal[k],
                Catch::Matchers::WithinRel(nrm[k], eps) ||
                    (Catch::Matchers::WithinAbs(nrm[k], eps) &&
                     Catch::Matchers::WithinAbs(0, eps)));
        }
    }
}

TEST_CASE("mesh_subdivision_adaptive_mixed", "[mesh][subdivision]")
{
    auto mesh = lagrange::testing::load_surface_mesh<double, uint32_t>("open/subdivision/cube.obj");

    for (auto id : lagrange::find_matching_attributes(mesh, lagrange::AttributeElement::Indexed)) {
        lagrange::cast_attribute_in_place<float>(mesh, id);
    }

    lagrange::subdivision::SubdivisionOptions options;
    options.scheme = lagrange::subdivision::SchemeType::CatmullClark;
    options.num_levels = 4;

    options.use_limit_surface = true;
    options.refinement = lagrange::subdivision::RefinementType::EdgeAdaptive;
    auto refined_mesh = lagrange::subdivision::subdivide_mesh(mesh, options);
}

namespace {

template <typename Scalar, typename Index>
void compare_with_expected(
    const lagrange::SurfaceMesh<Scalar, Index>& result_mesh,
    std::string_view expected_filename)
{
    auto expected_mesh = lagrange::testing::load_surface_mesh<Scalar, Index>(expected_filename);
    auto V_result = vertex_view(result_mesh);
    auto V_expected = vertex_view(expected_mesh);
    auto F_result = facet_view(result_mesh);
    auto F_expected = facet_view(expected_mesh);
    REQUIRE(F_result == F_expected);
    const Scalar eps = Scalar(1e-8);
    for (Index i = 0; i < V_result.size(); ++i) {
        REQUIRE_THAT(
            V_result.data()[i],
            Catch::Matchers::WithinRel(V_expected.data()[i], eps) ||
                (Catch::Matchers::WithinAbs(V_expected.data()[i], eps) &&
                 Catch::Matchers::WithinAbs(0, eps)));
    }
}

} // namespace

TEST_CASE("mesh_subdivision_sharpness", "[mesh][subdivision]" LA_SLOW_DEBUG_FLAG)
{
    using Scalar = double;
    using Index = uint32_t;
    auto mesh = lagrange::testing::load_surface_mesh<double, uint32_t>("open/subdivision/cube.obj");

    lagrange::subdivision::SubdivisionOptions options;
    options.scheme = lagrange::subdivision::SchemeType::CatmullClark;
    options.num_levels = 3;

    SECTION("vertex sharpness")
    {
        // Set vertex 0 as sharp
        auto vsharp_id =
            mesh.create_attribute<Scalar>("vsharp", lagrange::AttributeElement::Vertex, 1);
        mesh.ref_attribute<Scalar>(vsharp_id).ref_row(0)[0] = 1.0;
        options.vertex_sharpness_attr = {vsharp_id};

        auto refined_mesh = lagrange::subdivision::subdivide_mesh(mesh, options);

        // Uncomment to save reference result:
        // lagrange::io::save_mesh("cube_vsharp.obj", refined_mesh);
        compare_with_expected(refined_mesh, "open/subdivision/cube_vsharp.obj");
    }

    SECTION("edge sharpness")
    {
        // Set edge [0, 1], [1, 2], [2, 3], [3, 0] as sharp
        mesh.initialize_edges();
        auto esharp_id =
            mesh.create_attribute<Scalar>("esharp", lagrange::AttributeElement::Edge, 1);
        for (Index k = 0; k < 4; ++k) {
            Index eid = mesh.find_edge_from_vertices(k, (k + 1) % 4);
            mesh.ref_attribute<Scalar>(esharp_id).ref_row(eid)[0] = 1.0;
        }
        options.edge_sharpness_attr = {esharp_id};

        auto refined_mesh = lagrange::subdivision::subdivide_mesh(mesh, options);

        // Uncomment to save reference result:
        // lagrange::io::save_mesh("cube_esharp.obj", refined_mesh);
        compare_with_expected(refined_mesh, "open/subdivision/cube_esharp.obj");
    }

    SECTION("face holes")
    {
        // Set face 0 as a hole
        auto fholes_id =
            mesh.create_attribute<Index>("fholes", lagrange::AttributeElement::Facet, 1);
        REQUIRE(
            mesh.get_attribute_base(fholes_id).get_value_type() ==
            lagrange::make_attribute_value_type<Index>());
        mesh.ref_attribute<Index>(fholes_id).ref_row(0)[0] = 1;
        options.face_hole_attr = {fholes_id};

        auto refined_mesh = lagrange::subdivision::subdivide_mesh(mesh, options);

        // Uncomment to save reference result:
        // lagrange::io::save_mesh("cube_fholes.obj", refined_mesh);
        compare_with_expected(refined_mesh, "open/subdivision/cube_fholes.obj");
    }
}

TEST_CASE("mesh_subdivision_sqrt", "[mesh][subdivision][sqrt]")
{
    auto mesh =
        lagrange::testing::load_surface_mesh<double, uint32_t>("open/subdivision/sphere.ply");
    auto expected_mesh =
        lagrange::testing::load_surface_mesh<double, uint32_t>("open/subdivision/sphere_sqrt.ply");
    auto subdivided_mesh = lagrange::subdivision::sqrt_subdivision(mesh);
    REQUIRE(vertex_view(subdivided_mesh) == vertex_view(expected_mesh));
    REQUIRE(facet_view(subdivided_mesh) == facet_view(expected_mesh));
}

TEST_CASE("mesh_subdivision_midpoint", "[mesh][subdivision][sqrt]")
{
    auto mesh =
        lagrange::testing::load_surface_mesh<double, uint32_t>("open/subdivision/sphere.ply");
    auto expected_mesh = lagrange::testing::load_surface_mesh<double, uint32_t>(
        "open/subdivision/sphere_midpoint.ply");
    auto subdivided_mesh = lagrange::subdivision::midpoint_subdivision(mesh);
    REQUIRE(vertex_view(subdivided_mesh) == vertex_view(expected_mesh));
    REQUIRE(facet_view(subdivided_mesh) == facet_view(expected_mesh));
}

TEST_CASE("mesh_subdivision_empty", "[mesh][subdivision]")
{
    SECTION("no vertices")
    {
        lagrange::SurfaceMesh32f mesh;
        auto result = lagrange::subdivision::subdivide_mesh(mesh);
        REQUIRE(result.get_num_vertices() == 0);
        REQUIRE(result.get_num_facets() == 0);
    }

    SECTION("vertices but no facets")
    {
        lagrange::SurfaceMesh32f mesh;
        mesh.add_vertices(5);
        auto result = lagrange::subdivision::subdivide_mesh(mesh);
        REQUIRE(result.get_num_vertices() == 5);
        REQUIRE(result.get_num_facets() == 0);
    }
}

TEST_CASE("compute_sharpness", "[mesh][subdivision][sharpness]")
{
    using Scalar = double;
    using Index = uint32_t;

    // Constants for common feature angles and sharpness thresholds
    constexpr Scalar feature_angle_90_deg = 0.5; // 90 degrees in units of pi
    constexpr Scalar feature_angle_45_deg = 0.25; // 45 degrees in units of pi
    constexpr Scalar feature_angle_18_deg = 0.1; // 18 degrees in units of pi
    constexpr float sharpness_threshold =
        0.5f; // Threshold for determining if an edge/vertex is sharp

    SECTION("with indexed normals")
    {
        // Create a simple cube mesh with indexed normals
        auto mesh =
            lagrange::testing::load_surface_mesh<Scalar, Index>("open/subdivision/cube.obj");

        // Add indexed normals with a feature angle
        auto nrm_id = lagrange::compute_normal(mesh, lagrange::internal::pi * feature_angle_90_deg);
        auto nrm_name = mesh.get_attribute_name(nrm_id);

        // Call compute_sharpness with the indexed normal attribute
        lagrange::subdivision::SharpnessOptions options;
        options.normal_attribute_name = nrm_name;
        auto results = lagrange::subdivision::compute_sharpness(mesh, options);

        // Verify that the function returned attribute ids
        REQUIRE(results.normal_attr.has_value());
        REQUIRE(results.edge_sharpness_attr.has_value());
        REQUIRE(results.vertex_sharpness_attr.has_value());

        // Verify the normal attribute is the one we provided
        REQUIRE(results.normal_attr.value() == nrm_id);

        // Verify edge sharpness attribute exists and has correct properties
        const auto& edge_sharpness_attr =
            mesh.get_attribute_base(results.edge_sharpness_attr.value());
        REQUIRE(edge_sharpness_attr.get_element_type() == lagrange::AttributeElement::Edge);
        REQUIRE(edge_sharpness_attr.get_num_channels() == 1);

        // Verify vertex sharpness attribute exists and has correct properties
        const auto& vertex_sharpness_attr =
            mesh.get_attribute_base(results.vertex_sharpness_attr.value());
        REQUIRE(vertex_sharpness_attr.get_element_type() == lagrange::AttributeElement::Vertex);
        REQUIRE(vertex_sharpness_attr.get_num_channels() == 1);

        // Verify that sharp edges are marked (cube has 12 edges, all should be sharp due to 90°
        // angles)
        mesh.initialize_edges();
        auto edge_sharpness =
            lagrange::attribute_vector_view<float>(mesh, results.edge_sharpness_attr.value());
        Index num_sharp_edges = 0;
        for (Index e = 0; e < mesh.get_num_edges(); ++e) {
            if (edge_sharpness[e] > sharpness_threshold) {
                num_sharp_edges++;
            }
        }
        REQUIRE(num_sharp_edges == 12); // All edges of a cube should be sharp

        // Verify that corner vertices are sharp (cube has 8 vertices, all should be sharp)
        auto vertex_sharpness =
            lagrange::attribute_vector_view<float>(mesh, results.vertex_sharpness_attr.value());
        Index num_sharp_vertices = 0;
        for (Index v = 0; v < mesh.get_num_vertices(); ++v) {
            if (vertex_sharpness[v] > sharpness_threshold) {
                num_sharp_vertices++;
            }
        }
        REQUIRE(num_sharp_vertices == 8); // All vertices of a cube should be sharp
    }

    SECTION("with feature angle threshold")
    {
        // Create a simple cube mesh without indexed normals
        auto mesh =
            lagrange::testing::load_surface_mesh<Scalar, Index>("open/subdivision/cube.obj");

        // Remove any existing indexed normal attributes
        std::vector<lagrange::AttributeId> to_remove;
        lagrange::AttributeMatcher matcher;
        matcher.usages = lagrange::AttributeUsage::Normal;
        matcher.element_types = lagrange::AttributeElement::Indexed;
        for (auto id : lagrange::find_matching_attributes(mesh, matcher)) {
            to_remove.push_back(id);
        }
        for (auto id : to_remove) {
            mesh.delete_attribute(mesh.get_attribute_name(id));
        }

        // Call compute_sharpness with a feature angle threshold
        lagrange::subdivision::SharpnessOptions options;
        options.feature_angle_threshold =
            lagrange::internal::pi * feature_angle_90_deg; // 90 degrees
        auto results = lagrange::subdivision::compute_sharpness(mesh, options);

        // Verify that the function computed normals and sharpness attributes
        REQUIRE(results.normal_attr.has_value());
        REQUIRE(results.edge_sharpness_attr.has_value());
        REQUIRE(results.vertex_sharpness_attr.has_value());

        // Verify that an indexed normal attribute was created
        const auto& normal_attr = mesh.get_attribute_base(results.normal_attr.value());
        REQUIRE(normal_attr.get_element_type() == lagrange::AttributeElement::Indexed);
    }

    SECTION("without normals or feature angle")
    {
        // Create a simple cube mesh
        auto mesh =
            lagrange::testing::load_surface_mesh<Scalar, Index>("open/subdivision/cube.obj");

        // Remove any existing indexed normal attributes
        std::vector<lagrange::AttributeId> to_remove;
        lagrange::AttributeMatcher matcher;
        matcher.usages = lagrange::AttributeUsage::Normal;
        matcher.element_types = lagrange::AttributeElement::Indexed;
        for (auto id : lagrange::find_matching_attributes(mesh, matcher)) {
            to_remove.push_back(id);
        }
        for (auto id : to_remove) {
            mesh.delete_attribute(mesh.get_attribute_name(id));
        }

        // Call compute_sharpness without providing normals or feature angle
        lagrange::subdivision::SharpnessOptions options;
        auto results = lagrange::subdivision::compute_sharpness(mesh, options);

        // Verify that no sharpness information is computed
        REQUIRE(!results.normal_attr.has_value());
        REQUIRE(!results.edge_sharpness_attr.has_value());
        REQUIRE(!results.vertex_sharpness_attr.has_value());
    }

    SECTION("vertex sharpness computation - junction vertices")
    {
        // Create a simple quad mesh
        lagrange::SurfaceMesh<Scalar, Index> mesh(3);
        // clang-format off
        mesh.add_vertices(5, {
            0.5, 0.5, 1, // vertex 0 - center junction (valence 4)
            1, 0, 0, // vertex 1
            1, 1, 0, // vertex 2
            0, 1, 0, // vertex 3
            0, 0, 0 // vertex 4
        });
        // clang-format on
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(0, 2, 3);
        mesh.add_triangle(0, 3, 4);
        mesh.add_triangle(0, 4, 1);

        // Add indexed normals
        lagrange::compute_normal(mesh, lagrange::internal::pi * feature_angle_45_deg);

        // Call compute_sharpness
        lagrange::subdivision::SharpnessOptions options;
        auto results = lagrange::subdivision::compute_sharpness(mesh, options);

        // Verify results
        REQUIRE(results.normal_attr.has_value());
        REQUIRE(results.vertex_sharpness_attr.has_value());

        // Check vertex sharpness - center vertex should have high valence and be sharp
        auto vertex_sharpness =
            lagrange::attribute_vector_view<float>(mesh, results.vertex_sharpness_attr.value());

        // Vertex 0 (center, valence > 2) should be sharp
        REQUIRE(vertex_sharpness[0] > sharpness_threshold);
    }

    SECTION("vertex sharpness computation - leaf vertices")
    {
        // Create a simple mesh with boundary edges
        lagrange::SurfaceMesh<Scalar, Index> mesh(3);
        // clang-format off
        mesh.add_vertices(4, {
            0, 0, 0,
            1, 0, 1,
            0.5, 1, 0,
            1.5, 0, 0
        });
        // clang-format on
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(1, 3, 2);

        // Add indexed normals that create seams
        lagrange::compute_normal(mesh, lagrange::internal::pi * feature_angle_18_deg);

        // Call compute_sharpness
        lagrange::subdivision::SharpnessOptions options;
        auto results = lagrange::subdivision::compute_sharpness(mesh, options);

        // Verify results
        REQUIRE(results.vertex_sharpness_attr.has_value());

        // At least some vertices should have sharpness values computed
        auto vertex_sharpness =
            lagrange::attribute_vector_view<float>(mesh, results.vertex_sharpness_attr.value());
        bool has_sharp = false;
        for (Index v = 0; v < mesh.get_num_vertices(); ++v) {
            if (vertex_sharpness[v] > sharpness_threshold) {
                has_sharp = true;
                break;
            }
        }
        REQUIRE(has_sharp);
        // The specific configuration depends on the normal seams created
        // Just verify the attribute is populated
        REQUIRE(vertex_sharpness.size() == mesh.get_num_vertices());
    }
}
