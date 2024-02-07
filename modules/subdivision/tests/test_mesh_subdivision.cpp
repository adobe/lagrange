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
#include <lagrange/compute_area.h>
#include <lagrange/compute_normal.h>
#include <lagrange/create_mesh.h>
#include <lagrange/io/load_mesh.h>
#include <lagrange/io/save_mesh.h>
#include <lagrange/subdivision/mesh_subdivision.h>
#include <lagrange/subdivision/midpoint_subdivision.h>
#include <lagrange/subdivision/sqrt_subdivision.h>
#include <lagrange/topology.h>
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
    int num_subdivisions)
{
    bool was_quads = subdiv_mesh.is_quad_mesh();
    auto original_num_facets = mesh.get_num_facets();

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

    // Check Euler characteristic
    auto euler_src = lagrange::compute_euler(mesh);
    auto euler_dst = lagrange::compute_euler(subdiv_mesh);
    REQUIRE(euler_dst == euler_src);
}

template <typename Scalar, typename Index>
void validate_loop_subdivision(
    const lagrange::SurfaceMesh<Scalar, Index>& mesh,
    const lagrange::SurfaceMesh<Scalar, Index>& subdiv_mesh,
    int num_subdivisions)
{
    // Output mesh is a triangle mesh.
    REQUIRE(subdiv_mesh.is_triangle_mesh());

    auto original_num_facets = mesh.get_num_facets();
    // auto original_num_vertices = mesh.get_num_vertices();

    REQUIRE(subdiv_mesh.get_num_facets() == std::pow(4, num_subdivisions) * original_num_facets);
    // REQUIRE(subdiv_mesh.get_num_vertices()...

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
    lagrange::subdivision::SchemeType scheme)
{
    if (scheme == lagrange::subdivision::SchemeType::Loop) {
        validate_loop_subdivision(mesh, subdiv_mesh, num_subdivisions);
    } else {
        validate_catmull_clark_subdivision(mesh, subdiv_mesh, num_subdivisions);
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

    for (auto scheme : schemes) {
        for (auto filename : filenames) {
            for (auto level : levels) {
                lagrange::subdivision::SubdivisionOptions options;
                options.scheme = scheme;
                options.num_levels = level;

                auto mesh = lagrange::testing::load_surface_mesh<double, uint32_t>(filename);
                if (scheme == lagrange::subdivision::SchemeType::Loop && !mesh.is_triangle_mesh()) {
                    LA_REQUIRE_THROWS(lagrange::subdivision::subdivide_mesh(mesh, options));
                } else {
                    auto subdivided_mesh = lagrange::subdivision::subdivide_mesh(mesh, options);
                    validate_subdivision(
                        mesh,
                        subdivided_mesh,
                        options.num_levels,
                        *options.scheme);
                }
            }
        }
    }
}

TEST_CASE("mesh_subdivision_with_uv", "[mesh][subdivision]" LA_SLOW_DEBUG_FLAG)
{
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
    options.interpolated_attributes.set_selected({uv_id});

    auto subdivided_mesh = lagrange::subdivision::subdivide_mesh(mesh, options);
    validate_subdivision(mesh, subdivided_mesh, options.num_levels, *options.scheme);

    auto uv_mesh = [&] {
        lagrange::SurfaceMesh32d uv_mesh(2);
        const auto& uv_attr = subdivided_mesh.get_indexed_attribute<Scalar>("uv");
        uv_mesh.wrap_as_const_vertices(
            uv_attr.values().get_all(),
            uv_attr.values().get_num_elements());
        uv_mesh.wrap_as_const_facets(
            uv_attr.indices().get_all(),
            subdivided_mesh.get_num_facets(),
            subdivided_mesh.get_vertex_per_facet());
        return uv_mesh;
    }();

    auto area = lagrange::compute_mesh_area(subdivided_mesh);
    auto uv_area = lagrange::compute_mesh_area(uv_mesh);

    const Scalar eps = Scalar(1e-6);
    REQUIRE_THAT(uv_area, Catch::Matchers::WithinRel(area, eps));
}

namespace {

template <typename Derived>
bool all_unit_axis(const Eigen::MatrixBase<Derived>& M)
{
    using Scalar = typename Derived::Scalar;
    using Vec3s = Eigen::RowVector3<Scalar>;
    for (const auto& p : M.rowwise()) {
        if (!(p == Vec3s::UnitX() || p == -Vec3s::UnitX() || p == Vec3s::UnitY() ||
              p == -Vec3s::UnitY() || p == Vec3s::UnitZ() || p == -Vec3s::UnitZ())) {
            return false;
        }
    }
    return true;
}

} // namespace

TEST_CASE("mesh_subdivision_limit", "[mesh][subdivision]" LA_SLOW_DEBUG_FLAG)
{
    using Scalar = double;
    using Index = uint32_t;
    auto mesh = lagrange::testing::load_surface_mesh<double, uint32_t>("open/subdivision/cube.obj");
    auto nrm_id = lagrange::compute_normal(mesh, M_PI * 0.5);
    auto nrm_name = mesh.get_attribute_name(nrm_id);

    auto N_coarse = lagrange::matrix_view(mesh.get_indexed_attribute<Scalar>(nrm_id).values());
    REQUIRE(all_unit_axis(N_coarse));

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
    REQUIRE(all_unit_axis(N_refined));

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
