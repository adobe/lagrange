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
#pragma once

#include <exception>
#include <iostream>

#include <lagrange/ExactPredicates.h>
#include <lagrange/Mesh.h>
#include <lagrange/MeshTrait.h>
#include <lagrange/common.h>

namespace lagrange {

/**
 * Compute a per-facet attribute indicating whether the given facet is
 * degenerate.
 *
 * Arguments:
 *   mesh: A Mesh pointer.
 *
 * Returns:
 *   Nothing, but a facet attribute named "is_degenerate" is added to the
 *   mesh.
 */
template <typename MeshType>
void detect_degenerate_triangles(MeshType& mesh)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");
    using Index = typename MeshType::Index;
    if (mesh.get_vertex_per_facet() != 3) {
        throw std::runtime_error("Input mesh is not a triangle mesh.");
    }

    auto predicates = ExactPredicates::create("shewchuk");
    auto is_degenerate_2D = [&predicates](double p1[2], double p2[2], double p3[3]) -> bool {
        return predicates->orient2D(p1, p2, p3) == 0;
    };
    auto is_degenerate_3D = [&predicates](double p1[3], double p2[3], double p3[3]) -> bool {
        double p1xy[] = {p1[0], p1[1]};
        double p1yz[] = {p1[1], p1[2]};
        double p1zx[] = {p1[2], p1[0]};
        double p2xy[] = {p2[0], p2[1]};
        double p2yz[] = {p2[1], p2[2]};
        double p2zx[] = {p2[2], p2[0]};
        double p3xy[] = {p3[0], p3[1]};
        double p3yz[] = {p3[1], p3[2]};
        double p3zx[] = {p3[2], p3[0]};
        return predicates->orient2D(p1xy, p2xy, p3xy) == 0 &&
               predicates->orient2D(p1yz, p2yz, p3yz) == 0 &&
               predicates->orient2D(p1zx, p2zx, p3zx) == 0;
    };
    const Index dim = mesh.get_dim();
    const Index num_facets = mesh.get_num_facets();
    typename MeshType::AttributeArray is_degenerate(num_facets, 1);
    is_degenerate.setConstant(0.0);

    const auto& vertices = mesh.get_vertices();
    const auto& facets = mesh.get_facets();
    if (dim == 3) {
        for (Index i = 0; i < num_facets; i++) {
            auto f = facets.row(i).eval();
            double p1[3]{vertices(f[0], 0), vertices(f[0], 1), vertices(f[0], 2)};
            double p2[3]{vertices(f[1], 0), vertices(f[1], 1), vertices(f[1], 2)};
            double p3[3]{vertices(f[2], 0), vertices(f[2], 1), vertices(f[2], 2)};
            is_degenerate(i, 0) = is_degenerate_3D(p1, p2, p3);
        }
    } else if (dim == 2) {
        for (Index i = 0; i < num_facets; i++) {
            auto f = facets.row(i).eval();
            double p1[2]{vertices(f[0], 0), vertices(f[0], 1)};
            double p2[2]{vertices(f[1], 0), vertices(f[1], 1)};
            double p3[2]{vertices(f[2], 0), vertices(f[2], 1)};
            is_degenerate(i, 0) = is_degenerate_2D(p1, p2, p3);
        }
    }

    mesh.add_facet_attribute("is_degenerate");
    mesh.import_facet_attribute("is_degenerate", is_degenerate);
}
} // namespace lagrange
