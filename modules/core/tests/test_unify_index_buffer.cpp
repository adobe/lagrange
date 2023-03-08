/*
 * Copyright 2022 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/IndexedAttribute.h>
#include <lagrange/Logger.h>
#include <lagrange/create_mesh.h>
#include <lagrange/foreach_attribute.h>
#include <lagrange/mesh_convert.h>
#include <lagrange/testing/common.h>
#include <lagrange/unify_index_buffer.h>
#include <lagrange/utils/range.h>

#include <Eigen/Core>

#include <algorithm>
#include <vector>
#include <numeric>

namespace {
using namespace lagrange;

/**
 * 3     2
 *  *---*
 *  | / |
 *  *---*
 * 0     1
 */
template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> generate_square()
{
    lagrange::SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({1, 1, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_triangle(0, 1, 2);
    mesh.add_triangle(2, 3, 0);
    return mesh;
}

/**
 * 3     2   5
 *  *---*---*
 *  | / |   |
 *  *---*---*
 * 0     1   4
 */
template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> generate_rectangle()
{
    lagrange::SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({1, 1, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_vertex({2, 0, 0});
    mesh.add_vertex({2, 1, 0});
    mesh.add_triangle(0, 1, 2);
    mesh.add_triangle(2, 3, 0);
    mesh.add_quad(2, 1, 4, 5);
    return mesh;
}

} // namespace

TEST_CASE("unify_index_buffer", "[attribute][next][unify]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    auto check_for_consistency = [&](const auto& mesh1, const auto& mesh2) {
        REQUIRE(mesh1.get_num_facets() == mesh2.get_num_facets());
        REQUIRE(mesh1.get_num_corners() == mesh2.get_num_corners());

        // Check for vertex positions.
        auto num_corners = mesh1.get_num_corners();
        for (auto cid : lagrange::range(num_corners)) {
            auto vid1 = mesh1.get_corner_vertex(cid);
            auto vid2 = mesh2.get_corner_vertex(cid);

            auto v1 = mesh1.get_position(vid1);
            auto v2 = mesh2.get_position(vid2);
            REQUIRE(std::equal(v1.begin(), v1.end(), v2.begin()));
        }

        auto vertex_attributes_are_equivalent = [&](const auto& attr1, const auto& attr2) {
            for (auto cid : lagrange::range(num_corners)) {
                const auto vid1 = mesh1.get_corner_vertex(cid);
                const auto vid2 = mesh2.get_corner_vertex(cid);

                auto v1 = attr1.get_row(vid1);
                auto v2 = attr2.get_row(vid2);
                REQUIRE(v1.size() == v2.size());
                REQUIRE(std::equal(v1.begin(), v1.end(), v2.begin()));
            }
        };

        auto facet_attributes_are_equivalent = [&](const auto& attr1, const auto& attr2) {
            for (auto cid : lagrange::range(num_corners)) {
                const auto fid1 = mesh1.get_corner_facet(cid);
                const auto fid2 = mesh2.get_corner_facet(cid);

                auto v1 = attr1.get_row(fid1);
                auto v2 = attr2.get_row(fid2);
                REQUIRE(v1.size() == v2.size());
                REQUIRE(std::equal(v1.begin(), v1.end(), v2.begin()));
            }
        };

        auto corner_attributes_are_equivalent = [&](const auto& attr1, const auto& attr2) {
            for (auto cid : lagrange::range(num_corners)) {
                auto v1 = attr1.get_row(cid);
                auto v2 = attr2.get_row(cid);
                REQUIRE(v1.size() == v2.size());
                REQUIRE(std::equal(v1.begin(), v1.end(), v2.begin()));
            }
        };

        auto indexed_attributes_are_equivalent = [&](const auto& attr1, const auto& attr2) {
            const auto& values_1 = attr1.values();
            const auto& indices_1 = attr1.indices();
            const auto& values_2 = attr2.values();
            const auto& indices_2 = attr2.indices();

            for (auto cid : lagrange::range(num_corners)) {
                auto vid1 = indices_1.get(cid);
                auto vid2 = indices_2.get(cid);

                auto v1 = values_1.get_row(vid1);
                auto v2 = values_2.get_row(vid2);
                REQUIRE(v1.size() == v2.size());
                REQUIRE(std::equal(v1.begin(), v1.end(), v2.begin()));
            }
        };

        auto indexed_and_vertex_attributes_are_equivalent = [&](const auto& attr1,
                                                                const auto& attr2) {
            const auto& values = attr1.values();
            const auto& indices = attr1.indices();

            for (auto cid : lagrange::range(num_corners)) {
                auto vid1 = indices.get(cid);
                auto vid2 = mesh2.get_corner_vertex(cid);

                auto v1 = values.get_row(vid1);
                auto v2 = attr2.get_row(vid2);
                REQUIRE(v1.size() == v2.size());
                REQUIRE(std::equal(v1.begin(), v1.end(), v2.begin()));
            }
        };

        // Check for vertex attributes in input.
        seq_foreach_named_attribute_read<Vertex>(mesh1, [&](std::string_view name, auto&& attr1) {
            if (mesh1.attr_name_is_reserved(name)) return;
            using ValueType = typename std::decay_t<decltype(attr1)>::ValueType;
            REQUIRE(mesh2.has_attribute(name));
            REQUIRE(mesh2.template is_attribute_type<ValueType>(name));
            const auto& attr2 = mesh2.template get_attribute<ValueType>(name);
            vertex_attributes_are_equivalent(attr1, attr2);
        });

        // Check for facet attributes in input.
        seq_foreach_named_attribute_read<Facet>(mesh1, [&](std::string_view name, auto&& attr1) {
            if (mesh1.attr_name_is_reserved(name)) return;
            using ValueType = typename std::decay_t<decltype(attr1)>::ValueType;
            REQUIRE(mesh2.has_attribute(name));
            REQUIRE(mesh2.template is_attribute_type<ValueType>(name));
            const auto& attr2 = mesh2.template get_attribute<ValueType>(name);
            facet_attributes_are_equivalent(attr1, attr2);
        });

        // Check for corner attribute in input.
        seq_foreach_named_attribute_read<Corner>(mesh1, [&](std::string_view name, auto&& attr1) {
            if (mesh1.attr_name_is_reserved(name)) return;
            using ValueType = typename std::decay_t<decltype(attr1)>::ValueType;
            REQUIRE(mesh2.has_attribute(name));
            REQUIRE(mesh2.template is_attribute_type<ValueType>(name));
            const auto& attr2 = mesh2.template get_attribute<ValueType>(name);
            corner_attributes_are_equivalent(attr1, attr2);
        });

        // Check for indexed attributes in input.
        seq_foreach_named_attribute_read<Indexed>(mesh1, [&](std::string_view name, auto&& attr1) {
            if (mesh1.attr_name_is_reserved(name)) return;

            using ValueType = typename std::decay_t<decltype(attr1)>::ValueType;
            REQUIRE(mesh2.has_attribute(name));
            REQUIRE(mesh2.template is_attribute_type<ValueType>(name));
            if (mesh2.is_attribute_indexed(name)) {
                const auto& attr2 = mesh2.template get_indexed_attribute<ValueType>(name);
                indexed_attributes_are_equivalent(attr1, attr2);
            } else {
                const auto& attr2 = mesh2.template get_attribute<ValueType>(name);
                indexed_and_vertex_attributes_are_equivalent(attr1, attr2);
            }
        });
    };

    auto add_indexed_attribute =
        [&](auto& mesh1, std::string_view name, auto&& values, auto&& indices) {
            using ValueType = typename std::decay_t<decltype(values)>::value_type;
            return mesh1.template create_attribute<ValueType>(
                name,
                Indexed,
                AttributeUsage::Scalar,
                1,
                values,
                indices);
        };

    SECTION("Default")
    {
        auto mesh = generate_square<Scalar, Index>();
        auto mesh2 = unify_index_buffer(mesh);
        REQUIRE(mesh2.get_num_vertices() == 4);
        mesh2.initialize_edges();
        check_for_consistency(mesh, mesh2);
    }

    SECTION("Consistent attribute")
    {
        auto mesh = generate_square<Scalar, Index>();
        std::vector<Scalar> values = {1};
        std::vector<Index> indices = {0, 0, 0, 0, 0, 0};
        auto attr_id = add_indexed_attribute(mesh, "test", values, indices);
        REQUIRE(mesh.has_attribute("test"));
        REQUIRE(mesh.is_attribute_indexed("test"));

        auto mesh2 = unify_index_buffer(mesh, {attr_id});
        REQUIRE(mesh2.get_num_vertices() == 4);
        mesh2.initialize_edges();
        check_for_consistency(mesh, mesh2);
    }

    SECTION("Facet index as attribute")
    {
        auto mesh = generate_square<Scalar, Index>();
        std::string name = "facet_id";
        std::vector<Scalar> values = {0, 1};
        std::vector<Index> indices = {0, 0, 0, 1, 1, 1};
        auto attr_id = add_indexed_attribute(mesh, name, values, indices);
        REQUIRE(mesh.has_attribute(name));
        REQUIRE(mesh.is_attribute_indexed(name));

        auto mesh2 = unify_index_buffer(mesh, {attr_id});
        REQUIRE(mesh2.get_num_vertices() == 6);
        mesh2.initialize_edges();
        check_for_consistency(mesh, mesh2);
    }

    SECTION("Combined")
    {
        auto mesh = generate_square<Scalar, Index>();
        std::vector<AttributeId> ids;

        {
            std::string name = "facet_id";
            std::vector<Scalar> values = {0, 1};
            std::vector<Index> indices = {0, 0, 0, 1, 1, 1};
            auto attr_id = add_indexed_attribute(mesh, name, values, indices);
            ids.push_back(attr_id);
        }
        {
            std::string name = "uniform";
            std::vector<Scalar> values = {0, 1};
            std::vector<Index> indices = {0, 0, 0, 0, 0, 0};
            auto attr_id = add_indexed_attribute(mesh, name, values, indices);
            ids.push_back(attr_id);
        }

        SECTION("Include Both")
        {
            auto mesh2 = unify_index_buffer(mesh, ids);
            REQUIRE(mesh2.get_num_vertices() == 6);
            mesh2.initialize_edges();
            REQUIRE(mesh2.has_attribute("facet_id"));
            REQUIRE(!mesh2.is_attribute_indexed("facet_id"));
            REQUIRE(mesh2.has_attribute("uniform"));
            REQUIRE(!mesh2.is_attribute_indexed("uniform"));
            check_for_consistency(mesh, mesh2);
        }

        SECTION("Include uniform only")
        {
            auto mesh2 = unify_named_index_buffer(mesh, {"uniform"});
            REQUIRE(mesh2.get_num_vertices() == 4);
            mesh2.initialize_edges();
            REQUIRE(mesh2.has_attribute("facet_id"));
            REQUIRE(mesh2.is_attribute_indexed("facet_id"));
            REQUIRE(mesh2.has_attribute("uniform"));
            REQUIRE(!mesh2.is_attribute_indexed("uniform"));
            check_for_consistency(mesh, mesh2);
        }

        SECTION("Include facet_id only")
        {
            auto mesh2 = unify_named_index_buffer(mesh, {"facet_id"});
            REQUIRE(mesh2.get_num_vertices() == 6);
            mesh2.initialize_edges();
            REQUIRE(mesh2.has_attribute("facet_id"));
            REQUIRE(!mesh2.is_attribute_indexed("facet_id"));
            REQUIRE(mesh2.has_attribute("uniform"));
            REQUIRE(mesh2.is_attribute_indexed("uniform"));
            check_for_consistency(mesh, mesh2);
        }
    }

    SECTION("hybrid")
    {
        lagrange::SurfaceMesh<Scalar, Index> mesh = generate_rectangle<Scalar, Index>();
        REQUIRE(mesh.get_num_facets() == 3);
        REQUIRE(mesh.get_facet_size(0) == 3);
        REQUIRE(mesh.get_facet_size(1) == 3);
        REQUIRE(mesh.get_facet_size(2) == 4);

        {
            std::vector<int> vertex_id(6);
            std::iota(vertex_id.begin(), vertex_id.end(), 0);
            mesh.create_attribute<int>("vertex_id", Vertex, AttributeUsage::Scalar, 1, vertex_id);
        }
        {
            std::vector<float> facet_normals = {0, 0, 1, 0, 0, 1, 0, 0, 1};
            mesh.create_attribute<float>(
                "facet_normals",
                Facet,
                AttributeUsage::Vector,
                3,
                facet_normals);
        }
        {
            std::vector<uint32_t> corner_ids = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
            mesh.create_attribute<uint32_t>(
                "corner_id",
                Corner,
                AttributeUsage::Scalar,
                1,
                corner_ids);
        }
        {
            std::vector<Scalar> values = {0, 1, 2};
            std::vector<Index> indices = {0, 0, 0, 1, 1, 1, 2, 2, 2, 2};
            add_indexed_attribute(mesh, "facet_id", values, indices);
        }
        {
            std::vector<Scalar> values = {0, 1};
            std::vector<Index> indices = {0, 0, 0, 0, 0, 0, 1, 1, 1, 1};
            add_indexed_attribute(mesh, "is_quad", values, indices);
        }
        {
            std::vector<Scalar> values = {0, 1};
            std::vector<Index> indices = {0, 0, 0, 1, 1, 1, 0, 0, 0, 0};
            add_indexed_attribute(mesh, "color", values, indices);
        }
        {
            std::vector<Scalar> values = {0, 1};
            std::vector<Index> indices = {1, 1, 1, 0, 0, 0, 0, 0, 0, 0};
            add_indexed_attribute(mesh, "color2", values, indices);
        }
        {
            std::vector<Scalar> values = {0, 1, 2, 3, 4, 5, 6, 7};
            std::vector<Index> indices = {0, 1, 2, 2, 3, 4, 2, 5, 6, 7};
            add_indexed_attribute(mesh, "corner_color", values, indices);
        }
        {
            std::vector<Scalar> values = {0, 1, 2, 3};
            std::vector<Index> indices = {0, 1, 2, 2, 3, 0, 2, 1, 3, 0};
            add_indexed_attribute(mesh, "corner_color2", values, indices);
        }
        std::vector<std::string_view> names =
            {"facet_id", "is_quad", "color", "color2", "corner_color"};

        SECTION("Both")
        {
            auto mesh2 = unify_named_index_buffer(mesh, names);
            mesh2.initialize_edges();
            REQUIRE(mesh2.get_num_vertices() == 10);
            check_for_consistency(mesh, mesh2);
        }

        SECTION("Include is_quad only")
        {
            auto mesh2 = unify_named_index_buffer(mesh, {"is_quad"});
            mesh2.initialize_edges();
            REQUIRE(mesh2.get_num_vertices() == 8);
            check_for_consistency(mesh, mesh2);
        }

        SECTION("Include facet_id only")
        {
            auto mesh2 = unify_named_index_buffer(mesh, {"facet_id"});
            mesh2.initialize_edges();
            REQUIRE(mesh2.get_num_vertices() == 10);
            check_for_consistency(mesh, mesh2);
        }

        SECTION("Include color only")
        {
            auto mesh2 = unify_named_index_buffer(mesh, {"color"});
            mesh2.initialize_edges();
            REQUIRE(mesh2.get_num_vertices() == 8);
            check_for_consistency(mesh, mesh2);
        }
        SECTION("Include color2 only")
        {
            auto mesh2 = unify_named_index_buffer(mesh, {"color2"});
            mesh2.initialize_edges();
            REQUIRE(mesh2.get_num_vertices() == 9);
            check_for_consistency(mesh, mesh2);
        }
        SECTION("Include corner_color only")
        {
            auto mesh2 = unify_named_index_buffer(mesh, {"corner_color"});
            mesh2.initialize_edges();
            REQUIRE(mesh2.get_num_vertices() == 8);
            check_for_consistency(mesh, mesh2);
        }
        SECTION("Include corner_color2 only")
        {
            auto mesh2 = unify_named_index_buffer(mesh, {"corner_color2"});
            mesh2.initialize_edges();
            REQUIRE(mesh2.get_num_vertices() == 6);
            check_for_consistency(mesh, mesh2);
        }
    }

    SECTION("Ensure all index attribute are unified") {
        std::unique_ptr<TriangleMesh3D> legacy = create_cube();
        SurfaceMesh32d mesh = to_surface_mesh_copy<double, uint32_t, TriangleMesh3D>(*legacy);
        REQUIRE(mesh.has_attribute("uv"));
        REQUIRE(mesh.is_attribute_indexed("uv"));

        SurfaceMesh32d uni = unify_index_buffer(mesh);
        REQUIRE(!uni.is_attribute_indexed("uv"));
        seq_foreach_attribute_read(uni, [&](auto&& attr) {
            using AttributeType = std::decay_t<decltype(attr)>;
            REQUIRE(!AttributeType::IsIndexed);
        });
    }
}
