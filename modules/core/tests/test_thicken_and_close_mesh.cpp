/*
 * Copyright 2021 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/Logger.h>
#include <lagrange/find_matching_attributes.h>
#include <lagrange/testing/check_mesh.h>
#include <lagrange/testing/common.h>
#include <lagrange/thicken_and_close_mesh.h>
#include <lagrange/topology.h>
#include <lagrange/views.h>

#include <catch2/matchers/catch_matchers_floating_point.hpp>

namespace {

template <typename DerivedA, typename DerivedB>
void require_approx(
    const Eigen::MatrixBase<DerivedA>& A,
    const Eigen::MatrixBase<DerivedB>& B,
    typename DerivedA::Scalar eps_rel,
    typename DerivedA::Scalar eps_abs)
{
    REQUIRE(A.rows() == B.rows());
    REQUIRE(A.cols() == B.cols());
    for (Eigen::Index i = 0; i < A.size(); ++i) {
        REQUIRE_THAT(
            A.derived().data()[i],
            Catch::Matchers::WithinRel(B.derived().data()[i], eps_rel) ||
                (Catch::Matchers::WithinAbs(0, eps_abs) &&
                 Catch::Matchers::WithinAbs(B.derived().data()[i], eps_abs)));
    }
}

} // namespace

TEST_CASE("thicken_and_close_mesh wing", "[mesh][thicken_and_close_mesh][wing]" LA_CORP_FLAG)
{
    using Scalar = float;
    using Index = uint32_t;
    auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("corp/core/wing.obj");
    REQUIRE(mesh.is_triangle_mesh());

    lagrange::ThickenAndCloseOptions options;
    options.offset_amount = 1.0;
    options.num_segments = 3;
    auto thickened_mesh = lagrange::thicken_and_close_mesh(mesh, options);
    REQUIRE(mesh.get_num_vertices() * 2 <= thickened_mesh.get_num_vertices());
    REQUIRE(is_manifold(thickened_mesh));
    REQUIRE(compute_euler(thickened_mesh) == 2);
}

TEST_CASE("thicken_and_close_mesh stanford-bunny", "[mesh][thicken_and_close_mesh][fandisk]")
{
    using Scalar = float;
    using Index = uint32_t;
    auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/fandisk.obj");
    REQUIRE(compute_euler(mesh) == 2);
    REQUIRE(mesh.is_triangle_mesh());

    lagrange::ThickenAndCloseOptions options;
    options.offset_amount = 0.05;
    options.num_segments = 3;
    auto thickened_mesh = thicken_and_close_mesh(mesh, options);
    REQUIRE(mesh.get_num_vertices() * 2 <= thickened_mesh.get_num_vertices());
    REQUIRE(is_manifold(thickened_mesh));
    REQUIRE(compute_euler(thickened_mesh) == 4);
}

TEST_CASE("thicken_and_close_mesh hemisphere", "[mesh][thicken_and_close_mesh][hemisphere]")
{
    using Scalar = float;
    using Index = uint32_t;
    auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/hemisphere.obj");
    REQUIRE(compute_euler(mesh) == 1);

    lagrange::ThickenAndCloseOptions options;
    options.direction = std::array<double, 3>{0, 1, 0};

    options.offset_amount = -0.5;
    options.mirror_ratio = 0;
    auto flat_mesh = lagrange::thicken_and_close_mesh(mesh, options);

    options.offset_amount = 0;
    options.mirror_ratio = -1;
    auto mirrored_mesh = lagrange::thicken_and_close_mesh(mesh, options);

    options.offset_amount = -0.5;
    options.mirror_ratio = 1;
    auto thickened_mesh = lagrange::thicken_and_close_mesh(mesh, options);

    REQUIRE(flat_mesh.get_num_vertices() == 682);
    REQUIRE(flat_mesh.get_num_facets() == 1360);
    REQUIRE(mirrored_mesh.get_num_vertices() == 682);
    REQUIRE(mirrored_mesh.get_num_facets() == 1360);
    REQUIRE(thickened_mesh.get_num_vertices() == 682);
    REQUIRE(thickened_mesh.get_num_facets() == 1360);
    REQUIRE(is_manifold(thickened_mesh));
    REQUIRE(compute_euler(thickened_mesh) == 2);
}

TEST_CASE("thicken_and_close_mesh poly", "[mesh][thicken_and_close_mesh]")
{
    using Scalar = float;
    using Index = uint32_t;
    auto mesh =
        lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/poly/mixedFaringPart.obj");
    REQUIRE(compute_euler(mesh) == 0); // torus-like

    lagrange::ThickenAndCloseOptions options;
    options.offset_amount = 0.01;
    options.num_segments = 3;
    auto thickened_mesh = lagrange::thicken_and_close_mesh(mesh, options);
    lagrange::testing::check_mesh(thickened_mesh);
    REQUIRE(is_manifold(thickened_mesh));
    REQUIRE(compute_euler(thickened_mesh) == 0); // also torus-like
}

TEST_CASE("thicken_and_close_mesh uv", "[mesh][thicken_and_close_mesh]")
{
    using Scalar = float;
    using Index = uint32_t;
    auto mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/blub_open.obj");

    lagrange::ThickenAndCloseOptions options;
    options.offset_amount = 0.08;
    options.num_segments = 3;
    bool has_uvs = false;
    for (auto id : find_matching_attributes(mesh, lagrange::AttributeUsage::UV)) {
        has_uvs = true;
        lagrange::logger().info("Indexed attribute: {}", mesh.get_attribute_name(id));
        options.indexed_attributes.emplace_back(mesh.get_attribute_name(id));
    }
    REQUIRE(has_uvs);
    mesh = lagrange::thicken_and_close_mesh(std::move(mesh), options);
    lagrange::testing::check_mesh(mesh);
    REQUIRE(is_manifold(mesh));
    REQUIRE(compute_euler(mesh) == 2);

    auto expected =
        lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/blub_thickened.obj");
    require_approx(vertex_view(mesh), vertex_view(expected), 1e-4f, 1e-4f);
    REQUIRE(facet_view(mesh) == facet_view(expected));
    for (auto id : find_matching_attributes(mesh, lagrange::AttributeUsage::UV)) {
        auto name = mesh.get_attribute_name(id);
        REQUIRE(expected.has_attribute(name));
        auto& computed_attr = mesh.get_indexed_attribute<Scalar>(name);
        auto& expected_attr = expected.get_indexed_attribute<Scalar>(name);
        REQUIRE(matrix_view(computed_attr.indices()) == matrix_view(expected_attr.indices()));
        require_approx(
            matrix_view(computed_attr.values()),
            matrix_view(expected_attr.values()),
            1e-4f,
            1e-4f);
    }
}
