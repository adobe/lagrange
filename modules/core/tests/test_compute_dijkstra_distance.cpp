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

#include <lagrange/Mesh.h>
#include <lagrange/common.h>
#include <lagrange/compute_dijkstra_distance.h>
#include <lagrange/create_mesh.h>

TEST_CASE("DijkstraDistance", "[dijstra][triangle]")
{
    using namespace lagrange;

    SECTION("cube")
    {
        auto cube = create_cube();
        compute_dijkstra_distance(*cube, 0, {1.0, 0.0, 0.0}, 0.0);

        REQUIRE(cube->has_vertex_attribute("dijkstra_distance"));
        const auto& dist = cube->get_vertex_attribute("dijkstra_distance");
        REQUIRE(dist.minCoeff() == Catch::Approx(0.0));
        REQUIRE(dist.maxCoeff() <= Catch::Approx(6.0));
    }

    SECTION("sphere")
    {
        auto sphere = create_sphere(4);
        compute_dijkstra_distance(*sphere, 0, {1.0, 0.0, 0.0}, 0.0);

        REQUIRE(sphere->has_vertex_attribute("dijkstra_distance"));
        const auto& dist = sphere->get_vertex_attribute("dijkstra_distance");
        REQUIRE(dist.minCoeff() == Catch::Approx(0.0));
        REQUIRE(dist.maxCoeff() == Catch::Approx(M_PI).epsilon(0.1));
    }
}
