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
#include <lagrange/testing/common.h>
#include <lagrange/testing/create_test_mesh.h>
#include <catch2/benchmark/catch_benchmark.hpp>

#include <lagrange/Attribute.h>
#include <lagrange/combine_meshes.h>
#include <lagrange/compute_facet_normal.h>
#include <lagrange/compute_vertex_normal.h>
#include <lagrange/extract_submesh.h>
#include <lagrange/foreach_attribute.h>
#include <lagrange/mesh_cleanup/split_long_edges.h>
#include <lagrange/separate_by_facet_groups.h>
#include <lagrange/views.h>

#include <numeric>

namespace {

using Scalar = float;
using Index = uint32_t;

/// Validate that a submesh correctly represents a subset of the source mesh.
/// Checks positions, facet connectivity via source mappings, and attributes.
void validate_submesh(
    const lagrange::SurfaceMesh<Scalar, Index>& source,
    const lagrange::SurfaceMesh<Scalar, Index>& submesh,
    std::string_view source_vertex_attr_name,
    std::string_view source_facet_attr_name)
{
    if (submesh.get_num_facets() == 0 && submesh.get_num_vertices() == 0) return;

    REQUIRE(submesh.has_attribute(source_vertex_attr_name));
    REQUIRE(submesh.has_attribute(source_facet_attr_name));

    auto vertex_mapping = lagrange::attribute_vector_view<Index>(submesh, source_vertex_attr_name);
    auto facet_mapping = lagrange::attribute_vector_view<Index>(submesh, source_facet_attr_name);

    const Index num_vertices = submesh.get_num_vertices();
    const Index num_facets = submesh.get_num_facets();

    // Validate vertex positions.
    for (Index i = 0; i < num_vertices; i++) {
        auto src_pos = source.get_position(vertex_mapping[i]);
        auto out_pos = submesh.get_position(i);
        REQUIRE(std::equal(src_pos.begin(), src_pos.end(), out_pos.begin()));
    }

    // Validate facet connectivity.
    for (Index i = 0; i < num_facets; i++) {
        Index src_fi = facet_mapping[i];
        REQUIRE(submesh.get_facet_size(i) == source.get_facet_size(src_fi));
        auto src_f = source.get_facet_vertices(src_fi);
        auto out_f = submesh.get_facet_vertices(i);
        for (Index k = 0; k < submesh.get_facet_size(i); k++) {
            REQUIRE(src_f[k] == vertex_mapping[out_f[k]]);
        }
    }
}

/// Validate indexed attributes by comparing resolved per-corner values.
void validate_indexed_attributes(
    const lagrange::SurfaceMesh<Scalar, Index>& source,
    const lagrange::SurfaceMesh<Scalar, Index>& submesh,
    std::string_view source_facet_attr_name)
{
    if (submesh.get_num_facets() == 0) return;

    auto facet_mapping = lagrange::attribute_vector_view<Index>(submesh, source_facet_attr_name);

    lagrange::seq_foreach_named_attribute_read<lagrange::Indexed>(
        source,
        [&](std::string_view name, auto&& attr) {
            if (source.attr_name_is_reserved(name)) return;
            REQUIRE(submesh.has_attribute(name));

            using AttributeType = std::decay_t<decltype(attr)>;
            using ValueType = typename AttributeType::ValueType;

            auto src_values = lagrange::matrix_view(attr.values());
            auto src_indices = lagrange::vector_view(attr.indices());
            const auto& target_attr = submesh.template get_indexed_attribute<ValueType>(name);
            auto tgt_values = lagrange::matrix_view(target_attr.values());
            auto tgt_indices = lagrange::vector_view(target_attr.indices());

            const Index num_facets = submesh.get_num_facets();
            for (Index fi = 0; fi < num_facets; fi++) {
                Index src_fi = facet_mapping[fi];
                Index fsize = submesh.get_facet_size(fi);
                for (Index lv = 0; lv < fsize; lv++) {
                    Index out_ci = submesh.get_facet_corner_begin(fi) + lv;
                    Index src_ci = source.get_facet_corner_begin(src_fi) + lv;
                    Index out_val = tgt_indices(out_ci);
                    Index src_val = src_indices(src_ci);
                    REQUIRE(tgt_values.row(out_val) == src_values.row(src_val));
                }
            }
        });
}

} // namespace

TEST_CASE("separate_by_facet_groups: empty mesh", "[core][utilities][separate]")
{
    lagrange::SurfaceMesh<Scalar, Index> mesh(3);
    std::vector<Index> groups;
    lagrange::span<const Index> groups_span(groups.data(), groups.size());
    auto result = lagrange::separate_by_facet_groups(mesh, size_t{0}, groups_span, {});
    REQUIRE(result.empty());
}

TEST_CASE("separate_by_facet_groups: single group", "[core][utilities][separate]")
{
    auto mesh = lagrange::testing::create_test_cube<Scalar, Index>();
    const Index num_facets = mesh.get_num_facets();

    std::vector<Index> group_ids(num_facets, 0);

    lagrange::SeparateByFacetGroupsOptions options;
    options.source_vertex_attr_name = "@source_vertex";
    options.source_facet_attr_name = "@source_facet";
    options.map_attributes = true;

    auto result = lagrange::separate_by_facet_groups(
        mesh,
        size_t{1},
        lagrange::span<const Index>(group_ids),
        options);
    REQUIRE(result.size() == 1);
    REQUIRE(result[0].get_num_vertices() == mesh.get_num_vertices());
    REQUIRE(result[0].get_num_facets() == mesh.get_num_facets());

    validate_submesh(
        mesh,
        result[0],
        options.source_vertex_attr_name,
        options.source_facet_attr_name);
    validate_indexed_attributes(mesh, result[0], options.source_facet_attr_name);
}

TEST_CASE("separate_by_facet_groups: two groups", "[core][utilities][separate]")
{
    auto mesh = lagrange::testing::create_test_cube<Scalar, Index>();
    const Index num_facets = mesh.get_num_facets();

    // Split into two groups: first half and second half.
    std::vector<Index> group_ids(num_facets);
    for (Index i = 0; i < num_facets; i++) {
        group_ids[i] = (i < num_facets / 2) ? 0 : 1;
    }

    lagrange::SeparateByFacetGroupsOptions options;
    options.source_vertex_attr_name = "@source_vertex";
    options.source_facet_attr_name = "@source_facet";

    auto result = lagrange::separate_by_facet_groups(
        mesh,
        size_t{2},
        lagrange::span<const Index>(group_ids),
        options);
    REQUIRE(result.size() == 2);
    REQUIRE(result[0].get_num_facets() == num_facets / 2);
    REQUIRE(result[1].get_num_facets() == num_facets - num_facets / 2);

    validate_submesh(
        mesh,
        result[0],
        options.source_vertex_attr_name,
        options.source_facet_attr_name);
    validate_submesh(
        mesh,
        result[1],
        options.source_vertex_attr_name,
        options.source_facet_attr_name);
}

TEST_CASE("separate_by_facet_groups: many small groups", "[core][utilities][separate]")
{
    // Create N disconnected triangles by combining many cubes.
    constexpr size_t N = 50;
    auto cube = lagrange::testing::create_test_cube<Scalar, Index>();
    auto mesh = lagrange::combine_meshes<Scalar, Index>(
        N,
        [&](size_t) -> const lagrange::SurfaceMesh<Scalar, Index>& { return cube; });

    const Index num_facets = mesh.get_num_facets();
    const Index facets_per_cube = cube.get_num_facets();

    // Assign each cube copy its own group.
    std::vector<Index> group_ids(num_facets);
    for (Index i = 0; i < num_facets; i++) {
        group_ids[i] = i / facets_per_cube;
    }

    lagrange::SeparateByFacetGroupsOptions options;
    options.source_vertex_attr_name = "@source_vertex";
    options.source_facet_attr_name = "@source_facet";

    auto result = lagrange::separate_by_facet_groups(
        mesh,
        N,
        lagrange::span<const Index>(group_ids),
        options);
    REQUIRE(result.size() == N);

    for (size_t g = 0; g < N; g++) {
        REQUIRE(result[g].get_num_facets() == facets_per_cube);
        REQUIRE(result[g].get_num_vertices() == cube.get_num_vertices());
        validate_submesh(
            mesh,
            result[g],
            options.source_vertex_attr_name,
            options.source_facet_attr_name);
    }
}

TEST_CASE("separate_by_facet_groups: shared vertices across groups", "[core][utilities][separate]")
{
    // Build a simple mesh: two triangles sharing an edge (vertices 0,1 shared).
    // Triangle 0: (0,1,2), Triangle 1: (0,1,3)
    lagrange::SurfaceMesh<Scalar, Index> mesh(3);
    mesh.add_vertices(4);
    auto V = lagrange::vertex_ref(mesh);
    V.row(0) << 0, 0, 0;
    V.row(1) << 1, 0, 0;
    V.row(2) << 0, 1, 0;
    V.row(3) << 1, 1, 0;
    mesh.add_triangle(0, 1, 2);
    mesh.add_triangle(0, 1, 3);

    std::vector<Index> group_ids = {0, 1};

    lagrange::SeparateByFacetGroupsOptions options;
    options.source_vertex_attr_name = "@source_vertex";
    options.source_facet_attr_name = "@source_facet";

    auto result = lagrange::separate_by_facet_groups(
        mesh,
        size_t{2},
        lagrange::span<const Index>(group_ids),
        options);
    REQUIRE(result.size() == 2);

    // Each triangle should have 3 vertices (shared vertices duplicated).
    REQUIRE(result[0].get_num_vertices() == 3);
    REQUIRE(result[0].get_num_facets() == 1);
    REQUIRE(result[1].get_num_vertices() == 3);
    REQUIRE(result[1].get_num_facets() == 1);

    validate_submesh(
        mesh,
        result[0],
        options.source_vertex_attr_name,
        options.source_facet_attr_name);
    validate_submesh(
        mesh,
        result[1],
        options.source_vertex_attr_name,
        options.source_facet_attr_name);
}

TEST_CASE("separate_by_facet_groups: attribute mapping", "[core][utilities][separate]")
{
    auto mesh = lagrange::testing::create_test_cube<Scalar, Index>();
    lagrange::compute_facet_normal(mesh);
    lagrange::compute_vertex_normal(mesh);

    const Index num_facets = mesh.get_num_facets();
    std::vector<Index> group_ids(num_facets);
    for (Index i = 0; i < num_facets; i++) {
        group_ids[i] = (i < num_facets / 2) ? 0 : 1;
    }

    lagrange::SeparateByFacetGroupsOptions options;
    options.source_vertex_attr_name = "@source_vertex";
    options.source_facet_attr_name = "@source_facet";
    options.map_attributes = true;

    auto result = lagrange::separate_by_facet_groups(
        mesh,
        size_t{2},
        lagrange::span<const Index>(group_ids),
        options);
    REQUIRE(result.size() == 2);

    for (auto& submesh : result) {
        validate_submesh(
            mesh,
            submesh,
            options.source_vertex_attr_name,
            options.source_facet_attr_name);
        validate_indexed_attributes(mesh, submesh, options.source_facet_attr_name);
    }
}

TEST_CASE("separate_by_facet_groups: source mapping attributes", "[core][utilities][separate]")
{
    auto mesh = lagrange::testing::create_test_cube<Scalar, Index>();
    const Index num_facets = mesh.get_num_facets();

    std::vector<Index> group_ids(num_facets, 0);

    lagrange::SeparateByFacetGroupsOptions options;
    options.source_vertex_attr_name = "@sv";
    options.source_facet_attr_name = "@sf";

    auto result = lagrange::separate_by_facet_groups(
        mesh,
        size_t{1},
        lagrange::span<const Index>(group_ids),
        options);
    REQUIRE(result.size() == 1);
    REQUIRE(result[0].has_attribute("@sv"));
    REQUIRE(result[0].has_attribute("@sf"));

    auto sv = lagrange::attribute_vector_view<Index>(result[0], "@sv");
    auto sf = lagrange::attribute_vector_view<Index>(result[0], "@sf");

    // All vertices and facets should map to valid source indices.
    for (Index i = 0; i < result[0].get_num_vertices(); i++) {
        REQUIRE(sv[i] < mesh.get_num_vertices());
    }
    for (Index i = 0; i < result[0].get_num_facets(); i++) {
        REQUIRE(sf[i] < mesh.get_num_facets());
    }
}

TEST_CASE("separate_by_facet_groups: hybrid mesh", "[core][utilities][separate]")
{
    // Build a hybrid mesh with triangles and quads.
    lagrange::SurfaceMesh<Scalar, Index> mesh(3);
    mesh.add_vertices(6);
    auto V = lagrange::vertex_ref(mesh);
    V.row(0) << 0, 0, 0;
    V.row(1) << 1, 0, 0;
    V.row(2) << 1, 1, 0;
    V.row(3) << 0, 1, 0;
    V.row(4) << 2, 0, 0;
    V.row(5) << 2, 1, 0;

    // Quad: (0,1,2,3), Triangle: (1,4,5)
    mesh.add_quad(0, 1, 2, 3);
    mesh.add_triangle(1, 4, 5);

    std::vector<Index> group_ids = {0, 1};

    lagrange::SeparateByFacetGroupsOptions options;
    options.source_vertex_attr_name = "@source_vertex";
    options.source_facet_attr_name = "@source_facet";

    auto result = lagrange::separate_by_facet_groups(
        mesh,
        size_t{2},
        lagrange::span<const Index>(group_ids),
        options);
    REQUIRE(result.size() == 2);
    REQUIRE(result[0].get_num_facets() == 1);
    REQUIRE(result[0].get_num_vertices() == 4);
    REQUIRE(result[0].get_facet_size(0) == 4); // quad
    REQUIRE(result[1].get_num_facets() == 1);
    REQUIRE(result[1].get_num_vertices() == 3);
    REQUIRE(result[1].get_facet_size(0) == 3); // triangle

    validate_submesh(
        mesh,
        result[0],
        options.source_vertex_attr_name,
        options.source_facet_attr_name);
    validate_submesh(
        mesh,
        result[1],
        options.source_vertex_attr_name,
        options.source_facet_attr_name);
}

TEST_CASE("separate_by_facet_groups: function_ref overload", "[core][utilities][separate]")
{
    auto mesh = lagrange::testing::create_test_cube<Scalar, Index>();
    const Index num_facets = mesh.get_num_facets();

    // Compare span-based and function_ref-based overloads.
    std::vector<Index> group_ids(num_facets);
    for (Index i = 0; i < num_facets; i++) {
        group_ids[i] = (i < num_facets / 2) ? 0 : 1;
    }

    lagrange::SeparateByFacetGroupsOptions options;
    options.source_vertex_attr_name = "@source_vertex";
    options.source_facet_attr_name = "@source_facet";

    auto result_span = lagrange::separate_by_facet_groups(
        mesh,
        size_t{2},
        lagrange::span<const Index>(group_ids),
        options);

    auto result_func = lagrange::separate_by_facet_groups(
        mesh,
        size_t{2},
        lagrange::function_ref<Index(Index)>(
            [&](Index fi) -> Index { return (fi < num_facets / 2) ? 0 : 1; }),
        options);

    REQUIRE(result_span.size() == result_func.size());
    for (size_t g = 0; g < result_span.size(); g++) {
        REQUIRE(result_span[g].get_num_vertices() == result_func[g].get_num_vertices());
        REQUIRE(result_span[g].get_num_facets() == result_func[g].get_num_facets());
    }
}

TEST_CASE("separate_by_facet_groups: reference equivalence", "[core][utilities][separate]")
{
    auto mesh = lagrange::testing::create_test_cube<Scalar, Index>();
    lagrange::compute_facet_normal(mesh);
    lagrange::compute_vertex_normal(mesh);

    const Index num_facets = mesh.get_num_facets();
    std::vector<Index> group_ids(num_facets);
    for (Index i = 0; i < num_facets; i++) {
        group_ids[i] = i % 3;
    }

    lagrange::SeparateByFacetGroupsOptions options;
    options.source_vertex_attr_name = "@source_vertex";
    options.source_facet_attr_name = "@source_facet";
    options.map_attributes = true;

    auto result = lagrange::separate_by_facet_groups(
        mesh,
        size_t{3},
        lagrange::span<const Index>(group_ids),
        options);

    // Build reference results by manually calling extract_submesh per group.
    // Sort facets into groups.
    std::vector<std::vector<Index>> facets_per_group(3);
    for (Index i = 0; i < num_facets; i++) {
        facets_per_group[group_ids[i]].push_back(i);
    }

    lagrange::SubmeshOptions sub_options;
    sub_options.source_vertex_attr_name = options.source_vertex_attr_name;
    sub_options.source_facet_attr_name = options.source_facet_attr_name;
    sub_options.map_attributes = true;

    for (size_t g = 0; g < 3; g++) {
        auto ref = lagrange::extract_submesh(
            mesh,
            lagrange::span<const Index>(facets_per_group[g]),
            sub_options);
        REQUIRE(result[g].get_num_vertices() == ref.get_num_vertices());
        REQUIRE(result[g].get_num_facets() == ref.get_num_facets());

        // Compare positions.
        auto ref_v = lagrange::vertex_view(ref);
        auto out_v = lagrange::vertex_view(result[g]);
        REQUIRE(ref_v == out_v);

        // Compare facet connectivity.
        for (Index fi = 0; fi < ref.get_num_facets(); fi++) {
            auto ref_f = ref.get_facet_vertices(fi);
            auto out_f = result[g].get_facet_vertices(fi);
            REQUIRE(std::equal(ref_f.begin(), ref_f.end(), out_f.begin()));
        }

        // Compare source mapping attributes.
        auto ref_sv = lagrange::attribute_vector_view<Index>(ref, "@source_vertex");
        auto out_sv = lagrange::attribute_vector_view<Index>(result[g], "@source_vertex");
        REQUIRE(std::equal(ref_sv.begin(), ref_sv.end(), out_sv.begin()));

        auto ref_sf = lagrange::attribute_vector_view<Index>(ref, "@source_facet");
        auto out_sf = lagrange::attribute_vector_view<Index>(result[g], "@source_facet");
        REQUIRE(std::equal(ref_sf.begin(), ref_sf.end(), out_sf.begin()));

        // Compare indexed attributes by resolved per-corner values.
        validate_indexed_attributes(mesh, result[g], "@source_facet");
    }
}

TEST_CASE("separate_by_facet_groups: groups with zero facets", "[core][utilities][separate]")
{
    auto mesh = lagrange::testing::create_test_cube<Scalar, Index>();
    const Index num_facets = mesh.get_num_facets();

    // All facets in group 0, but claim there are 5 groups.
    std::vector<Index> group_ids(num_facets, 0);

    auto result = lagrange::separate_by_facet_groups(
        mesh,
        size_t{5},
        lagrange::span<const Index>(group_ids),
        {});
    REQUIRE(result.size() == 5);
    REQUIRE(result[0].get_num_facets() == num_facets);
    for (size_t g = 1; g < 5; g++) {
        REQUIRE(result[g].get_num_facets() == 0);
        REQUIRE(result[g].get_num_vertices() == 0);
    }
}

TEST_CASE(
    "separate_by_facet_groups: single-facet groups on hybrid input",
    "[core][utilities][separate]")
{
    // Build a hybrid mesh: mix of tris and quads, one facet per group.
    lagrange::SurfaceMesh<Scalar, Index> mesh(3);
    mesh.add_vertices(7);
    auto V = lagrange::vertex_ref(mesh);
    V.row(0) << 0, 0, 0;
    V.row(1) << 1, 0, 0;
    V.row(2) << 0.5f, 1, 0;
    V.row(3) << 2, 0, 0;
    V.row(4) << 3, 0, 0;
    V.row(5) << 3, 1, 0;
    V.row(6) << 2, 1, 0;

    mesh.add_triangle(0, 1, 2);
    mesh.add_quad(3, 4, 5, 6);

    std::vector<Index> group_ids = {0, 1};

    lagrange::SeparateByFacetGroupsOptions options;
    options.source_vertex_attr_name = "@sv";
    options.source_facet_attr_name = "@sf";

    auto result = lagrange::separate_by_facet_groups(
        mesh,
        size_t{2},
        lagrange::span<const Index>(group_ids),
        options);
    REQUIRE(result.size() == 2);
    REQUIRE(result[0].get_num_facets() == 1);
    REQUIRE(result[0].get_facet_size(0) == 3);
    REQUIRE(result[1].get_num_facets() == 1);
    REQUIRE(result[1].get_facet_size(0) == 4);

    validate_submesh(mesh, result[0], "@sv", "@sf");
    validate_submesh(mesh, result[1], "@sv", "@sf");
}

TEST_CASE(
    "separate_by_facet_groups: span overload (auto num_groups)",
    "[core][utilities][separate]")
{
    auto mesh = lagrange::testing::create_test_cube<Scalar, Index>();
    const Index num_facets = mesh.get_num_facets();

    std::vector<Index> group_ids(num_facets);
    for (Index i = 0; i < num_facets; i++) {
        group_ids[i] = i % 4;
    }

    // Use the overload that deduces num_groups from max element.
    auto result =
        lagrange::separate_by_facet_groups(mesh, lagrange::span<const Index>(group_ids), {});
    REQUIRE(result.size() == 4);

    Index total_facets = 0;
    for (auto& m : result) {
        total_facets += m.get_num_facets();
    }
    REQUIRE(total_facets == num_facets);
}

TEST_CASE("separate_by_facet_groups benchmark", "[core][utilities][separate][!benchmark]")
{
    using BScalar = double;
    using BIndex = uint32_t;

    SECTION("many tiny groups")
    {
        auto cube = lagrange::testing::create_test_cube<BScalar, BIndex>();
        lagrange::split_long_edges(cube);
        constexpr size_t N = 1000;
        auto mesh = lagrange::combine_meshes<BScalar, BIndex>(
            N,
            [&](size_t) -> const lagrange::SurfaceMesh<BScalar, BIndex>& { return cube; });
        lagrange::logger().info(
            "Mesh has {} vertices and {} facets",
            mesh.get_num_vertices(),
            mesh.get_num_facets());

        const BIndex num_facets = mesh.get_num_facets();
        const BIndex facets_per_cube = cube.get_num_facets();
        std::vector<BIndex> group_ids(num_facets);
        for (BIndex i = 0; i < num_facets; i++) {
            group_ids[i] = i / facets_per_cube;
        }

        BENCHMARK("1000 tiny groups")
        {
            return lagrange::separate_by_facet_groups(
                mesh,
                N,
                lagrange::span<const BIndex>(group_ids),
                {});
        };
    }

    SECTION("few large groups")
    {
        auto cube = lagrange::testing::create_test_cube<BScalar, BIndex>();
        auto sphere = lagrange::testing::create_test_sphere<BScalar, BIndex>();
        lagrange::split_long_edges(cube);
        lagrange::split_long_edges(sphere);
        cube.delete_attribute("@normal");
        auto mesh = lagrange::combine_meshes<BScalar, BIndex>({&cube, &sphere});
        lagrange::logger().info(
            "Mesh has {} vertices and {} facets",
            mesh.get_num_vertices(),
            mesh.get_num_facets());

        const BIndex num_facets = mesh.get_num_facets();
        std::vector<BIndex> group_ids(num_facets);
        for (BIndex i = 0; i < num_facets; i++) {
            group_ids[i] = (i < cube.get_num_facets()) ? 0 : 1;
        }

        BENCHMARK("2 large groups")
        {
            return lagrange::separate_by_facet_groups(
                mesh,
                size_t{2},
                lagrange::span<const BIndex>(group_ids),
                {});
        };
    }

    SECTION("single group")
    {
        auto cube = lagrange::testing::create_test_cube<BScalar, BIndex>();
        lagrange::split_long_edges(cube);
        const BIndex num_facets = cube.get_num_facets();
        std::vector<BIndex> group_ids(num_facets, 0);
        lagrange::logger().info(
            "Mesh has {} vertices and {} facets",
            cube.get_num_vertices(),
            cube.get_num_facets());

        BENCHMARK("single group")
        {
            return lagrange::separate_by_facet_groups(
                cube,
                size_t{1},
                lagrange::span<const BIndex>(group_ids),
                {});
        };
    }

    SECTION("many tiny groups with attributes")
    {
        auto cube = lagrange::testing::create_test_cube<BScalar, BIndex>();
        lagrange::split_long_edges(cube);
        lagrange::compute_facet_normal(cube);
        lagrange::compute_vertex_normal(cube);

        constexpr size_t N = 1000;
        auto mesh = lagrange::combine_meshes<BScalar, BIndex>(
            N,
            [&](size_t) -> const lagrange::SurfaceMesh<BScalar, BIndex>& { return cube; });
        lagrange::logger().info(
            "Mesh has {} vertices and {} facets",
            mesh.get_num_vertices(),
            mesh.get_num_facets());
        mesh.delete_attribute("@edge_length");
        mesh.clear_edges();

        const BIndex num_facets = mesh.get_num_facets();
        const BIndex facets_per_cube = cube.get_num_facets();
        std::vector<BIndex> group_ids(num_facets);
        for (BIndex i = 0; i < num_facets; i++) {
            group_ids[i] = i / facets_per_cube;
        }

        lagrange::SeparateByFacetGroupsOptions options;
        options.map_attributes = true;

        BENCHMARK("1000 tiny groups with attrs")
        {
            return lagrange::separate_by_facet_groups(
                mesh,
                N,
                lagrange::span<const BIndex>(group_ids),
                options);
        };
    }
}
