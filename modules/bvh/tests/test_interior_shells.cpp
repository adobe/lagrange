/*
 * Copyright 2025 Adobe. All rights reserved.
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

#include <lagrange/bvh/remove_interior_shells.h>
#include <lagrange/combine_meshes.h>
#include <lagrange/primitive/generate_rounded_cube.h>
#include <lagrange/reorder_mesh.h>
#include <lagrange/views.h>

TEST_CASE("remove_interior_shells", "[bvh][surface]")
{
    using Scalar = float;
    using Index = uint32_t;

    auto make_cube = [](std::array<Scalar, 3> center, std::array<Scalar, 3> size) {
        lagrange::primitive::RoundedCubeOptions options;
        options.center = center;
        options.width = size[0];
        options.height = size[1];
        options.depth = size[2];
        options.triangulate = true;
        return lagrange::primitive::generate_rounded_cube<Scalar, Index>(options);
    };

    std::vector<lagrange::SurfaceMesh<Scalar, Index>> shells;
    shells.push_back(make_cube({-1, -1, 0}, {1.f, 1.f, 1.f}));
    shells.push_back(make_cube({1, -1, 0}, {1.f, 1.f, 1.f}));
    shells.push_back(make_cube({1, 1, 0}, {1.f, 1.f, 1.f}));
    shells.push_back(make_cube({-1, 1, 0}, {1.f, 1.f, 1.f}));
    shells.push_back(make_cube({3, -1, 0}, {1.f, 1.f, 1.f}));
    shells.push_back(make_cube({3, 1, 0}, {1.f, 1.f, 1.f}));
    shells.push_back(make_cube({0, -1, 0}, {3.1f, 1.1f, 1.1f}));
    shells.push_back(make_cube({0, 0, 0}, {3.2f, 3.2f, 3.2f}));
    shells.push_back(make_cube({3, 0, 0}, {1.1f, 3.1f, 1.1f}));

    auto mesh = lagrange::combine_meshes<Scalar, Index>(shells);

    auto expected = lagrange::combine_meshes<Scalar, Index>({&shells[7], &shells[8]});
    auto cleaned = lagrange::bvh::remove_interior_shells(mesh);

    lagrange::reorder_mesh(cleaned, lagrange::ReorderingMethod::Lexicographic);
    lagrange::reorder_mesh(expected, lagrange::ReorderingMethod::Lexicographic);

    REQUIRE(vertex_view(cleaned) == vertex_view(expected));
    REQUIRE(facet_view(cleaned) == facet_view(expected));
}
