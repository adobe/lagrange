/*
 * Copyright 2024 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
////////////////////////////////////////////////////////////////////////////////
#include <lagrange/Logger.h>
#include <lagrange/cast_attribute.h>
#include <lagrange/compute_vertex_normal.h>
#include <lagrange/find_matching_attributes.h>
#include <lagrange/io/save_mesh.h>
#include <lagrange/poisson/AttributeEvaluator.h>
#include <lagrange/poisson/mesh_from_oriented_points.h>
#include <lagrange/testing/common.h>
#include <lagrange/topology.h>
#include <lagrange/utils/fmt_eigen.h>
#include <lagrange/views.h>

#include <catch2/catch_test_macros.hpp>
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("PoissonRecon: Simple", "[poisson]")
{
    using Scalar = float;
    using Index = uint32_t;

    lagrange::poisson::ReconstructionOptions recon_options;
#ifndef NDEBUG
    recon_options.octree_depth = 5;
#endif

    auto input_mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/ball.obj");
    lagrange::compute_vertex_normal(input_mesh);
    input_mesh.clear_facets();

    tbb::task_arena arena(1);
    arena.execute([&] {
        auto mesh1 = lagrange::poisson::mesh_from_oriented_points(input_mesh, recon_options);
        auto mesh2 = lagrange::poisson::mesh_from_oriented_points(input_mesh, recon_options);

        REQUIRE(mesh1.get_num_facets() > 0);

        REQUIRE(vertex_view(mesh1) == vertex_view(mesh2));
        REQUIRE(facet_view(mesh1) == facet_view(mesh2));
    });
}

TEST_CASE("PoissonRecon: Octree", "[poisson]")
{
    using Scalar = float;
    using Index = uint32_t;

    lagrange::poisson::ReconstructionOptions recon_options;

    auto input_mesh = lagrange::testing::load_surface_mesh<Scalar, Index>("open/core/ball.obj");
    lagrange::compute_vertex_normal(input_mesh);
    input_mesh.clear_facets();

#ifndef NDEBUG
    unsigned min_depth = 1;
    unsigned max_depth = 5;
#else
    unsigned min_depth = 0;
    unsigned max_depth = 6;
#endif

    tbb::task_arena arena(1);
    arena.execute([&] {
        std::vector<Index> expected_nf = {11296, 8, 104, 504, 2056, 8008};
        for (unsigned depth = min_depth; depth < max_depth; ++depth) {
            recon_options.octree_depth = depth;
            recon_options.use_dirichlet_boundary = true;
            auto mesh = lagrange::poisson::mesh_from_oriented_points(input_mesh, recon_options);
            CHECK(mesh.get_num_facets() == expected_nf[depth]);
            REQUIRE(compute_euler(mesh) == 2);
            REQUIRE(is_manifold(mesh));
        }
    });
}

namespace {

template <typename Scalar, typename Index>
void poisson_recon_with_colors()
{
    lagrange::poisson::ReconstructionOptions recon_options;
    recon_options.interpolated_attribute_name = "Vertex_Color";
    recon_options.output_vertex_depth_attribute_name = "value";
#ifndef NDEBUG
    recon_options.octree_depth = 5;
#endif

    auto input_mesh =
        lagrange::testing::load_surface_mesh<Scalar, Index>("open/poisson/sphere.striped.ply");
    input_mesh.clear_facets();

    tbb::task_arena arena(1);
    arena.execute([&] {
        auto mesh1 = lagrange::poisson::mesh_from_oriented_points(input_mesh, recon_options);
        auto mesh2 = lagrange::poisson::mesh_from_oriented_points(input_mesh, recon_options);

        REQUIRE(mesh1.has_attribute("Vertex_Color"));
        REQUIRE(mesh1.has_attribute("value"));

        REQUIRE(mesh1.get_num_facets() > 0);
        REQUIRE(compute_euler(mesh1) == 2);
        REQUIRE(is_manifold(mesh1));

        REQUIRE(vertex_view(mesh1) == vertex_view(mesh2));
        REQUIRE(facet_view(mesh1) == facet_view(mesh2));
    });
}

} // namespace

TEST_CASE("PoissonRecon: Colors", "[poisson]")
{
    poisson_recon_with_colors<float, uint32_t>();
    poisson_recon_with_colors<double, uint32_t>();
}

namespace {

template <typename Scalar, typename ValueType>
void test_samples(const lagrange::poisson::AttributeEvaluator& evaluator)
{
    Eigen::Matrix3<Scalar> positions;
    Eigen::Matrix3<ValueType> colors;
    positions.col(0) << -1, 0, 0;
    positions.col(1) << 0, 1, 0;
    positions.col(2) << 1, 0, 0;

    for (int i = 0; i < 3; i++) {
        evaluator.eval<Scalar, ValueType>({positions.col(i).data(), 3}, {colors.col(i).data(), 3});
    }

    lagrange::logger().debug("Positions:\n{}", positions);
    lagrange::logger().debug("Colors:\n{}", colors);

    REQUIRE(colors.col(0).isApprox(Eigen::Vector3<ValueType>(1, 0, 0)));
    REQUIRE(colors.col(1).isApprox(Eigen::Vector3<ValueType>(0, 1, 0)));
    REQUIRE(colors.col(2).isApprox(Eigen::Vector3<ValueType>(0, 0, 1)));
}

} // namespace

TEST_CASE("PoissonRecon: Attribute Evaluator", "[poisson]")
{
    using Scalar = float;
    using Index = uint32_t;

    auto input_mesh =
        lagrange::testing::load_surface_mesh<Scalar, Index>("open/poisson/sphere.striped.ply");

    lagrange::cast_attribute_in_place<Scalar>(input_mesh, "Vertex_Color");
    lagrange::attribute_matrix_ref<Scalar>(input_mesh, "Vertex_Color") /= 255.f;

    lagrange::poisson::EvaluatorOptions eval_options;
    eval_options.interpolated_attribute_name = "Vertex_Color";
#ifndef NDEBUG
    eval_options.octree_depth = 5;
#else
    eval_options.octree_depth = 6;
#endif

    tbb::task_arena arena(1);
    arena.execute([&] {
        auto evaluator = lagrange::poisson::AttributeEvaluator(input_mesh, eval_options);
        test_samples<float, Scalar>(evaluator);
        test_samples<double, Scalar>(evaluator);
    });
}
