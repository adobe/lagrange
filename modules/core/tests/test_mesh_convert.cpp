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
#include <lagrange/attributes/rename_attribute.h>
#include <lagrange/common.h>
#include <lagrange/mesh_convert.h>


// clang-format off
#include <lagrange/utils/warnoff.h>
#include <catch2/catch_test_macros.hpp>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <random>
#include <regex>

// clang-format off
#define LA_LEGACY_MESH_X(mode, data) \
    LA_X_##mode(data, lagrange::TriangleMesh3D) \
    LA_X_##mode(data, lagrange::TriangleMesh2D) \
    LA_X_##mode(data, lagrange::TriangleMesh3Df) \
    LA_X_##mode(data, lagrange::TriangleMesh2Df) \
    LA_X_##mode(data, TriangleMesh3D32) \
    LA_X_##mode(data, TriangleMesh3D64) \
    LA_X_##mode(data, lagrange::QuadMesh3D) \
    LA_X_##mode(data, lagrange::QuadMesh2D) \
    LA_X_##mode(data, lagrange::QuadMesh3Df) \
    LA_X_##mode(data, lagrange::QuadMesh2Df) \
    LA_X_##mode(data, QuadMesh3D32) \
    LA_X_##mode(data, QuadMesh3D64)
// clang-format on

namespace {

using Triangles32 = Eigen::Matrix<uint32_t, Eigen::Dynamic, 3, Eigen::RowMajor>;
using Quads32 = Eigen::Matrix<uint32_t, Eigen::Dynamic, 4, Eigen::RowMajor>;
using Triangles64 = Eigen::Matrix<uint64_t, Eigen::Dynamic, 3, Eigen::RowMajor>;
using Quads64 = Eigen::Matrix<uint64_t, Eigen::Dynamic, 4, Eigen::RowMajor>;

using TriangleMesh3D32 = lagrange::Mesh<lagrange::Vertices3D, Triangles32>;
using QuadMesh3D32 = lagrange::Mesh<lagrange::Vertices3D, Quads32>;
using TriangleMesh3D64 = lagrange::Mesh<lagrange::Vertices3D, Triangles64>;
using QuadMesh3D64 = lagrange::Mesh<lagrange::Vertices3D, Quads64>;

namespace detail {

template <typename From, typename To, typename = void>
struct is_narrowing_conversion_impl : std::true_type
{
};

template <typename From, typename To>
struct is_narrowing_conversion_impl<From, To, std::void_t<decltype(To{std::declval<From>()})>>
    : std::false_type
{
};

} // namespace detail

template <typename From, typename To>
struct is_narrowing_conversion : detail::is_narrowing_conversion_impl<From, To>
{
};

template <typename From, typename To>
inline constexpr bool is_narrowing_conversion_v = is_narrowing_conversion<From, To>::value;


template <typename MatrixType>
int deduce_num_cols()
{
    int dim = MatrixType::ColsAtCompileTime;
    if (dim == Eigen::Dynamic) {
        return 3;
    }
    return dim;
}

template <typename MeshType>
std::unique_ptr<MeshType> create_legacy_mesh(bool with_edges, bool duplicate_names)
{
    using Scalar = typename MeshType::Scalar;
    using Index = typename MeshType::Index;
    using VertexArray = typename MeshType::VertexArray;
    using FacetArray = typename MeshType::FacetArray;
    using AttributeArray = typename MeshType::AttributeArray;
    using IndexArray = typename MeshType::IndexArray;

    const int num_vertices = 8;
    const int num_facets = 12;
    const int num_uv_vertices = 14;

    const int dim = deduce_num_cols<VertexArray>();
    const int nvpf = deduce_num_cols<FacetArray>();

    std::mt19937 gen;

    // Create mesh
    auto mesh = [&] {
        VertexArray vertices(num_vertices, dim);
        std::uniform_real_distribution<Scalar> dist_values(-1, 1);
        vertices = vertices.unaryExpr([&](auto) { return dist_values(gen); });

        FacetArray facets(num_facets, nvpf);
        std::uniform_int_distribution<Index> dist_indices(0, num_vertices - 1);
        facets = facets.unaryExpr([&](auto) { return dist_indices(gen); });
        return lagrange::create_mesh(std::move(vertices), std::move(facets));
    }();

    // Create indexed attribute (uvs)
    {
        AttributeArray uv_values(num_uv_vertices, 2);
        std::uniform_real_distribution<Scalar> dist_values(-1, 1);
        uv_values = uv_values.unaryExpr([&](auto) { return dist_values(gen); });

        IndexArray uv_indices(num_facets, nvpf);
        std::uniform_int_distribution<Index> dist_indices(0, num_uv_vertices - 1);
        uv_indices = uv_indices.unaryExpr([&](auto) { return dist_indices(gen); });

        std::string name = (duplicate_names ? "vector" : "uv");
        mesh->add_indexed_attribute(name);
        mesh->import_indexed_attribute(name, std::move(uv_values), std::move(uv_indices));
    }

    // Create vertex attribute (normals)
    {
        AttributeArray normals(num_vertices, 3);
        std::uniform_real_distribution<Scalar> dist_values(-1, 1);
        normals = normals.unaryExpr([&](auto) { return dist_values(gen); });
        std::string name = (duplicate_names ? "vector" : "normal");
        mesh->add_vertex_attribute(name);
        mesh->import_vertex_attribute(name, std::move(normals));
    }

    // Create facet attribute (color)
    {
        AttributeArray colors(num_facets, 3);
        std::uniform_real_distribution<Scalar> dist_values(0, 1);
        colors = colors.unaryExpr([&](auto) { return dist_values(gen); });
        std::string name = (duplicate_names ? "vector" : "color");
        mesh->add_facet_attribute(name);
        mesh->import_facet_attribute(name, std::move(colors));
    }

    // Create corner attribute (vector)
    {
        AttributeArray vector(num_facets * nvpf, 5);
        std::uniform_real_distribution<Scalar> dist_values(-1, 1);
        vector = vector.unaryExpr([&](auto) { return dist_values(gen); });
        std::string name = (duplicate_names ? "vector" : "vector");
        mesh->add_corner_attribute(name);
        mesh->import_corner_attribute(name, std::move(vector));
    }

    // Create edge attribute (length)
    if (with_edges) {
        mesh->initialize_edge_data();
        AttributeArray lengths(mesh->get_num_edges(), 1);
        std::uniform_real_distribution<Scalar> dist_values(0.1, 1);
        lengths = lengths.unaryExpr([&](auto) { return dist_values(gen); });
        std::string name = (duplicate_names ? "vector" : "length");
        mesh->add_edge_attribute(name);
        mesh->import_edge_attribute(name, std::move(lengths));
    }

    return mesh;
}

template <typename Scalar, typename Index>
lagrange::SurfaceMesh<Scalar, Index> create_surface_mesh(int dim, int nvpf, bool with_edges)
{
    using namespace lagrange;

    const int num_vertices = 8;
    const int num_facets = 12;
    const int num_uv_vertices = 14;
    const int num_values = 42;

    // TODO: Create attr using all numerical types

    SurfaceMesh<Scalar, Index> mesh(dim);

    std::mt19937 gen;

    // Create vertices and facets
    {
        std::uniform_real_distribution<Scalar> dist_values(-1, 1);
        std::uniform_int_distribution<Index> dist_indices(0, num_vertices - 1);

        mesh.add_vertices(num_vertices, [&](Index, lagrange::span<Scalar> p) {
            for (auto& x : p) {
                x = dist_values(gen);
            }
        });
        mesh.add_polygons(num_facets, nvpf, [&](Index, lagrange::span<Index> t) {
            for (auto& v : t) {
                v = dist_indices(gen);
            }
        });
    }

    // Create indexed attribute (uvs)
    {
        auto id = mesh.template create_attribute<Scalar>(
            "uv",
            AttributeElement::Indexed,
            AttributeUsage::UV,
            2);
        auto& attr = mesh.template ref_indexed_attribute<Scalar>(id);
        attr.values().resize_elements(num_uv_vertices);

        std::uniform_real_distribution<Scalar> dist_values(-1, 1);
        std::uniform_int_distribution<Index> dist_indices(0, num_uv_vertices - 1);

        for (auto& x : attr.values().ref_all()) {
            x = dist_values(gen);
        }
        for (auto& v : attr.indices().ref_all()) {
            v = dist_indices(gen);
        }
    }

    // Create vertex attribute (normals)
    {
        auto id = mesh.template create_attribute<Scalar>(
            "normal",
            AttributeElement::Vertex,
            AttributeUsage::Normal,
            3);
        std::uniform_real_distribution<Scalar> dist_values(-1, 1);
        for (auto& x : mesh.template ref_attribute<Scalar>(id).ref_all()) {
            x = dist_values(gen);
        }
    }

    // Create facet attribute (color)
    {
        auto id = mesh.template create_attribute<Scalar>(
            "color",
            AttributeElement::Facet,
            AttributeUsage::Color,
            3);
        std::uniform_real_distribution<Scalar> dist_values(0, 1);
        for (auto& x : mesh.template ref_attribute<Scalar>(id).ref_all()) {
            x = dist_values(gen);
        }
    }

    // Create corner attribute (vector)
    {
        auto id = mesh.template create_attribute<Scalar>(
            "vector",
            AttributeElement::Corner,
            AttributeUsage::Vector,
            5);
        std::uniform_real_distribution<Scalar> dist_values(-1, 1);
        for (auto& x : mesh.template ref_attribute<Scalar>(id).ref_all()) {
            x = dist_values(gen);
        }
    }

    // Create edge attribute (length)
    if (with_edges) {
        mesh.initialize_edges();
        auto id = mesh.template create_attribute<Scalar>(
            "length",
            AttributeElement::Edge,
            AttributeUsage::Scalar,
            1);
        std::uniform_real_distribution<Scalar> dist_values(0.1, 1);
        for (auto& x : mesh.template ref_attribute<Scalar>(id).ref_all()) {
            x = dist_values(gen);
        }
    }

    // Create value attribute (value)
    {
        auto id = mesh.template create_attribute<Scalar>(
            "value",
            AttributeElement::Value,
            AttributeUsage::Scalar);
        auto& attr = mesh.template ref_attribute<Scalar>(id);
        attr.resize_elements(num_values);
        std::uniform_real_distribution<Scalar> dist_values(-1, 1);
        for (auto& x : attr.ref_all()) {
            x = dist_values(gen);
        }
    }

    return mesh;
}

template <typename MeshType>
std::map<std::string, std::string> rename_attributes_with_suffix(MeshType& mesh)
{
    static_assert(lagrange::MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");
    std::regex re(R"((.*)(\.\d+)$)");
    std::map<std::string, std::string> map;
    for (const auto& name : mesh.get_vertex_attribute_names()) {
        std::string new_name = std::regex_replace(name, re, "$1");
        rename_vertex_attribute(mesh, name, new_name);
        map[name] = new_name;
    }
    for (const auto& name : mesh.get_facet_attribute_names()) {
        std::string new_name = std::regex_replace(name, re, "$1");
        rename_facet_attribute(mesh, name, new_name);
        map[name] = new_name;
    }
    for (const auto& name : mesh.get_corner_attribute_names()) {
        std::string new_name = std::regex_replace(name, re, "$1");
        rename_corner_attribute(mesh, name, new_name);
        map[name] = new_name;
    }
    for (const auto& name : mesh.get_edge_attribute_names()) {
        std::string new_name = std::regex_replace(name, re, "$1");
        rename_edge_attribute(mesh, name, new_name);
        map[name] = new_name;
    }
    for (const auto& name : mesh.get_indexed_attribute_names()) {
        std::string new_name = std::regex_replace(name, re, "$1");
        rename_indexed_attribute(mesh, name, new_name);
        map[name] = new_name;
    }
    return map;
}

template <typename Scalar, typename MeshType>
void assert_same_legacy_mesh(const MeshType& old_mesh, const MeshType& new_mesh)
{
    static_assert(lagrange::MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");
    REQUIRE(
        old_mesh.get_vertices().template cast<Scalar>() ==
        new_mesh.get_vertices().template cast<Scalar>());
    REQUIRE(old_mesh.get_facets() == new_mesh.get_facets());
    REQUIRE(old_mesh.get_vertex_attribute_names() == new_mesh.get_vertex_attribute_names());
    REQUIRE(old_mesh.get_facet_attribute_names() == new_mesh.get_facet_attribute_names());
    REQUIRE(old_mesh.get_corner_attribute_names() == new_mesh.get_corner_attribute_names());
    REQUIRE(old_mesh.get_edge_attribute_names() == new_mesh.get_edge_attribute_names());
    REQUIRE(old_mesh.get_indexed_attribute_names() == new_mesh.get_indexed_attribute_names());
    for (const auto& name : old_mesh.get_vertex_attribute_names()) {
        REQUIRE(
            old_mesh.get_vertex_attribute(name).template cast<Scalar>() ==
            new_mesh.get_vertex_attribute(name).template cast<Scalar>());
    }
    for (const auto& name : old_mesh.get_facet_attribute_names()) {
        REQUIRE(
            old_mesh.get_facet_attribute(name).template cast<Scalar>() ==
            new_mesh.get_facet_attribute(name).template cast<Scalar>());
    }
    for (const auto& name : old_mesh.get_corner_attribute_names()) {
        REQUIRE(
            old_mesh.get_corner_attribute(name).template cast<Scalar>() ==
            new_mesh.get_corner_attribute(name).template cast<Scalar>());
    }
    for (const auto& name : old_mesh.get_edge_attribute_names()) {
        REQUIRE(
            old_mesh.get_edge_attribute(name).template cast<Scalar>() ==
            new_mesh.get_edge_attribute(name).template cast<Scalar>());
    }
    for (const auto& name : old_mesh.get_indexed_attribute_names()) {
        const auto& [val1, ind1] = old_mesh.get_indexed_attribute(name);
        const auto& [val2, ind2] = new_mesh.get_indexed_attribute(name);
        REQUIRE(val1.template cast<Scalar>() == val2.template cast<Scalar>());
        REQUIRE(ind1 == ind2);
    }
}

template <typename NarrowScalar, typename ScalarL, typename ScalarR>
void assert_same_narrow(lagrange::span<const ScalarL> l, lagrange::span<const ScalarR> r)
{
    REQUIRE(l.size() == r.size());
    for (size_t i = 0; i < l.size(); ++i) {
        REQUIRE(static_cast<NarrowScalar>(l[i]) == static_cast<NarrowScalar>(r[i]));
    }
}

template <typename Scalar>
void assert_same_strict(lagrange::span<const Scalar> l, lagrange::span<const Scalar> r)
{
    REQUIRE(l.size() == r.size());
    REQUIRE(std::equal(l.begin(), l.end(), r.begin()));
}

template <typename NarrowScalar, typename MeshScalar, typename Scalar, typename Index>
void assert_same_surface_mesh(
    const lagrange::SurfaceMesh<Scalar, Index>& old_mesh,
    const lagrange::SurfaceMesh<Scalar, Index>& new_mesh)
{
    // Non-reserved attributes in the "new_mesh" will have kept the scalar type from the
    // intermediary "lagrange::Mesh<>", which is why we are comparing attributes with different
    // types.
    seq_foreach_named_attribute_read(old_mesh, [&](std::string_view name, auto&& attr1) {
        using AttributeType = std::decay_t<decltype(attr1)>;
        auto func = [&](auto x) {
            using OtherType = std::decay_t<decltype(x)>;
            if (attr1.get_element_type() == lagrange::AttributeElement::Value) {
                // Conversion will drop value attributes
                REQUIRE(!new_mesh.has_attribute(name));
                return;
            } else {
                REQUIRE(new_mesh.has_attribute(name));
            }
            la_runtime_assert(new_mesh.template is_attribute_type<OtherType>(name));
            if constexpr (AttributeType::IsIndexed) {
                REQUIRE(new_mesh.is_attribute_indexed(name));
                auto& attr2 = new_mesh.template get_indexed_attribute<OtherType>(name);
                assert_same_strict(attr1.indices().get_all(), attr2.indices().get_all());
                assert_same_narrow<NarrowScalar>(
                    attr1.values().get_all(),
                    attr2.values().get_all());
            } else {
                REQUIRE(!new_mesh.is_attribute_indexed(name));
                auto& attr2 = new_mesh.template get_attribute<OtherType>(name);
                assert_same_narrow<NarrowScalar>(attr1.get_all(), attr2.get_all());
            }
        };
        using ValueType = typename AttributeType::ValueType;
        if (old_mesh.attr_name_is_reserved(name)) {
            func(ValueType(0));
        } else {
            func(MeshScalar(0));
        }
    });
    seq_foreach_named_attribute_read(new_mesh, [&](std::string_view name, auto&&) {
        REQUIRE(old_mesh.has_attribute(name));
    });
}

template <typename Scalar, typename Index, typename MeshType>
void assert_same_attribute_buffers(
    const MeshType& old_mesh,
    const lagrange::SurfaceMesh<Scalar, Index>& new_mesh,
    const std::map<std::string, std::string>& map = {})
{
    seq_foreach_named_attribute_read(new_mesh, [&](std::string_view new_name, auto&& attr) {
        using AttributeType = std::decay_t<decltype(attr)>;
        using ValueType = typename AttributeType::ValueType;
        if (new_mesh.attr_name_is_reserved(new_name)) {
            return;
        }
        std::string old_name(new_name);
        if (!map.empty()) {
            lagrange::logger().info("map: {}", new_name);
            old_name = map.at(std::string(new_name));
        }
        if constexpr (std::is_same_v<Scalar, ValueType>) {
            if constexpr (AttributeType::IsIndexed) {
                REQUIRE(attr.values().is_external());
                REQUIRE(attr.indices().is_external());
                const auto& [val, ind] = old_mesh.get_indexed_attribute(old_name);
                REQUIRE(val.data() == attr.values().get_all().data());
                REQUIRE(ind.data() == attr.indices().get_all().data());
            } else {
                REQUIRE(attr.is_external());
                const Scalar* old_ptr = nullptr;
                switch (attr.get_element_type()) {
                case lagrange::AttributeElement::Vertex:
                    old_ptr = old_mesh.get_vertex_attribute(old_name).data();
                    break;
                case lagrange::AttributeElement::Facet:
                    old_ptr = old_mesh.get_facet_attribute(old_name).data();
                    break;
                case lagrange::AttributeElement::Corner:
                    old_ptr = old_mesh.get_corner_attribute(old_name).data();
                    break;
                case lagrange::AttributeElement::Edge:
                    old_ptr = old_mesh.get_edge_attribute(old_name).data();
                    break;
                case lagrange::AttributeElement::Value:
                case lagrange::AttributeElement::Indexed:
                default: la_runtime_assert(false);
                }
                REQUIRE(old_ptr == attr.get_all().data());
            }
        } else {
            REQUIRE(false);
        }
    });
}

template <typename Scalar, typename Index, typename MeshType>
void test_to_surface_mesh()
{
    using MeshScalar = typename MeshType::Scalar;
    using MeshIndex = typename MeshType::Index;

    // Choose either MeshScalar or Scalar, whichever is narrower!
    using NarrowScalar =
        std::conditional_t<is_narrowing_conversion_v<MeshScalar, Scalar>, Scalar, MeshScalar>;

    const bool with_edges = true;
    const bool duplicate_names = false;
    auto mesh_ptr = create_legacy_mesh<MeshType>(with_edges, duplicate_names);
    MeshType& mesh = *mesh_ptr;
    const MeshType& cmesh = *mesh_ptr;
    {
        auto res = lagrange::to_surface_mesh_copy<Scalar, Index>(mesh);
        auto bak = lagrange::to_legacy_mesh<MeshType>(res);
        assert_same_legacy_mesh<NarrowScalar>(mesh, *bak);
    }
    {
        auto res = lagrange::to_surface_mesh_copy<Scalar, Index>(cmesh);
        auto bak = lagrange::to_legacy_mesh<MeshType>(res);
        assert_same_legacy_mesh<NarrowScalar>(mesh, *bak);
    }

    if constexpr (std::is_same_v<Scalar, MeshScalar> && std::is_same_v<Index, MeshIndex>) {
        lagrange::logger().info("to_surface_mesh_wrap: {}", std::is_const_v<const MeshType>);
        {
            auto res = lagrange::to_surface_mesh_wrap<Scalar, Index>(mesh);
            auto bak = lagrange::to_legacy_mesh<MeshType>(res);
            assert_same_legacy_mesh<NarrowScalar>(mesh, *bak);
            assert_same_attribute_buffers(mesh, res);
        }
        {
            auto res = lagrange::to_surface_mesh_wrap<Scalar, Index>(cmesh);
            auto bak = lagrange::to_legacy_mesh<MeshType>(res);
            assert_same_legacy_mesh<NarrowScalar>(mesh, *bak);
            assert_same_attribute_buffers(mesh, res);
        }

        // The following should also NOT compile
        // if constexpr (std::is_same_v<Scalar, double>) {
        //     lagrange::Mesh<Eigen::MatrixXd, Eigen::Matrix<Index, Eigen::Dynamic, 3,
        //     Eigen::RowMajor>> mesh2; auto res6 = lagrange::to_surface_mesh_wrap<Scalar,
        //     Index>(mesh2);
        // }
    }

    // This one should NOT compile
    // TODO: Write custom testing script to test failed compilation scenario...
    // auto res6 = lagrange::to_surface_mesh_wrap<Scalar, Index>(MeshType());
}

template <typename Scalar, typename Index, typename MeshType>
void test_to_surface_mesh_empty()
{
    using VertexArray = typename MeshType::VertexArray;
    using FacetArray = typename MeshType::FacetArray;
    using MeshScalar = typename MeshType::Scalar;
    using MeshIndex = typename MeshType::Index;

    // Choose either MeshScalar or Scalar, whichever is narrower!
    using NarrowScalar =
        std::conditional_t<is_narrowing_conversion_v<MeshScalar, Scalar>, Scalar, MeshScalar>;

    auto mesh_ptr = lagrange::create_empty_mesh<VertexArray, FacetArray>();
    MeshType& mesh = *mesh_ptr;
    const MeshType& cmesh = *mesh_ptr;
    {
        auto res = lagrange::to_surface_mesh_copy<Scalar, Index>(mesh);
        auto bak = lagrange::to_legacy_mesh<MeshType>(res);
        assert_same_legacy_mesh<NarrowScalar>(mesh, *bak);
    }
    {
        auto res = lagrange::to_surface_mesh_copy<Scalar, Index>(cmesh);
        auto bak = lagrange::to_legacy_mesh<MeshType>(res);
        assert_same_legacy_mesh<NarrowScalar>(mesh, *bak);
    }

    if constexpr (std::is_same_v<Scalar, MeshScalar> && std::is_same_v<Index, MeshIndex>) {
        lagrange::logger().info("to_surface_mesh_wrap: {}", std::is_const_v<const MeshType>);
        {
            auto res = lagrange::to_surface_mesh_wrap<Scalar, Index>(mesh);
            auto bak = lagrange::to_legacy_mesh<MeshType>(res);
            assert_same_legacy_mesh<NarrowScalar>(mesh, *bak);
            assert_same_attribute_buffers(mesh, res);
        }
        {
            auto res = lagrange::to_surface_mesh_wrap<Scalar, Index>(cmesh);
            auto bak = lagrange::to_legacy_mesh<MeshType>(res);
            assert_same_legacy_mesh<NarrowScalar>(mesh, *bak);
            assert_same_attribute_buffers(mesh, res);
        }

        // The following should also NOT compile
        // if constexpr (std::is_same_v<Scalar, double>) {
        //     lagrange::Mesh<Eigen::MatrixXd, Eigen::Matrix<Index, Eigen::Dynamic, 3,
        //     Eigen::RowMajor>> mesh2; auto res6 = lagrange::to_surface_mesh_wrap<Scalar,
        //     Index>(mesh2);
        // }
    }

    // This one should NOT compile
    // TODO: Write custom testing script to test failed compilation scenario...
    // auto res6 = lagrange::to_surface_mesh_wrap<Scalar, Index>(MeshType());
}

template <typename Scalar, typename Index, typename MeshType>
void test_to_surface_duplicate()
{
    using MeshScalar = typename MeshType::Scalar;
    using MeshIndex = typename MeshType::Index;

    // Choose either MeshScalar or Scalar, whichever is narrower!
    using NarrowScalar =
        std::conditional_t<is_narrowing_conversion_v<MeshScalar, Scalar>, Scalar, MeshScalar>;

    const bool with_edges = true;
    const bool duplicate_names = true;
    auto mesh_ptr = create_legacy_mesh<MeshType>(with_edges, duplicate_names);
    MeshType& mesh = *mesh_ptr;
    const MeshType& cmesh = *mesh_ptr;
    {
        auto res = lagrange::to_surface_mesh_copy<Scalar, Index>(mesh);
        auto bak = lagrange::to_legacy_mesh<MeshType>(res);
        rename_attributes_with_suffix(*bak);
        assert_same_legacy_mesh<NarrowScalar>(mesh, *bak);
    }
    {
        auto res = lagrange::to_surface_mesh_copy<Scalar, Index>(cmesh);
        auto bak = lagrange::to_legacy_mesh<MeshType>(res);
        rename_attributes_with_suffix(*bak);
        assert_same_legacy_mesh<NarrowScalar>(mesh, *bak);
    }

    if constexpr (std::is_same_v<Scalar, MeshScalar> && std::is_same_v<Index, MeshIndex>) {
        lagrange::logger().info("to_surface_mesh_wrap: {}", std::is_const_v<const MeshType>);
        {
            auto res = lagrange::to_surface_mesh_wrap<Scalar, Index>(mesh);
            auto bak = lagrange::to_legacy_mesh<MeshType>(res);
            auto map = rename_attributes_with_suffix(*bak);
            assert_same_legacy_mesh<NarrowScalar>(mesh, *bak);
            assert_same_attribute_buffers(mesh, res, map);
        }
        {
            auto res = lagrange::to_surface_mesh_wrap<Scalar, Index>(cmesh);
            auto bak = lagrange::to_legacy_mesh<MeshType>(res);
            auto map = rename_attributes_with_suffix(*bak);
            assert_same_legacy_mesh<NarrowScalar>(mesh, *bak);
            assert_same_attribute_buffers(mesh, res, map);
        }

        // The following should also NOT compile
        // if constexpr (std::is_same_v<Scalar, double>) {
        //     lagrange::Mesh<Eigen::MatrixXd, Eigen::Matrix<Index, Eigen::Dynamic, 3,
        //     Eigen::RowMajor>> mesh2; auto res6 = lagrange::to_surface_mesh_wrap<Scalar,
        //     Index>(mesh2);
        // }
    }

    // This one should NOT compile
    // TODO: Write custom testing script to test failed compilation scenario...
    // auto res6 = lagrange::to_surface_mesh_wrap<Scalar, Index>(MeshType());
}

template <typename Scalar, typename Index, typename MeshType>
void test_from_surface_mesh()
{
    using VertexArray = typename MeshType::VertexArray;
    using FacetArray = typename MeshType::FacetArray;
    using MeshScalar = typename MeshType::Scalar;

    // Choose either MeshScalar or Scalar, whichever is narrower!
    using NarrowScalar =
        std::conditional_t<is_narrowing_conversion_v<MeshScalar, Scalar>, Scalar, MeshScalar>;

    const int dim = deduce_num_cols<VertexArray>();
    const int nvpf = deduce_num_cols<FacetArray>();
    const bool with_edges = true;

    auto mesh = create_surface_mesh<Scalar, Index>(dim, nvpf, with_edges);
    auto res = lagrange::to_legacy_mesh<MeshType>(mesh);
    auto bak = lagrange::to_surface_mesh_copy<Scalar, Index>(*res);

    assert_same_surface_mesh<NarrowScalar, MeshScalar>(mesh, bak);
}

template <typename Scalar, typename Index, typename MeshType>
void test_from_surface_mesh_empty()
{
    using MeshScalar = typename MeshType::Scalar;

    // Choose either MeshScalar or Scalar, whichever is narrower!
    using NarrowScalar =
        std::conditional_t<is_narrowing_conversion_v<MeshScalar, Scalar>, Scalar, MeshScalar>;

    lagrange::SurfaceMesh<Scalar, Index> mesh;
    auto res = lagrange::to_legacy_mesh<MeshType>(mesh);
    auto bak = lagrange::to_surface_mesh_copy<Scalar, Index>(*res);

    assert_same_surface_mesh<NarrowScalar, MeshScalar>(mesh, bak);
}

void edge_sort_naive(std::vector<std::array<int, 2>>& edges)
{
    for (auto& e : edges) {
        std::sort(e.begin(), e.end());
    }
    std::sort(edges.begin(), edges.end());
}

void edge_sort_fast(std::vector<std::array<int, 2>>& edges)
{
    int num_edges = static_cast<int>(edges.size());
    int num_vertices = 0;
    for (auto e : edges) {
        num_vertices = std::max(num_vertices, e[0] + 1);
        num_vertices = std::max(num_vertices, e[1] + 1);
    }
    std::vector<int> buckets(num_vertices + 1);
    auto ids = lagrange::detail::fast_edge_sort<int>(
        num_edges,
        num_vertices,
        [&](int e) -> std::array<int, 2> {
            return edges[e];
        },
        buckets);
    std::vector<std::array<int, 2>> new_edges(num_edges);
    for (size_t i = 0; i < ids.size(); ++i) {
        std::array<int, 2> e = edges[ids[i]];
        std::sort(e.begin(), e.end());
        new_edges[i] = e;
    }
    edges = new_edges;
}

void test_edge_sort(const std::vector<std::array<int, 2>> &edges)
{
    std::vector<std::array<int, 2>> edges_fast = edges;
    std::vector<std::array<int, 2>> edges_naive = edges;
    edge_sort_fast(edges_fast);
    edge_sort_naive(edges_naive);
    CAPTURE(edges);
    REQUIRE(edges_fast == edges_naive);
}

} // namespace

// TODO: converting from a mesh with normal/uv/color but invalid number of channel should fail

TEST_CASE("mesh_convert: to_surface_mesh", "[core][mesh_convert]")
{
#define LA_X_to_surface_mesh(MeshType, Scalar, Index)      \
    test_to_surface_mesh<Scalar, Index, MeshType>();       \
    test_to_surface_mesh_empty<Scalar, Index, MeshType>(); \
    test_to_surface_duplicate<Scalar, Index, MeshType>();
#define LA_X_to_surface_mesh_aux(_, MeshType) LA_SURFACE_MESH_X(to_surface_mesh, MeshType)
    LA_LEGACY_MESH_X(to_surface_mesh_aux, 0)
}

TEST_CASE("mesh_convert: from_surface_mesh", "[core][mesh_convert]")
{
#define LA_X_from_surface_mesh(MeshType, Scalar, Index) \
    test_from_surface_mesh<Scalar, Index, MeshType>();  \
    test_from_surface_mesh_empty<Scalar, Index, MeshType>();
#define LA_X_from_surface_mesh_aux(_, MeshType) LA_SURFACE_MESH_X(from_surface_mesh, MeshType)
    LA_LEGACY_MESH_X(from_surface_mesh_aux, 0)
}

TEST_CASE("mesh_convert: fast_edge_sort", "[core][mesh_convert]")
{
    std::mt19937 gen;
    for (int nv : {0, 1, 2, 5, 10, 100}) {
        if (nv == 0) {
            std::vector<std::array<int, 2>> edges;
            test_edge_sort(edges);
        } else {
            std::uniform_int_distribution<int> dist(0, nv - 1);
            for (int ne : {0, 1, 2, 5, 10, 100}) {
                std::vector<std::array<int, 2>> edges(ne);
                for (auto& e : edges) {
                    e[0] = dist(gen);
                    e[1] = dist(gen);
                }
                test_edge_sort(edges);
            }
        }
    }
}
