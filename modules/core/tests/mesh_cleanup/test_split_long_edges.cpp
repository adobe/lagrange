/*
 * Copyright 2017 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <lagrange/Attribute.h>
#include <lagrange/IndexedAttribute.h>
#include <lagrange/compute_area.h>
#include <lagrange/compute_edge_lengths.h>
#include <lagrange/compute_facet_area.h>
#include <lagrange/io/save_mesh.h>
#include <lagrange/mesh_cleanup/split_long_edges.h>
#include <lagrange/mesh_convert.h>
#include <lagrange/testing/common.h>
#include <lagrange/views.h>

TEST_CASE("split_long_edges", "[surface][cleanup]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    auto check_edge_length =
        [](auto& mesh, Scalar max_edge_length, std::string_view active_attr_name = "") {
            auto is_active = [&](Index fid) {
                if (active_attr_name.empty()) return true;
                auto active_view = attribute_vector_view<uint8_t>(mesh, active_attr_name);
                return static_cast<bool>(active_view[fid]);
            };
            auto attr_id = compute_edge_lengths(mesh);
            auto edge_lengths = attribute_vector_view<Scalar>(mesh, attr_id);
            for (Index fid = 0; fid < mesh.get_num_facets(); fid++) {
                if (!is_active(fid)) continue;
                for (Index i = 0; i < 3; i++) {
                    Index eid = mesh.get_edge(fid, i);
                    REQUIRE(edge_lengths(eid) <= max_edge_length);
                }
            }
        };

    auto check_area = [](auto& mesh, Scalar total_area) {
        REQUIRE_THAT(compute_mesh_area(mesh), Catch::Matchers::WithinRel(total_area, 1e-6));
    };

    SECTION("Single triangle")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_triangle(0, 1, 2);

        SplitLongEdgesOptions options;
        options.max_edge_length = 0.5;
        options.recursive = true;
        split_long_edges(mesh, options);
        REQUIRE(mesh.get_num_vertices() == 9);
        REQUIRE(mesh.get_num_facets() == 9);
        check_edge_length(mesh, options.max_edge_length);
        check_area(mesh, 0.5);
    }

    SECTION("Two triangles")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({1, 1, 0});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(2, 1, 3);

        SECTION("non-recursive")
        {
            SplitLongEdgesOptions options;
            options.max_edge_length = 0.5;
            options.recursive = false;
            split_long_edges(mesh, options);
            REQUIRE(mesh.get_num_facets() == 10);
            check_area(mesh, 1);
        }

        SECTION("recursive")
        {
            SplitLongEdgesOptions options;
            options.max_edge_length = 0.5;
            options.recursive = true;
            split_long_edges(mesh, options);
            REQUIRE(mesh.get_num_facets() == 18);
            check_edge_length(mesh, options.max_edge_length);
            check_area(mesh, 1);
        }

        SECTION("with active region")
        {
            uint8_t active_buffer[2] = {1, 0};
            mesh.create_attribute<uint8_t>(
                "active",
                AttributeElement::Facet,
                AttributeUsage::Scalar,
                1,
                active_buffer);

            SplitLongEdgesOptions options;
            options.max_edge_length = 0.5;
            options.active_region_attribute = "active";
            options.recursive = true;
            split_long_edges(mesh, options);
            REQUIRE(mesh.get_num_facets() == 12);
            REQUIRE(mesh.has_attribute("active"));
            check_edge_length(mesh, options.max_edge_length, "active");
            check_area(mesh, 1);
        }

        SECTION("with uv")
        {
            {
                Scalar uv[8] = {0, 0, 1, 0, 0, 1, 1, 1};
                Index uv_indices[6] = {0, 1, 2, 2, 1, 3};
                mesh.create_attribute<Scalar>(
                    "uv",
                    AttributeElement::Indexed,
                    AttributeUsage::Vector,
                    2,
                    uv,
                    uv_indices);
            }
            SplitLongEdgesOptions options;
            options.max_edge_length = 0.5;
            options.recursive = true;
            split_long_edges(mesh, options);
            REQUIRE(mesh.get_num_facets() == 18);
            check_edge_length(mesh, options.max_edge_length);
            check_area(mesh, 1);

            REQUIRE(mesh.has_attribute("uv"));

            // Ensure uv values are correctly interpolated.
            auto& attr = mesh.get_indexed_attribute<Scalar>("uv");
            auto& uv_values = matrix_view(attr.values());
            auto& uv_indices = vector_view(attr.indices());
            auto vertices = vertex_view(mesh);
            auto facets = facet_view(mesh);
            Index num_facets = mesh.get_num_facets();
            for (Index fid = 0; fid < num_facets; fid++) {
                auto v0 = vertices.row(facets(fid, 0)).template segment<2>(0).eval();
                auto v1 = vertices.row(facets(fid, 1)).template segment<2>(0).eval();
                auto v2 = vertices.row(facets(fid, 2)).template segment<2>(0).eval();

                auto uv_idx_0 = uv_indices(fid * 3);
                auto uv_idx_1 = uv_indices(fid * 3 + 1);
                auto uv_idx_2 = uv_indices(fid * 3 + 2);

                auto uv0 = uv_values.row(uv_idx_0).template segment<2>(0).eval();
                auto uv1 = uv_values.row(uv_idx_1).template segment<2>(0).eval();
                auto uv2 = uv_values.row(uv_idx_2).template segment<2>(0).eval();

                REQUIRE_THAT((v0 - uv0).norm(), Catch::Matchers::WithinAbs(0, 1e-6));
                REQUIRE_THAT((v1 - uv1).norm(), Catch::Matchers::WithinAbs(0, 1e-6));
                REQUIRE_THAT((v2 - uv2).norm(), Catch::Matchers::WithinAbs(0, 1e-6));
            }
        }
    }
}

TEST_CASE("split_long_edges benchmark", "[surface][mesh_cleanup][!benchmark]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/ball.obj");

    SplitLongEdgesOptions options;
    options.max_edge_length = 0.1f;
    options.recursive = true;

    BENCHMARK_ADVANCED("split_long_edges")(Catch::Benchmark::Chronometer meter)
    {
        auto mesh_copy = SurfaceMesh<Scalar, Index>::stripped_copy(mesh);
        meter.measure([&mesh_copy, options] { split_long_edges(mesh_copy, options); });
        REQUIRE(mesh_copy.get_num_facets() > mesh.get_num_facets());
    };

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
    using MeshType = TriangleMesh3D;
    auto legacy_mesh = to_legacy_mesh<MeshType>(mesh);
    BENCHMARK("legacy::split_long_edges")
    {
        auto split_mesh = legacy::split_long_edges(
            *legacy_mesh,
            options.max_edge_length * options.max_edge_length,
            options.recursive);
        REQUIRE(split_mesh->get_num_facets() > legacy_mesh->get_num_facets());
        return split_mesh;
    };
#endif
}

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
    #include <lagrange/Mesh.h>
    #include <lagrange/common.h>
    #include <lagrange/create_mesh.h>
TEST_CASE("SplitLongEdgesTest", "[split_long_edges][triangle_mesh][cleanup]" LA_SLOW_DEBUG_FLAG)
{
    using namespace lagrange;
    Vertices3D vertices(3, 3);
    vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0;
    Triangles facets(1, 3);
    facets << 0, 1, 2;

    using Scalar = typename Vertices3D::Scalar;

    auto mesh = create_mesh(vertices, facets);
    REQUIRE(mesh->get_num_vertices() == 3);
    REQUIRE(mesh->get_num_facets() == 1);

    mesh->initialize_uv(vertices.leftCols(2).eval(), facets);
    REQUIRE(mesh->is_uv_initialized());

    SECTION("no split")
    {
        auto mesh2 = split_long_edges(*mesh, 2.0);
        REQUIRE(mesh2->get_num_vertices() == 3);
        REQUIRE(mesh2->get_num_facets() == 1);

        const auto uv_areas = compute_uv_area_raw(mesh2->get_uv(), mesh2->get_uv_indices());
        REQUIRE(uv_areas.sum() == Catch::Approx(0.5));
    }

    SECTION("single split")
    {
        auto mesh2 = split_long_edges(*mesh, 1.5);
        REQUIRE(mesh2->get_num_vertices() == 4);
        REQUIRE(mesh2->get_num_facets() == 2);

        const auto uv_areas = compute_uv_area_raw(mesh2->get_uv(), mesh2->get_uv_indices());
        REQUIRE(uv_areas.sum() == Catch::Approx(0.5));
    }

    SECTION("Two triangles")
    {
        vertices.conservativeResize(4, 3);
        vertices.row(3) << 1.0, 1.0, 0.0;
        facets.resize(2, 3);
        facets << 0, 1, 2, 2, 1, 3;
        mesh->initialize_uv(vertices.leftCols(2).eval(), facets);
        REQUIRE(mesh->is_uv_initialized());

        mesh->import_vertices(vertices);
        mesh->import_facets(facets);

        SECTION("simple")
        {
            auto mesh2 = split_long_edges(*mesh, 1.5);
            REQUIRE(mesh2->get_num_vertices() == 5);
            REQUIRE(mesh2->get_num_facets() == 4);

            const auto uv_areas = compute_uv_area_raw(mesh2->get_uv(), mesh2->get_uv_indices());
            REQUIRE(uv_areas.sum() == Catch::Approx(1.0));
        }

        SECTION("with attribute")
        {
            using MeshType = decltype(mesh)::element_type;
            using IndexArray = typename MeshType::AttributeArray;
            IndexArray vertex_indices(4, 1);
            vertex_indices << 0, 1, 2, 3;
            IndexArray facet_indices(2, 1);
            facet_indices << 0, 1;

            mesh->add_vertex_attribute("index");
            mesh->set_vertex_attribute("index", vertex_indices);
            mesh->add_facet_attribute("index");
            mesh->set_facet_attribute("index", facet_indices);

            auto mesh2 = split_long_edges(*mesh, 1.5);
            REQUIRE(mesh2->get_num_vertices() == 5);
            REQUIRE(mesh2->get_num_facets() == 4);

            REQUIRE(mesh2->has_vertex_attribute("index"));
            REQUIRE(mesh2->has_facet_attribute("index"));

            const auto& v_idx = mesh2->get_vertex_attribute("index");
            const auto& f_idx = mesh2->get_facet_attribute("index");

            REQUIRE(v_idx.rows() == 5);
            REQUIRE(f_idx.rows() == 4);

            const auto& ori_vts = mesh->get_vertices();
            const auto& vts = mesh2->get_vertices();
            for (size_t i = 0; i < 5; i++) {
                Scalar int_part, frac_part;
                frac_part = modf(v_idx(i, 0), &int_part);
                if (frac_part == 0.0) {
                    REQUIRE(vts.row(i) == ori_vts.row(safe_cast<int>(int_part)));
                } else {
                    REQUIRE(v_idx(i, 0) == 1.5);
                }
            }

            REQUIRE(f_idx.minCoeff() == 0);
            REQUIRE(f_idx.maxCoeff() == 1);

            const auto uv_areas = compute_uv_area_raw(mesh2->get_uv(), mesh2->get_uv_indices());
            REQUIRE(uv_areas.sum() == Catch::Approx(1.0));
        }
    }

    SECTION("with normals")
    {
        using MeshType = decltype(mesh)::element_type;
        mesh = testing::load_mesh<MeshType>("open/core/bunny_simple.obj");
        REQUIRE(mesh->get_num_vertices() == 2503);
        REQUIRE(mesh->get_num_facets() == 5002);
        REQUIRE(mesh->has_corner_attribute("normal"));
        map_corner_attribute_to_indexed_attribute(*mesh, "normal");
        REQUIRE(mesh->has_indexed_attribute("normal"));

        auto mesh2 = split_long_edges(*mesh, 0.0001, true);
        REQUIRE(mesh2->has_indexed_attribute("normal"));
    }

    SECTION("with uv")
    {
        using MeshType = decltype(mesh)::element_type;
        mesh = testing::load_mesh<MeshType>("open/core/blub_open.obj");
        REQUIRE(mesh->get_num_vertices() == 5857);
        REQUIRE(mesh->get_num_facets() == 11648);
        REQUIRE(mesh->is_uv_initialized());
        auto mesh2 = split_long_edges(*mesh, 0.0001, true);
        REQUIRE(mesh2->is_uv_initialized());
    }
}
#endif
