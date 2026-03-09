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
#include <lagrange/Attribute.h>
#include <lagrange/SurfaceMesh.h>
#include <lagrange/compute_vertex_normal.h>
#include <lagrange/io/load_mesh.h>
#include <lagrange/raycasting/RayCaster.h>
#include <lagrange/raycasting/project.h>
#include <lagrange/raycasting/project_closest_point.h>
#include <lagrange/raycasting/project_closest_vertex.h>
#include <lagrange/raycasting/project_directional.h>
#include <lagrange/views.h>

#include <lagrange/testing/common.h>
#include <catch2/catch_approx.hpp>

#include <atomic>
#include <random>

namespace {

using Scalar = double;
using Index = uint32_t;
using MeshType = lagrange::SurfaceMesh<Scalar, Index>;

/// Load the hemisphere mesh as a SurfaceMesh and add a 2-channel vertex attribute "pos" containing
/// the xy coordinates of each vertex.
MeshType load_hemisphere_with_pos()
{
    auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/hemisphere.obj");

    // Create a 2-channel vertex attribute "pos" = first 2 columns of vertex positions.
    auto V = lagrange::vertex_view(mesh);
    std::vector<Scalar> values(static_cast<size_t>(mesh.get_num_vertices()) * 2);
    for (Index v = 0; v < mesh.get_num_vertices(); ++v) {
        values[2 * v + 0] = V(v, 0);
        values[2 * v + 1] = V(v, 1);
    }
    mesh.template create_attribute<Scalar>(
        "pos",
        lagrange::AttributeElement::Vertex,
        2,
        lagrange::AttributeUsage::Vector,
        {values.data(), values.size()});

    return mesh;
}

/// Perturb vertex positions of a SurfaceMesh. If z_only is true, only z coordinates are perturbed.
MeshType perturb_mesh(const MeshType& mesh, double s, bool z_only = false)
{
    MeshType result = mesh; // shallow copy, will deep-copy on write

    auto V = lagrange::vertex_ref(result);

    // Compute minimum edge length for scaling.
    Scalar min_len = std::numeric_limits<Scalar>::max();
    for (Index f = 0; f < result.get_num_facets(); ++f) {
        auto fv = result.get_facet_vertices(f);
        for (Index lv = 0; lv < fv.size(); ++lv) {
            auto p1 = V.row(fv[lv]);
            auto p2 = V.row(fv[(lv + 1) % fv.size()]);
            min_len = std::min(min_len, Scalar((p1 - p2).norm()));
        }
    }

    std::mt19937 gen;
    std::uniform_real_distribution<double> dist(0, s * min_len);
    for (Index i = 0; i < result.get_num_vertices(); ++i) {
        if (z_only) {
            V(i, 2) += static_cast<Scalar>(dist(gen));
        } else {
            for (int c = 0; c < 3; ++c) {
                V(i, c) += static_cast<Scalar>(dist(gen));
            }
        }
    }

    // Remove the "pos" attribute if it exists (we only want vertex positions perturbed).
    if (result.has_attribute("pos")) {
        result.delete_attribute("pos");
    }

    return result;
}

/// Create a copy of the mesh geometry (same vertices and facets) without any custom attributes.
MeshType copy_geometry(const MeshType& mesh)
{
    MeshType result;
    auto V = lagrange::vertex_view(mesh);
    result.add_vertices(mesh.get_num_vertices());
    auto V_out = lagrange::vertex_ref(result);
    V_out = V;

    for (Index f = 0; f < mesh.get_num_facets(); ++f) {
        auto fv = mesh.get_facet_vertices(f);
        std::vector<Index> verts(fv.begin(), fv.end());
        result.add_polygon(verts);
    }

    return result;
}

} // namespace

TEST_CASE("SurfaceMesh: project_directional", "[raycasting]")
{
    auto source = load_hemisphere_with_pos();
    auto pos_id = source.get_attribute_id("pos");

    SECTION("perturbed")
    {
        auto target = perturb_mesh(source, 0.1);

        std::vector<char> ishit(target.get_num_vertices(), true);

        lagrange::raycasting::ProjectDirectionalOptions options;
        options.attribute_ids = {pos_id};
        options.project_vertices = false;
        options.direction = Eigen::Vector3f(0, 0, 1);
        options.cast_mode = lagrange::raycasting::CastMode::BothWays;
        options.fallback_mode = lagrange::raycasting::FallbackMode::Constant;
        options.default_value = 0.0;
        options.user_callback = [&](uint64_t v, bool hit) { ishit[v] = hit; };

        lagrange::raycasting::project_directional(source, target, options);

        REQUIRE(target.has_attribute("pos"));
        auto V = lagrange::vertex_view(target);
        auto P = lagrange::attribute_matrix_view<Scalar>(target, "pos");
        REQUIRE(P.cols() == 2);
        for (Index v = 0; v < target.get_num_vertices(); ++v) {
            if (ishit[v]) {
                REQUIRE((P.row(v) - V.row(v).head<2>()).norm() < 1e-7);
            }
        }
    }

    SECTION("perturbed in z")
    {
        auto target = perturb_mesh(source, 0.1, true);

        std::atomic_bool all_hit(true);

        lagrange::raycasting::ProjectDirectionalOptions options;
        options.attribute_ids = {pos_id};
        options.project_vertices = false;
        options.direction = Eigen::Vector3f(0, 0, 1);
        options.cast_mode = lagrange::raycasting::CastMode::BothWays;
        options.fallback_mode = lagrange::raycasting::FallbackMode::Constant;
        options.user_callback = [&](uint64_t /*v*/, bool hit) {
            if (!hit) all_hit = false;
        };

        lagrange::raycasting::project_directional(source, target, options);

        REQUIRE(all_hit);
        REQUIRE(target.has_attribute("pos"));
        auto V = lagrange::vertex_view(target);
        auto P = lagrange::attribute_matrix_view<Scalar>(target, "pos");
        REQUIRE(P.cols() == 2);
        for (Index v = 0; v < target.get_num_vertices(); ++v) {
            REQUIRE((P.row(v) - V.row(v).head<2>()).norm() < 1e-7);
        }
    }

    SECTION("exact copy")
    {
        auto target = copy_geometry(source);

        std::atomic_bool all_hit(true);

        lagrange::raycasting::ProjectDirectionalOptions options;
        options.attribute_ids = {pos_id};
        options.project_vertices = false;
        options.direction = Eigen::Vector3f(0, 0, 1);
        options.cast_mode = lagrange::raycasting::CastMode::BothWays;
        options.fallback_mode = lagrange::raycasting::FallbackMode::Constant;
        options.user_callback = [&](uint64_t /*v*/, bool hit) {
            if (!hit) all_hit = false;
        };

        lagrange::raycasting::project_directional(source, target, options);

        REQUIRE(all_hit);
        REQUIRE(target.has_attribute("pos"));
        auto V = lagrange::vertex_view(target);
        auto P = lagrange::attribute_matrix_view<Scalar>(target, "pos");
        REQUIRE(P.cols() == 2);
        for (Index v = 0; v < target.get_num_vertices(); ++v) {
            CAPTURE(v, P.row(v), V.row(v));
            REQUIRE((P.row(v) - V.row(v).head<2>()).norm() < 1e-7);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

TEST_CASE("SurfaceMesh: project_directional closest vertex fallback", "[raycasting]")
{
    auto source = load_hemisphere_with_pos();
    auto pos_id = source.get_attribute_id("pos");

    // Perturb the mesh so that some vertices are not hit by the directional cast.
    // Using a direction perpendicular to the z-axis ensures partial misses for a hemisphere.
    auto target = perturb_mesh(source, 0.1);

    lagrange::raycasting::ProjectDirectionalOptions options;
    options.attribute_ids = {pos_id};
    options.project_vertices = false;
    options.direction = Eigen::Vector3f(0, 0, 1);
    options.cast_mode = lagrange::raycasting::CastMode::BothWays;
    options.fallback_mode = lagrange::raycasting::FallbackMode::ClosestVertex;

    lagrange::raycasting::project_directional(source, target, options);

    REQUIRE(target.has_attribute("pos"));
    auto P = lagrange::attribute_matrix_view<Scalar>(target, "pos");
    REQUIRE(P.cols() == 2);

    // Every vertex should now have a non-zero attribute value from the fallback.
    // Verify that the attribute was actually written (not left as zero).
    auto source_pos = lagrange::attribute_matrix_view<Scalar>(source, "pos");
    Scalar src_min = source_pos.minCoeff();
    Scalar src_max = source_pos.maxCoeff();
    for (Index v = 0; v < target.get_num_vertices(); ++v) {
        // Each projected channel should lie within the source attribute range.
        for (int c = 0; c < 2; ++c) {
            REQUIRE(P(v, c) >= src_min - 1e-10);
            REQUIRE(P(v, c) <= src_max + 1e-10);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

TEST_CASE("SurfaceMesh: project_closest_point", "[raycasting]")
{
    auto source = load_hemisphere_with_pos();
    auto pos_id = source.get_attribute_id("pos");

    SECTION("perturbed")
    {
        auto target = perturb_mesh(source, 0.1);

        lagrange::raycasting::ProjectCommonOptions options;
        options.attribute_ids = {pos_id};
        options.project_vertices = false;
        lagrange::raycasting::project_closest_point(source, target, options);

        REQUIRE(target.has_attribute("pos"));
        auto P = lagrange::attribute_matrix_view<Scalar>(target, "pos");
        REQUIRE(P.cols() == 2);
    }

    SECTION("exact copy")
    {
        auto target = copy_geometry(source);

        lagrange::raycasting::ProjectCommonOptions options;
        options.attribute_ids = {pos_id};
        options.project_vertices = false;
        lagrange::raycasting::project_closest_point(source, target, options);

        REQUIRE(target.has_attribute("pos"));
        auto V = lagrange::vertex_view(target);
        auto P = lagrange::attribute_matrix_view<Scalar>(target, "pos");
        REQUIRE(P.cols() == 2);
        for (Index v = 0; v < target.get_num_vertices(); ++v) {
            CAPTURE(v, P.row(v), V.row(v));
            REQUIRE((P.row(v) - V.row(v).head<2>()).norm() < 1e-15);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

TEST_CASE("SurfaceMesh: project reproducibility", "[raycasting]")
{
    auto source = load_hemisphere_with_pos();
    auto pos_id = source.get_attribute_id("pos");

    using ProjectMode = lagrange::raycasting::ProjectMode;

    for (auto proj : {ProjectMode::ClosestPoint, ProjectMode::RayCasting}) {
        auto target1 = perturb_mesh(source, 0.1);
        auto target2 = perturb_mesh(source, 0.1);

        // Same seed => same perturbation
        auto V1 = lagrange::vertex_view(target1);
        auto V2 = lagrange::vertex_view(target2);
        REQUIRE(V1 == V2);

        lagrange::raycasting::ProjectOptions options;
        options.project_mode = proj;
        options.attribute_ids = {pos_id};
        options.project_vertices = false;
        options.direction = Eigen::Vector3f(0, 0, 1);

        lagrange::raycasting::project(source, target1, options);
        lagrange::raycasting::project(source, target2, options);

        auto P1 = lagrange::attribute_matrix_view<Scalar>(target1, "pos");
        auto P2 = lagrange::attribute_matrix_view<Scalar>(target2, "pos");
        REQUIRE(P1 == P2);
    }
}

////////////////////////////////////////////////////////////////////////////////

TEST_CASE("SurfaceMesh: project with external RayCaster", "[raycasting]")
{
    auto source = load_hemisphere_with_pos();
    auto pos_id = source.get_attribute_id("pos");

    // Build a RayCaster externally
    lagrange::raycasting::RayCaster rc(
        lagrange::raycasting::SceneFlags::Robust,
        lagrange::raycasting::BuildQuality::High);
    {
        MeshType source_copy = source;
        rc.add_mesh(std::move(source_copy));
    }
    rc.commit_updates();

    auto target = copy_geometry(source);

    // Use the external RayCaster
    lagrange::raycasting::ProjectDirectionalOptions options;
    options.attribute_ids = {pos_id};
    options.project_vertices = false;
    options.direction = Eigen::Vector3f(0, 0, 1);
    options.cast_mode = lagrange::raycasting::CastMode::BothWays;

    lagrange::raycasting::project_directional(source, target, options, &rc);

    REQUIRE(target.has_attribute("pos"));
    auto V = lagrange::vertex_view(target);
    auto P = lagrange::attribute_matrix_view<Scalar>(target, "pos");
    for (Index v = 0; v < target.get_num_vertices(); ++v) {
        REQUIRE((P.row(v) - V.row(v).head<2>()).norm() < 1e-7);
    }
}

////////////////////////////////////////////////////////////////////////////////

TEST_CASE("SurfaceMesh: project_directional with vertex normals", "[raycasting]")
{
    auto source = load_hemisphere_with_pos();
    auto pos_id = source.get_attribute_id("pos");

    SECTION("monostate direction (auto-compute normals)")
    {
        auto target = copy_geometry(source);

        lagrange::raycasting::ProjectDirectionalOptions options;
        options.attribute_ids = {pos_id};
        options.project_vertices = false;
        // direction defaults to monostate: auto-compute vertex normals on target
        options.cast_mode = lagrange::raycasting::CastMode::BothWays;
        options.fallback_mode = lagrange::raycasting::FallbackMode::Constant;

        lagrange::raycasting::project_directional(source, target, options);

        REQUIRE(target.has_attribute("pos"));
        auto P = lagrange::attribute_matrix_view<Scalar>(target, "pos");
        REQUIRE(P.cols() == 2);
        // Since target is a copy of source, all vertices should get a hit via vertex normals.
        auto V = lagrange::vertex_view(target);
        for (Index v = 0; v < target.get_num_vertices(); ++v) {
            REQUIRE((P.row(v) - V.row(v).head<2>()).norm() < 1e-5);
        }
    }

    SECTION("per-vertex direction attribute")
    {
        auto target = copy_geometry(source);

        // Compute vertex normals on the target mesh and pass as direction.
        auto normal_id = lagrange::compute_vertex_normal(target);

        lagrange::raycasting::ProjectDirectionalOptions options;
        options.attribute_ids = {pos_id};
        options.project_vertices = false;
        options.direction = normal_id;
        options.cast_mode = lagrange::raycasting::CastMode::BothWays;
        options.fallback_mode = lagrange::raycasting::FallbackMode::Constant;

        lagrange::raycasting::project_directional(source, target, options);

        REQUIRE(target.has_attribute("pos"));
        auto P = lagrange::attribute_matrix_view<Scalar>(target, "pos");
        REQUIRE(P.cols() == 2);
        auto V = lagrange::vertex_view(target);
        for (Index v = 0; v < target.get_num_vertices(); ++v) {
            REQUIRE((P.row(v) - V.row(v).head<2>()).norm() < 1e-5);
        }
    }
}
