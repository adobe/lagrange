/*
 * Copyright 2019 Adobe. All rights reserved.
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
#include <catch2/benchmark/catch_benchmark.hpp>

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
    #include <lagrange/create_mesh.h>
#endif

#include <lagrange/Logger.h>
#include <lagrange/common.h>
#include <lagrange/compute_facet_normal.h>
#include <lagrange/compute_vertex_normal.h>
#include <lagrange/extract_submesh.h>
#include <lagrange/mesh_convert.h>
#include <lagrange/testing/create_test_mesh.h>
#include <lagrange/utils/range.h>
#include <lagrange/views.h>

#include <array>

namespace {

template <typename Scalar, typename Index>
void validate_submesh(
    lagrange::SurfaceMesh<Scalar, Index>& mesh,
    lagrange::SurfaceMesh<Scalar, Index>& submesh,
    std::string_view vertex_map_attr_name,
    std::string_view facet_map_attr_name)
{
    auto vertex_mapping = lagrange::attribute_vector_view<Index>(submesh, vertex_map_attr_name);
    auto facet_mapping = lagrange::attribute_vector_view<Index>(submesh, facet_map_attr_name);

    const Index num_vertices = submesh.get_num_vertices();
    const Index num_facets = submesh.get_num_facets();

    for (Index i = 0; i < num_vertices; i++) {
        Index j = vertex_mapping[i];
        auto source = mesh.get_position(j);
        auto target = submesh.get_position(i);
        REQUIRE(std::equal(source.begin(), source.end(), target.begin()));
    }

    for (Index i = 0; i < num_facets; i++) {
        Index j = facet_mapping[i];
        Index s = submesh.get_facet_size(i);
        REQUIRE(s == mesh.get_facet_size(j));
        auto source = mesh.get_facet_vertices(j);
        auto target = submesh.get_facet_vertices(i);
        for (Index k = 0; k < s; k++) {
            REQUIRE(source[k] == vertex_mapping[target[k]]);
        }
    }

    lagrange::seq_foreach_named_attribute_read<lagrange::AttributeElement::Vertex>(
        mesh,
        [&](std::string_view name, auto&& attr) {
            if (mesh.attr_name_is_reserved(name)) return;
            lagrange::logger().info("Validating vertex attribute: {}", name);

            using AttributeType = std::decay_t<decltype(attr)>;
            using ValueType = typename AttributeType::ValueType;

            REQUIRE(submesh.has_attribute(name));
            REQUIRE(submesh.template is_attribute_type<ValueType>(name));

            auto source = lagrange::matrix_view(attr);
            auto target = lagrange::attribute_matrix_view<ValueType>(submesh, name);
            for (Index i = 0; i < num_vertices; i++) {
                Index j = vertex_mapping[i];
                REQUIRE(source.row(j) == target.row(i));
            }
        });

    lagrange::seq_foreach_named_attribute_read<lagrange::AttributeElement::Facet>(
        mesh,
        [&](std::string_view name, auto&& attr) {
            if (mesh.attr_name_is_reserved(name)) return;
            lagrange::logger().info("Validating facet attribute: {}", name);

            using AttributeType = std::decay_t<decltype(attr)>;
            using ValueType = typename AttributeType::ValueType;

            REQUIRE(submesh.has_attribute(name));
            REQUIRE(submesh.template is_attribute_type<ValueType>(name));

            auto source = lagrange::matrix_view(attr);
            auto target = lagrange::attribute_matrix_view<ValueType>(submesh, name);
            for (Index i = 0; i < num_facets; i++) {
                Index j = facet_mapping[i];
                REQUIRE(source.row(j) == target.row(i));
            }
        });

    lagrange::seq_foreach_named_attribute_read<lagrange::AttributeElement::Corner>(
        mesh,
        [&](std::string_view name, auto&& attr) {
            if (mesh.attr_name_is_reserved(name)) return;
            lagrange::logger().info("Validating corner attribute: {}", name);

            using AttributeType = std::decay_t<decltype(attr)>;
            using ValueType = typename AttributeType::ValueType;

            REQUIRE(submesh.has_attribute(name));
            REQUIRE(submesh.template is_attribute_type<ValueType>(name));

            auto source = lagrange::matrix_view(attr);
            auto target = lagrange::attribute_matrix_view<ValueType>(submesh, name);
            for (Index i = 0; i < num_facets; i++) {
                const Index s = submesh.get_facet_size(i);
                for (Index j = 0; j < s; j++) {
                    Index source_ci = submesh.get_facet_corner_begin(i) + j;
                    Index target_ci = submesh.get_facet_corner_begin(facet_mapping[i]) + j;
                    REQUIRE(source.row(source_ci) == target.row(target_ci));
                }
            }
        });

    lagrange::seq_foreach_named_attribute_read<lagrange::AttributeElement::Indexed>(
        mesh,
        [&](std::string_view name, auto&& attr) {
            if (mesh.attr_name_is_reserved(name)) return;
            lagrange::logger().info("Validating indexed attribute: {}", name);

            using AttributeType = std::decay_t<decltype(attr)>;
            using ValueType = typename AttributeType::ValueType;

            REQUIRE(submesh.has_attribute(name));
            REQUIRE(submesh.template is_attribute_type<ValueType>(name));

            auto source_values = lagrange::matrix_view(attr.values());
            auto source_indices = lagrange::matrix_view(attr.indices());
            const auto& target_attr = submesh.template get_indexed_attribute<ValueType>(name);
            auto target_values = lagrange::matrix_view(target_attr.values());
            auto target_indices = lagrange::matrix_view(target_attr.indices());
            for (Index i = 0; i < num_facets; i++) {
                const Index s = submesh.get_facet_size(i);
                for (Index j = 0; j < s; j++) {
                    Index source_ci = submesh.get_facet_corner_begin(i) + j;
                    Index target_ci = submesh.get_facet_corner_begin(facet_mapping[i]) + j;
                    Index source_idx = source_indices(source_ci);
                    Index target_idx = target_indices(target_ci);
                    REQUIRE(source_values.row(source_idx) == target_values.row(target_idx));
                }
            }
        });
}

} // namespace

TEST_CASE("extract_submesh", "[core][utilities][submesh]")
{
    using Scalar = float;
    using Index = uint32_t;
    auto mesh = lagrange::testing::create_test_cube<Scalar, Index>();
    lagrange::compute_facet_normal(mesh);
    lagrange::compute_vertex_normal(mesh);

    lagrange::SubmeshOptions options;
    options.source_vertex_attr_name = "@source_vertex";
    options.source_facet_attr_name = "@source_facet";
    options.map_attributes = true;

    SECTION("Empty selection")
    {
        auto submesh = lagrange::extract_submesh(mesh, {}, options);
        REQUIRE(submesh.get_num_vertices() == 0);
        REQUIRE(submesh.get_num_facets() == 0);
        validate_submesh(
            mesh,
            submesh,
            options.source_vertex_attr_name,
            options.source_facet_attr_name);
    }

    SECTION("Single face")
    {
        std::array<const Index, 1> selected_facets{0};
        auto submesh = lagrange::extract_submesh(
            mesh,
            {selected_facets.data(), selected_facets.size()},
            options);
        REQUIRE(submesh.get_num_vertices() == 3);
        REQUIRE(submesh.get_num_facets() == 1);
        validate_submesh(
            mesh,
            submesh,
            options.source_vertex_attr_name,
            options.source_facet_attr_name);
    }

    SECTION("All faces")
    {
        std::array<const Index, 12> selected_facets{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
        auto submesh = lagrange::extract_submesh(
            mesh,
            {selected_facets.data(), selected_facets.size()},
            options);
        REQUIRE(submesh.get_num_vertices() == 8);
        REQUIRE(submesh.get_num_facets() == 12);
        validate_submesh(
            mesh,
            submesh,
            options.source_vertex_attr_name,
            options.source_facet_attr_name);
    }
}

TEST_CASE("extract_submesh benchmark", "[core][utilities][submesh][!benchmark]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/dragon.obj");
    lagrange::SubmeshOptions options;
    options.map_attributes = false;

    std::vector<Index> selected_facets(mesh.get_num_facets() / 2);
    std::iota(selected_facets.begin(), selected_facets.end(), 0);

    BENCHMARK("extract_submesh")
    {
        return lagrange::extract_submesh(
            mesh,
            {selected_facets.data(), selected_facets.size()},
            options);
    };

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
    using VertexArray = Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;
    using FacetArray = Eigen::Matrix<Index, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;
    using MeshType = Mesh<VertexArray, FacetArray>;
    auto legacy_mesh = to_legacy_mesh<MeshType>(mesh);
    std::vector<typename MeshType::Index> vertex_mapping, facet_mapping;

    BENCHMARK("legacy::extract_submesh")
    {
        return lagrange::legacy::extract_submesh(
            *legacy_mesh,
            selected_facets,
            &vertex_mapping,
            &facet_mapping);
    };
#endif
}

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
template <typename MeshType, typename Index>
void ensure_valid_mapping(
    const MeshType& original,
    const MeshType& submesh,
    const std::vector<Index>* vertex_mapping = nullptr,
    const std::vector<Index>* facet_mapping = nullptr)
{
    using namespace lagrange;
    using MeshIndex = typename MeshType::Index;

    // using 's' for submesh and 'o' for original
    const auto& Vs = submesh.get_vertices();
    const auto& Vo = original.get_vertices();
    const auto& Fs = submesh.get_facets();
    const auto& Fo = original.get_facets();

    REQUIRE(original.get_vertex_per_facet() == submesh.get_vertex_per_facet());
    const auto& vertex_per_facet = original.get_vertex_per_facet();

    if (vertex_mapping) {
        REQUIRE(safe_cast<MeshIndex>(vertex_mapping->size()) == submesh.get_num_vertices());
        for (auto s_v_i : lagrange::range(submesh.get_num_vertices())) {
            const auto o_v_i = (*vertex_mapping)[s_v_i];
            const auto& v1 = Vs.row(s_v_i);
            const auto& v2 = Vo.row(o_v_i);
            REQUIRE(v1 == v2);
        }
    }

    if (facet_mapping) {
        REQUIRE(safe_cast<MeshIndex>(facet_mapping->size()) == submesh.get_num_facets());
        for (auto s_f_i : lagrange::range(submesh.get_num_facets())) {
            const auto o_f_i = (*facet_mapping)[s_f_i];

            typename MeshType::VertexType c1 = Vs.row(Fs(s_f_i, 0));
            typename MeshType::VertexType c2 = Vo.row(Fo(o_f_i, 0));
            for (auto j : lagrange::range(1, vertex_per_facet)) {
                // note: we cannot rely on the order of vertices
                // sum the vertex positions instead.
                const auto v1 = Fs(s_f_i, j);
                const auto v2 = Fo(o_f_i, j);
                c1 += Vs.row(v1);
                c2 += Vo.row(v2);
            }
            REQUIRE(c1 == c2);
        }
    }
}

TEST_CASE("legacy: extract_components", "[submesh][mesh]")
{
    using namespace lagrange;
    Vertices3D vertices(8, 3);
    vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0,

        0.0, 0.0, 1.0, 1.0, 0.0, 1.0, 0.0, 1.0, 1.0, 1.0, 1.0, 1.0;
    Triangles facets(4, 3);
    facets << 0, 1, 2, 2, 1, 3,

        4, 5, 6, 6, 5, 7;
    auto mesh = wrap_with_mesh(vertices, facets);

    SECTION("simple query")
    {
        auto components = extract_component_submeshes(*mesh);
        REQUIRE(components.size() == 2);
        for (const auto& component : components) {
            REQUIRE(component->get_num_facets() == 2);
            REQUIRE(component->get_num_vertices() == 4);
        }
    }

    SECTION("with mapping")
    {
        std::vector<std::vector<int>> vertex_mapping;
        std::vector<std::vector<int>> facet_mapping;
        auto components = extract_component_submeshes(*mesh, &vertex_mapping, &facet_mapping);
        REQUIRE(components.size() == 2);
        for (auto i : range(components.size())) {
            REQUIRE(components[i]->get_num_facets() == 2);
            REQUIRE(components[i]->get_num_vertices() == 4);
            ensure_valid_mapping(*mesh, *components[i], &vertex_mapping[i], &facet_mapping[i]);
        }
    }
}

TEST_CASE("legacy: extract_submesh", "[submesh][mesh]")
{
    using namespace lagrange;

    Vertices3D vertices(4, 3);
    vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0;
    Triangles facets(2, 3);
    facets << 0, 1, 2, 2, 1, 3;

    auto mesh = wrap_with_mesh(vertices, facets);

    {
        // some garbage data so we crash if they don't get set properly
        std::vector<int> vertex_mapping = {-1, -1, -1, -1};
        std::vector<int> facet_mapping = {-1, -1, -1, -1};

        SECTION("Empty selection")
        {
            std::vector<int> selected_facets = {};
            auto submesh = extract_submesh(*mesh, selected_facets, &vertex_mapping, &facet_mapping);
            REQUIRE(submesh->get_num_vertices() == 0);
            REQUIRE(submesh->get_num_facets() == 0);
            ensure_valid_mapping(*mesh, *submesh, &vertex_mapping, &facet_mapping);
        }

        SECTION("Select 1 face")
        {
            std::vector<int> selected_facets = {1};
            auto submesh = extract_submesh(*mesh, selected_facets, &vertex_mapping, &facet_mapping);
            REQUIRE(submesh->get_num_vertices() == 3);
            REQUIRE(submesh->get_num_facets() == 1);
            ensure_valid_mapping(*mesh, *submesh, &vertex_mapping, &facet_mapping);
        }

        SECTION("Select all faces")
        {
            std::vector<int> selected_facets = {1, 0};
            auto submesh = extract_submesh(*mesh, selected_facets, &vertex_mapping, &facet_mapping);
            REQUIRE(submesh->get_num_vertices() == 4);
            REQUIRE(submesh->get_num_facets() == 2);
            ensure_valid_mapping(*mesh, *submesh, &vertex_mapping, &facet_mapping);
        }

        SECTION("Select invalid faces")
        {
            std::vector<int> selected_facets = {3, 0};
            LA_REQUIRE_THROWS(
                extract_submesh(*mesh, selected_facets, &vertex_mapping, &facet_mapping));
        }

        SECTION("Only vertex mapping")
        {
            std::vector<int> selected_facets = {1};
            auto submesh = extract_submesh(*mesh, selected_facets, &vertex_mapping);
            REQUIRE(submesh->get_num_vertices() == 3);
            REQUIRE(submesh->get_num_facets() == 1);
            ensure_valid_mapping(*mesh, *submesh, &vertex_mapping);
        }

        SECTION("Only facet mapping")
        {
            std::vector<int> selected_facets = {1};

            // unfortunately we have to explicitly write the template instantiation
            // if we pass nullptr as one of the parameters.
            auto submesh = extract_submesh<decltype(mesh)::element_type, int>(
                *mesh,
                selected_facets,
                nullptr,
                &facet_mapping);

            REQUIRE(submesh->get_num_vertices() == 3);
            REQUIRE(submesh->get_num_facets() == 1);
            ensure_valid_mapping<decltype(mesh)::element_type, int>(
                *mesh,
                *submesh,
                nullptr,
                &facet_mapping);
        }
    }

    SECTION("Mismatch index types")
    {
        std::vector<size_t> selected_facets = {0};
        std::vector<size_t> vertex_mapping;
        std::vector<size_t> facet_mapping;
        auto submesh = extract_submesh(*mesh, selected_facets, &vertex_mapping, &facet_mapping);
        REQUIRE(submesh->get_num_vertices() == 3);
        REQUIRE(submesh->get_num_facets() == 1);
        ensure_valid_mapping(*mesh, *submesh, &vertex_mapping, &facet_mapping);
    }

    SECTION("Multiple submeshes")
    {
        std::vector<std::vector<int>> facet_groups = {{0}, {1}};
        std::vector<std::vector<int>> vertex_mapping;
        std::vector<std::vector<int>> facet_mapping;
        auto submeshes = extract_submeshes(*mesh, facet_groups, &vertex_mapping, &facet_mapping);

        REQUIRE(submeshes.size() == facet_groups.size());
        for (auto i : lagrange::range(submeshes.size())) {
            ensure_valid_mapping(*mesh, *(submeshes[i]), &vertex_mapping[i], &facet_mapping[i]);
        }
    }

    SECTION("Multiple intersecting submeshes")
    {
        std::vector<std::vector<int>> facet_groups = {{0, 1}, {1}};
        std::vector<std::vector<int>> vertex_mapping;
        std::vector<std::vector<int>> facet_mapping;
        auto submeshes = extract_submeshes(*mesh, facet_groups, &vertex_mapping, &facet_mapping);

        REQUIRE(submeshes.size() == facet_groups.size());
        for (auto i : lagrange::range(submeshes.size())) {
            ensure_valid_mapping(*mesh, *(submeshes[i]), &vertex_mapping[i], &facet_mapping[i]);
        }
    }

    SECTION("Multiple submeshes, one mapping")
    {
        std::vector<std::vector<int>> facet_groups = {{0}, {1}};
        std::vector<std::vector<int>> vertex_mapping;
        auto submeshes = extract_submeshes(*mesh, facet_groups, &vertex_mapping);

        REQUIRE(submeshes.size() == facet_groups.size());
        for (auto i : lagrange::range(submeshes.size())) {
            ensure_valid_mapping(*mesh, *(submeshes[i]), &vertex_mapping[i]);
        }
    }
}
#endif
