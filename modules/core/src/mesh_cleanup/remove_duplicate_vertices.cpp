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

#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/foreach_attribute.h>
#include <lagrange/mesh_cleanup/remove_duplicate_vertices.h>
#include <lagrange/remap_vertices.h>
#include <lagrange/utils/Error.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <tbb/parallel_sort.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <algorithm>
#include <numeric>
#include <vector>

namespace lagrange {

template <typename Scalar, typename Index>
void remove_duplicate_vertices(
    SurfaceMesh<Scalar, Index>& mesh,
    const RemoveDuplicateVerticesOptions& options)
{
    // Step 0: Some sanity check.
    for (const auto& id : options.extra_attributes) {
        const auto& attr = mesh.get_attribute_base(id);
        la_runtime_assert(
            attr.get_element_type() == AttributeElement::Vertex,
            "Only vertex attribute are supported.");
        la_runtime_assert(
            mesh.template is_attribute_type<Scalar>(id),
            "Attribute type must be Scalar.");
    }

    // Step 1: Sort vertices with custom comp.
    const auto num_vertices = mesh.get_num_vertices();
    std::vector<Index> order(num_vertices);
    std::iota(order.begin(), order.end(), 0);

    auto compare_vertex_attr = [&](Index vi, Index vj, AttributeId id) -> short {
        const auto& attr = mesh.template get_attribute<Scalar>(id);
        la_debug_assert(attr.get_element_type() == AttributeElement::Vertex);
        const Index num_channels = static_cast<Index>(attr.get_num_channels());
        for (Index i = 0; i < num_channels; i++) {
            if (attr.get(vi, i) < attr.get(vj, i)) {
                return -1;
            }
            if (attr.get(vi, i) > attr.get(vj, i)) {
                return 1;
            }
        }
        return 0;
    };

    auto compare_corner_attr = [&](Index vi, Index vj, AttributeId id) -> short {
        const auto& attr = mesh.template get_attribute<Scalar>(id);
        la_debug_assert(attr.get_element_type() == AttributeElement::Corner);
        const Index num_channels = static_cast<Index>(attr.get_num_channels());
        // It is possible for a vertex to have multiple values at different corners.
        // We will use their average corner value for the comparison.
        std::vector<Scalar> ave_value_i(num_channels, 0);
        std::vector<Scalar> ave_value_j(num_channels, 0);
        mesh.foreach_corner_around_vertex(vi, [&](Index ci) {
            la_debug_assert(mesh.get_corner_vertex(ci) == vi);
            for (Index i = 0; i < num_channels; i++) ave_value_i[i] += attr.get(ci, i);
        });
        mesh.foreach_corner_around_vertex(vj, [&](Index cj) {
            la_debug_assert(mesh.get_corner_vertex(cj) == vj);
            for (Index j = 0; j < num_channels; j++) ave_value_j[j] += attr.get(cj, j);
        });
        std::for_each(ave_value_i.begin(), ave_value_i.end(), [&](Scalar& val) {
            val /= mesh.count_num_corners_around_vertex(vi);
        });
        std::for_each(ave_value_j.begin(), ave_value_j.end(), [&](Scalar& val) {
            val /= mesh.count_num_corners_around_vertex(vj);
        });
        for (Index i = 0; i < num_channels; i++) {
            if (ave_value_i[i] < ave_value_j[i]) {
                return -1;
            } else if (ave_value_i[i] > ave_value_j[i]) {
                return 1;
            }
        }
        return 0;
    };

    auto compare_indexed_attr = [&](Index vi, Index vj, AttributeId id) -> short {
        const auto& attr = mesh.template get_indexed_attribute<Scalar>(id);
        const auto& values = attr.values();
        const auto& indices = attr.indices();
        const Index num_channels = static_cast<Index>(attr.get_num_channels());
        // It is possible for a vertex to have multiple values at different corners.
        // We will use their average corner value for the comparison.
        std::vector<Scalar> ave_value_i(num_channels, 0);
        std::vector<Scalar> ave_value_j(num_channels, 0);
        mesh.foreach_corner_around_vertex(vi, [&](Index ci) {
            la_debug_assert(mesh.get_corner_vertex(ci) == vi);
            for (Index i = 0; i < num_channels; i++)
                ave_value_i[i] += values.get(indices.get(ci), i);
        });
        mesh.foreach_corner_around_vertex(vj, [&](Index cj) {
            la_debug_assert(mesh.get_corner_vertex(cj) == vj);
            for (Index j = 0; j < num_channels; j++)
                ave_value_j[j] += values.get(indices.get(cj), j);
        });
        std::for_each(ave_value_i.begin(), ave_value_i.end(), [&](Scalar& val) {
            val /= mesh.count_num_corners_around_vertex(vi);
        });
        std::for_each(ave_value_j.begin(), ave_value_j.end(), [&](Scalar& val) {
            val /= mesh.count_num_corners_around_vertex(vj);
        });
        for (Index i = 0; i < num_channels; i++) {
            if (ave_value_i[i] < ave_value_j[i]) {
                return -1;
            } else if (ave_value_i[i] > ave_value_j[i]) {
                return 1;
            }
        }
        return 0;
    };

    auto compare_vertices = [&](Index vi, Index vj) -> short {
        auto result = compare_vertex_attr(vi, vj, mesh.attr_id_vertex_to_positions());
        if (result == 0) {
            for (const auto& id : options.extra_attributes) {
                const auto& attr = mesh.get_attribute_base(id);
                switch (attr.get_element_type()) {
                case AttributeElement::Vertex: result = compare_vertex_attr(vi, vj, id); break;
                case AttributeElement::Corner: result = compare_corner_attr(vi, vj, id); break;
                case AttributeElement::Indexed: result = compare_indexed_attr(vi, vj, id); break;
                default: throw Error("Only vertex/corner/indexed attributes are supported.");
                }
                if (result != 0) break;
            }
        }
        return result;
    };

    tbb::parallel_sort(order.begin(), order.end(), [&](Index vi, Index vj) {
        return compare_vertices(vi, vj) < 0;
    });

    // Step 2: Extract unique vertices.
    std::vector<Index> old_to_new(num_vertices);
    Index new_vertex_count = 0;
    for (Index i = 0; i < num_vertices; i++) {
        Index vi = order[i];
        old_to_new[vi] = new_vertex_count;
        for (Index j = i + 1; j < num_vertices; j++) {
            Index vj = order[j];
            if (compare_vertices(vi, vj) == 0) {
                old_to_new[vj] = new_vertex_count;
                i++;
            } else {
                break;
            }
        }
        new_vertex_count++;
    }

    // Step 3: Use remap_vertices to combine duplicate vertices.
    remap_vertices<Scalar, Index>(mesh, old_to_new);
}

#define LA_X_remove_duplicate_vertices(_, Scalar, Index)    \
    template void remove_duplicate_vertices<Scalar, Index>( \
        SurfaceMesh<Scalar, Index>&,                        \
        const RemoveDuplicateVerticesOptions&);
LA_SURFACE_MESH_X(remove_duplicate_vertices, 0)

} // namespace lagrange
