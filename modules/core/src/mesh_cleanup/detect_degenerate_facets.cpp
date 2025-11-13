/*
 * Copyright 2023 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */

#include <lagrange/Attribute.h>
#include <lagrange/ExactPredicatesShewchuk.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/mesh_cleanup/detect_degenerate_facets.h>
#include <lagrange/utils/assert.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <tbb/concurrent_vector.h>
#include <tbb/parallel_for.h>
#include <tbb/parallel_sort.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <array>

namespace lagrange {

template <typename Scalar, typename Index>
std::vector<Index> detect_degenerate_facets(const SurfaceMesh<Scalar, Index>& mesh)
{
    ExactPredicatesShewchuk predicates;
    auto is_degenerate_2D = [&predicates](double p1[2], double p2[2], double p3[3]) -> bool {
        return predicates.orient2D(p1, p2, p3) == 0;
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
        return predicates.orient2D(p1xy, p2xy, p3xy) == 0 &&
               predicates.orient2D(p1yz, p2yz, p3yz) == 0 &&
               predicates.orient2D(p1zx, p2zx, p3zx) == 0;
    };

    tbb::concurrent_vector<Index> result;
    const auto dim = mesh.get_dimension();
    const auto num_facets = mesh.get_num_facets();

    if (dim == 2) {
        const auto& vertices = mesh.get_vertex_to_position();
        tbb::parallel_for(Index(0), num_facets, [&](Index i) {
            Index vertex_per_facet = mesh.get_facet_size(i);
            la_debug_assert(vertex_per_facet >= 3);

            Index v0 = mesh.get_facet_vertex(i, 0);
            std::array<double, 2> p0, p1, p2;
            p0 = {
                static_cast<double>(vertices.get(v0, 0)),
                static_cast<double>(vertices.get(v0, 1))};

            bool degenerate = true;
            for (Index j = 1; j < vertex_per_facet - 1; j++) {
                Index v1 = mesh.get_facet_vertex(i, j);
                Index v2 = mesh.get_facet_vertex(i, j + 1);
                p1 = {
                    static_cast<double>(vertices.get(v1, 0)),
                    static_cast<double>(vertices.get(v1, 1))};
                p2 = {
                    static_cast<double>(vertices.get(v2, 0)),
                    static_cast<double>(vertices.get(v2, 1))};
                if (!is_degenerate_2D(p0.data(), p1.data(), p2.data())) {
                    degenerate = false;
                    break;
                }
            }
            if (degenerate) {
                result.push_back(i);
            }
        });
    } else {
        la_runtime_assert(dim == 3, "Only 2D and 3D meshes are supported!");
        const auto& vertices = mesh.get_vertex_to_position();
        tbb::parallel_for(Index(0), num_facets, [&](Index i) {
            Index vertex_per_facet = mesh.get_facet_size(i);
            la_debug_assert(vertex_per_facet >= 3);

            Index v0 = mesh.get_facet_vertex(i, 0);
            std::array<double, 3> p0, p1, p2;
            p0 = {
                static_cast<double>(vertices.get(v0, 0)),
                static_cast<double>(vertices.get(v0, 1)),
                static_cast<double>(vertices.get(v0, 2))};

            bool degenerate = true;
            for (Index j = 1; j < vertex_per_facet - 1; j++) {
                Index v1 = mesh.get_facet_vertex(i, j);
                Index v2 = mesh.get_facet_vertex(i, j + 1);
                p1 = {
                    static_cast<double>(vertices.get(v1, 0)),
                    static_cast<double>(vertices.get(v1, 1)),
                    static_cast<double>(vertices.get(v1, 2))};
                p2 = {
                    static_cast<double>(vertices.get(v2, 0)),
                    static_cast<double>(vertices.get(v2, 1)),
                    static_cast<double>(vertices.get(v2, 2))};
                if (!is_degenerate_3D(p0.data(), p1.data(), p2.data())) {
                    degenerate = false;
                    break;
                }
            }
            if (degenerate) {
                result.push_back(i);
            }
        });
    }
    tbb::parallel_sort(result.begin(), result.end());
    std::vector<Index> degenerate_facets(result.begin(), result.end());
    return degenerate_facets;
}

#define LA_X_detect_degenerate_facets(_, Scalar, Index)                              \
    template LA_CORE_API std::vector<Index> detect_degenerate_facets<Scalar, Index>( \
        const SurfaceMesh<Scalar, Index>&);
LA_SURFACE_MESH_X(detect_degenerate_facets, 0)

} // namespace lagrange
