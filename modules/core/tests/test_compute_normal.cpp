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
#include <lagrange/testing/common.h>
#include <lagrange/testing/detect_fp_behavior.h>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>

#include <lagrange/IndexedAttribute.h>
#include <lagrange/Logger.h>
#include <lagrange/Mesh.h>
#include <lagrange/common.h>
#include <lagrange/compute_facet_normal.h>
#include <lagrange/compute_normal.h>
#include <lagrange/compute_vertex_normal.h>
#include <lagrange/create_mesh.h>
#include <lagrange/io/save_mesh.h>
#include <lagrange/mesh_convert.h>
#include <lagrange/unify_index_buffer.h>
#include <lagrange/utils/geometry3d.h>
#include <lagrange/views.h>
#include <lagrange/weld_indexed_attribute.h>

#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

namespace {

template <class T1, class T2>
bool equal(lagrange::span<T1> const& l, lagrange::span<T2> const& r)
{
    return (l.size() == r.size() && std::equal(l.begin(), l.end(), r.begin()));
}

} // namespace

TEST_CASE("compute_normal", "[surface][attribute][normal][utilities]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    SECTION("Single triangle")
    {
        lagrange::SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_triangle(0, 1, 2);

        auto id = compute_normal<Scalar, Index>(mesh, [](Index) -> bool { return true; });
        REQUIRE(mesh.is_attribute_type<Scalar>(id));
        REQUIRE(mesh.is_attribute_indexed(id));
        auto& attr = mesh.get_indexed_attribute<Scalar>(id);
        auto& values = attr.values();
        auto& indices = attr.indices();
        REQUIRE(values.get_num_elements() == 3);
        REQUIRE(indices.get_num_elements() == 3);

        for (Index ci = 0; ci < 3; ci++) {
            auto n = values.get_row(indices.get(ci));
            REQUIRE(n[0] == Catch::Approx(0));
            REQUIRE(n[1] == Catch::Approx(0));
            REQUIRE(n[2] == Catch::Approx(1));
            REQUIRE(n[0] * n[0] + n[1] * n[1] + n[2] * n[2] == Catch::Approx(1));
        }
    }
    SECTION("Two triangles")
    {
        lagrange::SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({1, 1, 0});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(2, 1, 3);

        SECTION("Flat")
        {
            auto id = compute_normal<Scalar, Index>(mesh, [](Index) -> bool { return true; });
            REQUIRE(mesh.is_attribute_type<Scalar>(id));
            REQUIRE(mesh.is_attribute_indexed(id));
            auto& attr = mesh.get_indexed_attribute<Scalar>(id);
            auto& values = attr.values();
            auto& indices = attr.indices();
            REQUIRE(values.get_num_elements() == 4);
            REQUIRE(indices.get_num_elements() == 6);

            for (Index ci = 0; ci < mesh.get_num_corners(); ci++) {
                auto n = values.get_row(indices.get(ci));
                REQUIRE(n[0] == Catch::Approx(0));
                REQUIRE(n[1] == Catch::Approx(0));
                REQUIRE(n[2] == Catch::Approx(1));
                REQUIRE(n[0] * n[0] + n[1] * n[1] + n[2] * n[2] == Catch::Approx(1));
            }
        }

        SECTION("Non-flat")
        {
            mesh.ref_position(3)[2] = -1;

            auto id = compute_normal<Scalar, Index>(mesh, [&](Index ei) -> bool {
                auto e = mesh.get_edge_vertices(ei);
                if (e[1] > e[0]) std::swap(e[0], e[1]);
                if (e[0] == 1 && e[1] == 2)
                    return true;
                else
                    return false;
            });
            REQUIRE(mesh.is_attribute_type<Scalar>(id));
            REQUIRE(mesh.is_attribute_indexed(id));
            auto& attr = mesh.get_indexed_attribute<Scalar>(id);
            auto& values = attr.values();
            auto& indices = attr.indices();
            REQUIRE(values.get_num_elements() == 6);
            REQUIRE(indices.get_num_elements() == 6);

            // Check first triangle.
            for (Index ci = mesh.get_facet_corner_begin(0); ci < mesh.get_facet_corner_end(0);
                 ci++) {
                auto n = values.get_row(indices.get(ci));
                REQUIRE(n[0] == Catch::Approx(0));
                REQUIRE(n[1] == Catch::Approx(0));
                REQUIRE(n[2] == Catch::Approx(1));
                REQUIRE(n[0] * n[0] + n[1] * n[1] + n[2] * n[2] == Catch::Approx(1));
            }

            // Check second triangle
            for (Index ci = mesh.get_facet_corner_begin(1); ci < mesh.get_facet_corner_end(1);
                 ci++) {
                auto n = values.get_row(indices.get(ci));
                REQUIRE(n[0] == Catch::Approx(n[1]));
                REQUIRE(n[1] == Catch::Approx(n[2]));
                REQUIRE(n[0] * n[0] + n[1] * n[1] + n[2] * n[2] == Catch::Approx(1));
            }
        }
    }
    SECTION("Single quad")
    {
        lagrange::SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({1, 1, -1});
        mesh.add_quad(0, 1, 3, 2);

        SECTION("Non-planar")
        {
            auto id = compute_normal<Scalar, Index>(mesh, [](Index) -> bool { return true; });
            REQUIRE(mesh.is_attribute_type<Scalar>(id));
            REQUIRE(mesh.is_attribute_indexed(id));
            auto& attr = mesh.get_indexed_attribute<Scalar>(id);
            auto& values = attr.values();
            auto& indices = attr.indices();
            REQUIRE(values.get_num_elements() == 4);
            REQUIRE(indices.get_num_elements() == 4);

            auto n0 = values.get_row(indices.get(0));
            auto n1 = values.get_row(indices.get(1));
            auto n2 = values.get_row(indices.get(2));
            auto n3 = values.get_row(indices.get(3));

            REQUIRE(n0[0] == Catch::Approx(0));
            REQUIRE(n0[1] == Catch::Approx(0));
            REQUIRE(n0[2] == Catch::Approx(1));

            REQUIRE(n1[2] == Catch::Approx(n3[2]));
            REQUIRE(n1[0] * n1[0] + n1[1] * n1[1] + n1[2] * n1[2] == Catch::Approx(1));
            REQUIRE(n3[0] * n3[0] + n3[1] * n3[1] + n3[2] * n3[2] == Catch::Approx(1));

            REQUIRE(n2[0] == Catch::Approx(1 / std::sqrt(3)));
            REQUIRE(n2[1] == Catch::Approx(1 / std::sqrt(3)));
            REQUIRE(n2[2] == Catch::Approx(1 / std::sqrt(3)));
        }

        SECTION("Concave")
        {
            auto v = mesh.ref_position(3);
            v[0] = 0.1;
            v[1] = 0.1;
            v[2] = 0;

            auto id = compute_normal<Scalar, Index>(mesh, [](Index) -> bool { return true; });
            REQUIRE(mesh.is_attribute_type<Scalar>(id));
            REQUIRE(mesh.is_attribute_indexed(id));
            auto& attr = mesh.get_indexed_attribute<Scalar>(id);
            auto& values = attr.values();
            auto& indices = attr.indices();
            REQUIRE(values.get_num_elements() == 4);
            REQUIRE(indices.get_num_elements() == 4);

            for (Index ci = 0; ci < 4; ci++) {
                INFO("ci = " << ci);
                auto n = values.get_row(indices.get(ci));
                REQUIRE(n[0] == Catch::Approx(0));
                REQUIRE(n[1] == Catch::Approx(0));
                REQUIRE(n[2] == Catch::Approx(1));
            }
        }
    }

    SECTION("Pyramid")
    {
        lagrange::SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({1, 1, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({0.5, 0.5, 1.0});
        mesh.add_triangle(0, 1, 4);
        mesh.add_triangle(1, 2, 4);
        mesh.add_triangle(2, 3, 4);
        mesh.add_triangle(3, 0, 4);
        mesh.add_quad(3, 2, 1, 0);
        mesh.initialize_edges();

        SECTION("No cone vertices")
        {
            AttributeId id = compute_normal(mesh, M_PI / 2 - 0.1);
            REQUIRE(mesh.is_attribute_type<Scalar>(id));
            REQUIRE(mesh.is_attribute_indexed(id));
            auto& attr = mesh.get_indexed_attribute<Scalar>(id);
            auto& values = attr.values();
            auto& indices = attr.indices();
            REQUIRE(values.get_num_elements() == 9);
            REQUIRE(indices.get_num_elements() == 16);

            auto n0 = values.get_row(indices.get(2));
            auto n1 = values.get_row(indices.get(5));
            auto n2 = values.get_row(indices.get(8));
            auto n3 = values.get_row(indices.get(11));

            REQUIRE(n0[2] == Catch::Approx(1));
            REQUIRE(n1[2] == Catch::Approx(1));
            REQUIRE(n2[2] == Catch::Approx(1));
            REQUIRE(n3[2] == Catch::Approx(1));
        }

        SECTION("With cone vertices")
        {
            std::vector<Index> cones;
            cones.push_back(4);
            AttributeId id = compute_normal<Scalar, Index>(mesh, M_PI / 2 - 0.1, cones);
            REQUIRE(mesh.is_attribute_type<Scalar>(id));
            REQUIRE(mesh.is_attribute_indexed(id));
            auto& attr = mesh.get_indexed_attribute<Scalar>(id);
            auto& values = attr.values();
            auto& indices = attr.indices();
            REQUIRE(values.get_num_elements() == 12);
            REQUIRE(indices.get_num_elements() == 16);

            auto n0 = values.get_row(indices.get(2));
            auto n1 = values.get_row(indices.get(5));
            auto n2 = values.get_row(indices.get(8));
            auto n3 = values.get_row(indices.get(11));

            REQUIRE(n0[2] < 0.8);
            REQUIRE(n1[2] < 0.8);
            REQUIRE(n2[2] < 0.8);
            REQUIRE(n3[2] < 0.8);
        }
    }
}

TEST_CASE("compute_normal: facet normal attr", "[surface][attribute][normal][utilities]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    lagrange::SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertex({0, 0, 0});
    mesh.add_vertex({1, 0, 0});
    mesh.add_vertex({1, 1, 0});
    mesh.add_vertex({0, 1, 0});
    mesh.add_vertex({0.5, 0.5, 1.0});
    mesh.add_triangle(0, 1, 4);
    mesh.add_triangle(1, 2, 4);
    mesh.add_triangle(2, 3, 4);
    mesh.add_triangle(3, 0, 4);
    mesh.add_quad(3, 2, 1, 0);

    NormalOptions options;
    auto facet_normal_name = options.facet_normal_attribute_name;

    REQUIRE(!mesh.has_attribute(facet_normal_name));
    compute_normal(mesh, M_PI / 4, {}, options);
    REQUIRE(!mesh.has_attribute(facet_normal_name));

    options.keep_facet_normals = true;
    compute_normal(mesh, M_PI / 4, {}, options);
    REQUIRE(mesh.has_attribute(facet_normal_name));
    mesh.delete_attribute(facet_normal_name);

    // Whether to keep the facet attribute or not does *not* depend on the presence of edge
    // information, contrary to the compute_vertex_normal method.
    mesh.initialize_edges();
    compute_normal(mesh, M_PI / 4, {}, options);
    REQUIRE(mesh.has_attribute(facet_normal_name));
}

namespace {

template <typename DerivedA, typename DerivedB>
bool allclose(
    const Eigen::DenseBase<DerivedA>& a,
    const Eigen::DenseBase<DerivedB>& b,
    const typename DerivedA::RealScalar& rtol =
        Eigen::NumTraits<typename DerivedA::RealScalar>::dummy_precision(),
    const typename DerivedA::RealScalar& atol =
        Eigen::NumTraits<typename DerivedA::RealScalar>::epsilon())
{
    return ((a.derived() - b.derived()).array().abs() <= (atol + rtol * b.derived().array().abs()))
        .all();
}

} // namespace

TEST_CASE("compute_normal nmtest", "[core][normal]" LA_CORP_FLAG)
{
    using Scalar = float;
    using Index = uint32_t;

    auto original_mesh =
        lagrange::testing::load_surface_mesh<Scalar, Index>("corp/core/nmtest_no_tb_tri.obj");
    lagrange::weld_indexed_attribute(
        original_mesh,
        original_mesh.get_attribute_id(lagrange::AttributeName::texcoord));
    original_mesh.delete_attribute(lagrange::AttributeName::normal);

    for (double angle_threshold_deg : {0., 45., 90., 180.}) {
        auto mesh = original_mesh;
        const Scalar EPS = static_cast<Scalar>(1e-3);
        auto nrm_id = lagrange::compute_normal(
            mesh,
            std::max(Scalar(0), Scalar(angle_threshold_deg) * Scalar(M_PI / 180.0) - EPS));

        std::string nrm_name(mesh.get_attribute_name(nrm_id));
        mesh = lagrange::unify_index_buffer(mesh, {nrm_id});
        mesh.rename_attribute(nrm_name, "Vertex_Normal"); // match ply attribute name
        mesh.rename_attribute("color", "Vertex_Color"); // match ply attribute name

        auto filename = fmt::format("nmtest_normal_{}.ply", angle_threshold_deg);

        // Uncomment to save a new output
        // lagrange::io::save_mesh(filename, mesh);

        auto expected = lagrange::testing::load_surface_mesh<Scalar, Index>(
            fmt::format("corp/core/regression/{}", filename));

        lagrange::seq_foreach_named_attribute_read<lagrange::AttributeElement::Vertex>(
            mesh,
            [&](const auto& name, auto& attr) {
                using AttributeType = std::decay_t<decltype(attr)>;
                using ValueType = typename AttributeType::ValueType;
                if constexpr (std::is_same_v<ValueType, Scalar>) {
                    CAPTURE(angle_threshold_deg, name);
                    REQUIRE(expected.has_attribute(name));
                    const ValueType eps(static_cast<ValueType>(1e-4));
                    auto X = matrix_view(attr);
                    auto Y = lagrange::attribute_matrix_view<ValueType>(expected, name);
                    for (Eigen::Index i = 0; i < X.size(); ++i) {
                        REQUIRE_THAT(
                            X.data()[i],
                            Catch::Matchers::WithinRel(Y.data()[i], eps) ||
                                (Catch::Matchers::WithinAbs(Y.data()[i], eps) &&
                                 Catch::Matchers::WithinAbs(0, eps)));
                    }
                }
            });
    }
}

TEST_CASE("compute_normal benchmark", "[surface][attribute][normal][utilities][!benchmark]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/dragon.obj");
    mesh.delete_attribute("normal");
    compute_facet_normal(mesh);

    BENCHMARK_ADVANCED("compute_normal")(Catch::Benchmark::Chronometer meter)
    {
        if (mesh.has_attribute("@normal"))
            mesh.delete_attribute("@normal", AttributeDeletePolicy::Force);
        meter.measure([&]() { return compute_normal(mesh, M_PI / 4); });
        mesh.clear_edges();
    };

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
    using MeshType = TriangleMesh3D;
    mesh.rename_attribute("@facet_normal", "normal");
    auto legacy_mesh = to_legacy_mesh<MeshType>(mesh);

    BENCHMARK_ADVANCED("legacy::compute_normal")(Catch::Benchmark::Chronometer meter)
    {
        if (legacy_mesh->has_indexed_attribute("normal"))
            legacy_mesh->remove_indexed_attribute("normal");
        meter.measure([&]() { return compute_normal(*legacy_mesh, M_PI / 4); });
    };
#endif
}

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS

TEST_CASE("legacy::compute_normal vs compute_normal", "[mesh][attribute][normal][legacy]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;
    using MeshType = TriangleMesh3D;

    auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/blub/blub.obj");
    compute_facet_normal(mesh);

    auto normal_id = compute_normal(mesh, M_PI / 4);
    const auto& normal_attr = mesh.get_indexed_attribute<Scalar>(normal_id);
    auto normal_values = matrix_view(normal_attr.values());
    auto normal_indices = normal_attr.indices().get_all();

    using MeshType = TriangleMesh3D;
    mesh.rename_attribute("@facet_normal", "normal");
    auto legacy_mesh = to_legacy_mesh<MeshType>(mesh);

    CHECK(legacy_mesh->has_indexed_attribute("@normal"));
    legacy_mesh->remove_indexed_attribute("@normal");
    compute_normal(*legacy_mesh, M_PI / 4);
    legacy_mesh->remove_facet_attribute("normal");
    CHECK(legacy_mesh->has_indexed_attribute("normal"));

    auto new_mesh = to_surface_mesh_copy<Scalar, Index>(*legacy_mesh);
    const auto& new_attr = new_mesh.get_indexed_attribute<Scalar>("normal");
    auto new_values = matrix_view(new_attr.values());
    auto new_indices = new_attr.indices().get_all();

    REQUIRE(new_indices.size() == normal_indices.size());
    for (size_t i = 0; i < new_indices.size(); i++) {
        INFO("i=" << i << " new_index=" << new_indices[i] << " old_index=" << normal_indices[i]);
        const Eigen::Matrix<Scalar, 1, 3> new_normal = new_values.row(new_indices[i]);
        const Eigen::Matrix<Scalar, 1, 3> old_normal = normal_values.row(normal_indices[i]);
        INFO("new_normal=(" << new_normal << ")");
        INFO("old_normal=(" << old_normal << ")");

        REQUIRE(new_normal.squaredNorm() == Catch::Approx(1));
        REQUIRE(old_normal.squaredNorm() == Catch::Approx(1));
        REQUIRE(angle_between(new_normal, old_normal) == Catch::Approx(0).margin(1e-6));
    }
}


TEST_CASE("legacy::compute_normal", "[mesh][attribute][normal][legacy]" LA_SLOW_DEBUG_FLAG)
{
    using namespace lagrange;

    SECTION("Cube")
    {
        auto mesh = create_cube();
        using MeshType = typename decltype(mesh)::element_type;

        SECTION("Keep edge sharp")
        {
            compute_normal(*mesh, M_PI * 0.25);
            REQUIRE(mesh->has_indexed_attribute("normal"));

            MeshType::AttributeArray normal_values;
            MeshType::FacetArray normal_indices;
            std::tie(normal_values, normal_indices) = mesh->get_indexed_attribute("normal");

            REQUIRE(normal_values.rows() == 24);
            REQUIRE(normal_values.cols() == 3);
            REQUIRE(normal_indices.rows() == mesh->get_num_facets());
            REQUIRE(normal_indices.cols() == mesh->get_dim());

            auto normal_mesh = wrap_with_mesh(normal_values, normal_indices);
            normal_mesh->initialize_components();
            REQUIRE(normal_mesh->get_num_components() == 6);
        }

        SECTION("Smooth edge")
        {
            compute_normal(*mesh, M_PI);
            REQUIRE(mesh->has_indexed_attribute("normal"));

            MeshType::AttributeArray normal_values;
            MeshType::FacetArray normal_indices;
            std::tie(normal_values, normal_indices) = mesh->get_indexed_attribute("normal");

            REQUIRE(normal_values.rows() == 8);
            REQUIRE(normal_values.cols() == 3);
            REQUIRE(normal_indices.rows() == mesh->get_num_facets());
            REQUIRE(normal_indices.cols() == mesh->get_dim());

            auto normal_mesh = wrap_with_mesh(normal_values, normal_indices);
            normal_mesh->initialize_components();
            REQUIRE(normal_mesh->get_num_components() == 1);
        }
    }

    SECTION("Pyramid")
    {
        Eigen::Matrix<double, Eigen::Dynamic, 3> vertices(5, 3);
        vertices << 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0, 1.0, 0.0, 0.5, 0.5, 1.0;
        Eigen::Matrix<uint64_t, Eigen::Dynamic, 3> facets(6, 3);
        facets << 0, 1, 4, 1, 2, 4, 2, 3, 4, 3, 0, 4, 0, 2, 1, 0, 3, 2;

        auto mesh = wrap_with_mesh(vertices, facets);
        using MeshType = typename decltype(mesh)::element_type;

        SECTION("No cone vertices")
        {
            compute_normal(*mesh, M_PI * 0.5 - 0.1);
            REQUIRE(mesh->has_indexed_attribute("normal"));

            MeshType::AttributeArray normal_values;
            MeshType::FacetArray normal_indices;
            std::tie(normal_values, normal_indices) = mesh->get_indexed_attribute("normal");

            const Eigen::Matrix<double, 1, 3> up_dir(0, 0, 1);
            REQUIRE(
                (normal_values.row(normal_indices(0, 2)) - up_dir).norm() == Catch::Approx(0.0));
            REQUIRE(
                (normal_values.row(normal_indices(1, 2)) - up_dir).norm() == Catch::Approx(0.0));
            REQUIRE(
                (normal_values.row(normal_indices(2, 2)) - up_dir).norm() == Catch::Approx(0.0));
            REQUIRE(
                (normal_values.row(normal_indices(3, 2)) - up_dir).norm() == Catch::Approx(0.0));
        }

        SECTION("With cone vertices")
        {
            compute_normal(*mesh, M_PI * 0.5 - 0.1, {4});
            REQUIRE(mesh->has_indexed_attribute("normal"));

            MeshType::AttributeArray normal_values;
            MeshType::FacetArray normal_indices;
            std::tie(normal_values, normal_indices) = mesh->get_indexed_attribute("normal");

            const Eigen::Matrix<double, 1, 3> up_dir(0, 0, 1);
            REQUIRE((normal_values.row(normal_indices(0, 2)) - up_dir).norm() > 0.5);
            REQUIRE((normal_values.row(normal_indices(1, 2)) - up_dir).norm() > 0.5);
            REQUIRE((normal_values.row(normal_indices(2, 2)) - up_dir).norm() > 0.5);
            REQUIRE((normal_values.row(normal_indices(3, 2)) - up_dir).norm() > 0.5);
        }
    }

    SECTION("Blub")
    {
        using MeshType = TriangleMesh3D;
        auto mesh = lagrange::testing::load_mesh<MeshType>("open/core/blub/blub.obj");
        REQUIRE(mesh->get_num_vertices() == 7106);
        REQUIRE(mesh->get_num_facets() == 14208);

        compute_normal(*mesh, M_PI * 0.25);
        REQUIRE(mesh->has_indexed_attribute("normal"));

        compute_triangle_normal(*mesh);
        REQUIRE(mesh->has_facet_attribute("normal"));

        MeshType::AttributeArray normal_values;
        MeshType::FacetArray normal_indices;
        std::tie(normal_values, normal_indices) = mesh->get_indexed_attribute("normal");

        const auto& triangle_normals = mesh->get_facet_attribute("normal");

        const auto num_facets = mesh->get_num_facets();
        REQUIRE(normal_indices.rows() == num_facets);

        for (auto i : range(num_facets)) {
            for (auto j : {0, 1, 2}) {
                Eigen::Vector3d c_normal = normal_values.row(normal_indices(i, j));
                Eigen::Vector3d v_normal = triangle_normals.row(i);
                auto theta = angle_between(c_normal, v_normal);
                REQUIRE(theta < M_PI * 0.5);
            }
        }
    }

    SECTION("Degenerate")
    {
        Eigen::Matrix<double, Eigen::Dynamic, 3> vertices(2, 3);
        vertices << 0.1, 1.0, 0.5, 0.9, 0.3, 0.7;
        Eigen::Matrix<uint64_t, Eigen::Dynamic, 3> facets(2, 3);
        facets << 0, 1, 1, 1, 1, 1;

        auto mesh = wrap_with_mesh(vertices, facets);
        using MeshType = typename decltype(mesh)::element_type;

        compute_normal(*mesh, M_PI * 0.25);
        REQUIRE(mesh->has_indexed_attribute("normal"));

        compute_triangle_normal(*mesh);
        REQUIRE(mesh->has_facet_attribute("normal"));

        MeshType::AttributeArray normal_values;
        MeshType::FacetArray normal_indices;
        std::tie(normal_values, normal_indices) = mesh->get_indexed_attribute("normal");

        const auto& triangle_normals = mesh->get_facet_attribute("normal");

        if (lagrange::testing::detect_fp_behavior() ==
            lagrange::testing::FloatPointBehavior::XcodeGreaterThan14) {
            // For some reason x.cross(x) is not zero on arm64 Xcode 14+. It's around 1e-17, which
            // is enough for stableNormalize() to produce a non-zero first row.
            REQUIRE(normal_values(Eigen::seq(1, Eigen::last), Eigen::all).isZero(0));
            REQUIRE(triangle_normals(Eigen::seq(1, Eigen::last), Eigen::all).isZero(0));
        } else {
            REQUIRE(normal_values.isZero(0));
            REQUIRE(triangle_normals.isZero(0));
        }
    }
}
#endif
