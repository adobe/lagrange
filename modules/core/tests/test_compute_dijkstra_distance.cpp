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
#include <lagrange/testing/common.h>
#include <catch2/catch_approx.hpp>

#include <lagrange/compute_dijkstra_distance.h>
#include <lagrange/internal/constants.h>
#include <lagrange/views.h>

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
    #include <lagrange/Mesh.h>
    #include <lagrange/common.h>
    #include <lagrange/create_mesh.h>
#endif


#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
TEST_CASE("DijkstraDistance Legacy", "[dijkstra][triangle]")
{
    using namespace lagrange;

    SECTION("cube")
    {
        auto cube = create_cube();
        legacy::compute_dijkstra_distance(*cube, 0, {1.0, 0.0, 0.0}, 0.0);

        REQUIRE(cube->has_vertex_attribute("dijkstra_distance"));
        const auto& dist = cube->get_vertex_attribute("dijkstra_distance");
        REQUIRE(dist.minCoeff() == Catch::Approx(0.0));
        REQUIRE(dist.maxCoeff() <= Catch::Approx(6.0));
    }

    SECTION("sphere")
    {
        auto sphere = create_sphere(4);
        legacy::compute_dijkstra_distance(*sphere, 0, {1.0, 0.0, 0.0}, 0.0);

        REQUIRE(sphere->has_vertex_attribute("dijkstra_distance"));
        const auto& dist = sphere->get_vertex_attribute("dijkstra_distance");
        REQUIRE(dist.minCoeff() == Catch::Approx(0.0));
        REQUIRE(dist.maxCoeff() == Catch::Approx(lagrange::internal::pi).epsilon(0.1));
    }
}
#endif


TEST_CASE("DijkstraDistance", "[dijkstra][surface][triangle]")
{
    using namespace lagrange;
    using Scalar = double;
    using Index = uint32_t;

    SECTION("quad")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({2, 0, 0});
        mesh.add_vertex({0, 2, 0});
        mesh.add_vertex({2, 2, 0});
        mesh.add_triangle(0, 1, 2);
        mesh.add_triangle(2, 1, 3);

        DijkstraDistanceOptions<Scalar, Index> options;
        options.seed_facet = 0;
        options.barycentric_coords = {1, 0, 0};
        options.output_involved_vertices = true;

        auto result = compute_dijkstra_distance(mesh, options);

        REQUIRE(result.has_value());
        REQUIRE(result->size() == 4);
        REQUIRE(mesh.has_attribute(options.output_attribute_name));

        auto dist = matrix_view(mesh.get_attribute<Scalar>(options.output_attribute_name));
        REQUIRE(dist.minCoeff() == Catch::Approx(0.0));
        REQUIRE(dist.maxCoeff() == Catch::Approx(4.0));

        options.output_involved_vertices = false;
        result = compute_dijkstra_distance(mesh, options);
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("mixed")
    {
        SurfaceMesh<Scalar, Index> mesh;
        mesh.add_vertex({0, 0, 0});
        mesh.add_vertex({1, 0, 0});
        mesh.add_vertex({0, 1, 0});
        mesh.add_vertex({1, 1, 0});
        mesh.add_vertex({1, 0, 1});
        mesh.add_vertex({1, 1, 1});
        mesh.add_quad(0, 2, 3, 1);
        mesh.add_triangle(1, 3, 4);
        mesh.add_triangle(4, 3, 5);

        DijkstraDistanceOptions<Scalar, Index> options;
        options.seed_facet = 0;
        options.barycentric_coords = {1 / 4.0, 1 / 4.0, 1 / 4.0, 1 / 4.0};

        compute_dijkstra_distance(mesh, options);

        auto dist = matrix_view(mesh.get_attribute<Scalar>(options.output_attribute_name));

        Scalar hdiag = std::sqrt(2) / 2.0;
        Scalar expected_dist[]{hdiag, hdiag, hdiag, hdiag, 1 + hdiag, 1 + hdiag};
        for (Index i = 0; i < 6; ++i) {
            REQUIRE(dist(i) == Catch::Approx(expected_dist[i]));
        }
    }
}
