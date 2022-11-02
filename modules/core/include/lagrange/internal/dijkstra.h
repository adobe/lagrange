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
#pragma once

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
    #include <lagrange/legacy/internal/dijkstra.h>
#endif

#include <lagrange/SurfaceMesh.h>
#include <lagrange/utils/function_ref.h>
#include <lagrange/utils/span.h>

namespace lagrange::internal {

/**
 * Traverse the mesh based on Dijkstra's algorithm with customized distance
 * metric and process functions.
 *
 * @tparam Scalar  Mesh scalar type
 * @tparam Index   Mesh index type
 *
 * @param mesh              The input mesh.
 * @param seed_vertices     Seed vertices.
 * @param seed_vertex_dist  Initial distance to the seed vertices.
 * @param radius            The radius of the search. Radius <= 0 denotes the search is over the
 *                          entire mesh.
 * @param dist              The distance metric.  e.g. `d = dist(v0, v1)`
 * @param process           Call back function to process each new vertex reached.  Its return type
 *                          indicates whether the search is done.
 *                          e.g. `done = process(vid, v_dist)`
 */
template <typename Scalar, typename Index>
void dijkstra(
    SurfaceMesh<Scalar, Index>& mesh,
    span<const Index> seed_vertices,
    span<const Scalar> seed_vertex_dist,
    Scalar radius,
    const function_ref<Scalar(Index, Index)>& dist,
    const function_ref<bool(Index, Scalar)>& process);

} // namespace lagrange::internal
