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

#include <lagrange/orientation.h>

#include <lagrange/Attribute.h>
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/internal/find_attribute_utils.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <tbb/parallel_for.h>
#include <tbb/parallel_reduce.h>
#include <lagrange/utils/warnon.h>
// clang-format on

namespace lagrange {

namespace {

template <typename Scalar, typename Index>
bool is_edge_oriented(const SurfaceMesh<Scalar, Index>& mesh, Index eid)
{
    Index num_around = 0;
    Index num_forward = 0;
    Index num_backward = 0;
    auto v = mesh.get_edge_vertices(eid);
    mesh.foreach_corner_around_edge(eid, [&](Index c) {
        Index v0 = mesh.get_corner_vertex(c);
        if (v0 == v[0]) {
            ++num_forward;
        } else {
            ++num_backward;
        }
        ++num_around;
    });
    if (num_around <= 1) {
        return true;
    } else {
        return num_forward == num_backward;
    }
}

} // namespace

template <typename Scalar, typename Index>
bool is_oriented(const SurfaceMesh<Scalar, Index>& mesh)
{
    if (!mesh.has_edges()) {
        auto mesh_copy = mesh;
        mesh_copy.initialize_edges();
        return is_oriented(mesh_copy);
    }

    return tbb::parallel_reduce(
        tbb::blocked_range<Index>(0, mesh.get_num_edges()),
        true, ///< initial value of the result.
        [&](const tbb::blocked_range<Index>& r, bool oriented) -> bool {
            if (!oriented) return false;

            for (Index ei = r.begin(); ei != r.end(); ei++) {
                if (!is_edge_oriented(mesh, ei)) {
                    return false;
                }
            }
            return true;
        },
        std::logical_and<bool>());
}

template <typename Scalar, typename Index>
AttributeId compute_edge_is_oriented(
    SurfaceMesh<Scalar, Index>& mesh,
    const OrientationOptions& options)
{
    AttributeId id = internal::find_or_create_attribute<uint8_t>(
        mesh,
        options.output_attribute_name,
        Edge,
        AttributeUsage::Scalar,
        1,
        internal::ResetToDefault::No);
    auto attr = mesh.template ref_attribute<uint8_t>(id).ref_all();

    tbb::parallel_for(Index(0), mesh.get_num_edges(), [&](Index ei) {
        attr[ei] = is_edge_oriented(mesh, ei);
    });

    return id;
}

#define LA_X_topology(_, Scalar, Index)                                                           \
    template LA_CORE_API bool is_oriented<Scalar, Index>(const SurfaceMesh<Scalar, Index>& mesh); \
    template LA_CORE_API AttributeId compute_edge_is_oriented(                                    \
        SurfaceMesh<Scalar, Index>& mesh,                                                         \
        const OrientationOptions& options);
LA_SURFACE_MESH_X(topology, 0)

} // namespace lagrange
