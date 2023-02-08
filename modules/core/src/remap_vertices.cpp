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
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/foreach_attribute.h>
#include <lagrange/remap_vertices.h>
#include <lagrange/utils/Error.h>
#include <lagrange/utils/assert.h>
#include <lagrange/views.h>

#include <Eigen/Core>

#include <algorithm>
#include <iterator>
#include <numeric>
#include <vector>

namespace lagrange {

template <typename Scalar, typename Index>
void remap_vertices(
    SurfaceMesh<Scalar, Index>& mesh,
    span<const Index> old_to_new,
    RemapVerticesOptions options)
{
    const Index num_vertices = mesh.get_num_vertices();
    la_runtime_assert((Index)old_to_new.size() == num_vertices);

    // The internal data structure for edges (e.g. $vertex_to_first_corner and
    // $next_corner_around_vertex) cannot be easily updated.
    if (mesh.has_edges()) {
        throw Error(
            "Remap vertices will invalidate edge data in mesh. Please clear_edges() first.");
    }

    // Compute the backward order.
    std::vector<Index> new_to_old_indices(num_vertices + 1, 0);
    std::vector<Index> new_to_old(num_vertices);
    for (Index i = 0; i < num_vertices; ++i) {
        Index j = old_to_new[i];
        la_runtime_assert(
            j < num_vertices,
            "New vertex index cannot exceeds existing number of vertices!");
        ++new_to_old_indices[j + 1];
    }
    size_t num_out_vertices = num_vertices;
    for (; num_out_vertices != 0 && new_to_old_indices[num_out_vertices] == 0; --num_out_vertices) {
    }
    new_to_old_indices.resize(num_out_vertices + 1);
    std::partial_sum(
        new_to_old_indices.begin(),
        new_to_old_indices.end(),
        new_to_old_indices.begin());
    la_debug_assert(new_to_old_indices.back() == num_vertices);

    for (Index i = 0; i < num_vertices; i++) {
        Index j = old_to_new[i];
        new_to_old[new_to_old_indices[j]++] = i;
    }
    std::rotate(
        new_to_old_indices.begin(),
        std::prev(new_to_old_indices.end()),
        new_to_old_indices.end());
    new_to_old_indices[0] = 0;

    // Surjective check!
    for (Index i = 0; i < num_out_vertices; i++) {
        la_runtime_assert(
            new_to_old_indices[i] < new_to_old_indices[i + 1],
            "old_to_new mapping is not surjective!");
    }

    // Remap an attribute.  If multiple entries are mapped to the same slot, they will be averaged.
    auto remap_average = [&](auto&& attr) {
        auto usage = attr.get_usage();
        if (usage == AttributeUsage::VertexIndex || usage == AttributeUsage::FacetIndex ||
            usage == AttributeUsage::CornerIndex) {
            throw Error("remap_vertices cannot average indices!");
        }

        auto data = matrix_ref(attr);
        using ValueType = typename std::decay_t<decltype(data)>::Scalar;
        // A temp copy is necessary as we cannot make any assumptions about `old_to_new` order.
        Eigen::Matrix<ValueType, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> data_copy = data;

        for (Index i = 0; i < num_out_vertices; i++) {
            data.row(i).setZero();
            for (Index j = new_to_old_indices[i]; j < new_to_old_indices[i + 1]; j++) {
                data.row(i) += data_copy.row(new_to_old[j]);
            }
            data.row(i) /=
                static_cast<ValueType>(new_to_old_indices[i + 1] - new_to_old_indices[i]);
        }
        attr.resize_elements(num_out_vertices);
    };

    // Remap an attribute.  If multiple entries are mapped to the same slot, keep the first.
    auto remap_keep_first = [&](auto&& attr) {
        auto data = matrix_ref(attr);
        using ValueType = typename std::decay_t<decltype(data)>::Scalar;
        // A temp copy is necessary as we cannot make any assumptions about `old_to_new` order.
        Eigen::Matrix<ValueType, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> data_copy = data;

        for (Index i = 0; i < num_out_vertices; i++) {
            const Index j = new_to_old_indices[i];
            data.row(i) = data_copy.row(new_to_old[j]);
        }
        attr.resize_elements(num_out_vertices);
    };

    // Remap an attribute.  If multiple entries are mapped to the same slot, throw error.
    auto remap_injective = [&](auto&& attr) {
        auto data = matrix_ref(attr);
        using ValueType = typename std::decay_t<decltype(data)>::Scalar;
        // A temp copy is necessary as we cannot make any assumptions about `old_to_new` order.
        Eigen::Matrix<ValueType, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> data_copy = data;

        for (Index i = 0; i < num_out_vertices; i++) {
            const Index j = new_to_old_indices[i];
            la_runtime_assert(
                new_to_old_indices[i + 1] == j + 1,
                "Vertex mapping policy does not allow collision.");
            data.row(i) = data_copy.row(new_to_old[j]);
        }
        attr.resize_elements(num_out_vertices);
    };

    // Remap vertex attributes.
    par_foreach_named_attribute_write<AttributeElement::Vertex>(
        mesh,
        [&](std::string_view name, auto&& attr) {
            using AttributeType = std::decay_t<decltype(attr)>;
            using ValueType = typename AttributeType::ValueType;
            if (name == mesh.attr_name_vertex_to_first_corner() ||
                name == mesh.attr_name_next_corner_around_vertex())
                return;

            if constexpr (std::is_integral_v<ValueType>) {
                switch (options.collision_policy_integral) {
                case RemapVerticesOptions::CollisionPolicy::Average:
                    remap_average(std::forward<AttributeType>(attr));
                    break;
                case RemapVerticesOptions::CollisionPolicy::KeepFirst:
                    remap_keep_first(std::forward<AttributeType>(attr));
                    break;
                case RemapVerticesOptions::CollisionPolicy::Error:
                    remap_injective(std::forward<AttributeType>(attr));
                    break;
                default:
                    throw Error(fmt::format(
                        "Unsupported collision policy {}",
                        int(options.collision_policy_integral)));
                }
            } else {
                switch (options.collision_policy_float) {
                case RemapVerticesOptions::CollisionPolicy::Average:
                    remap_average(std::forward<AttributeType>(attr));
                    break;
                case RemapVerticesOptions::CollisionPolicy::KeepFirst:
                    remap_keep_first(std::forward<AttributeType>(attr));
                    break;
                case RemapVerticesOptions::CollisionPolicy::Error:
                    remap_injective(std::forward<AttributeType>(attr));
                    break;
                default:
                    throw Error(fmt::format(
                        "Unsupported collision policy {}",
                        int(options.collision_policy_float)));
                }
            }
        });

    // Update vertex indices.
    par_foreach_named_attribute_read(mesh, [&](std::string_view name, auto&& attr) {
        using AttributeType = std::decay_t<decltype(attr)>;
        using ValueType = typename AttributeType::ValueType;
        // Only remap vertex indices that are not associated with vertex element because vertex
        // attributes have already been updated in previous step.
        if (attr.get_usage() == AttributeUsage::VertexIndex &&
            attr.get_element_type() != AttributeElement::Vertex) {
            auto& attr_ref = mesh.template ref_attribute<ValueType>(name);
            auto data = attr_ref.ref_all();
            std::transform(data.begin(), data.end(), data.begin(), [&](ValueType i) {
                return static_cast<ValueType>(old_to_new[static_cast<size_t>(i)]);
            });
        }
    });

    // Remap vertices.  This must be done last because `remove_vertices` also delete facets adjacent
    // to the vertices.
    //
    // TODO: we has already resized the vertex-to-position attribute, but there is no way of
    // changing `SurfaceMesh::m_num_vertices`. Using `SurfaceMesh::remove_vertices` works, but it
    // feels like an overkill and a fragile solution...
    mesh.remove_vertices([&](Index i) { return i >= num_out_vertices; });
}

#define LA_X_remap_vertices(_, Scalar, Index)    \
    template void remap_vertices<Scalar, Index>( \
        SurfaceMesh<Scalar, Index>&,             \
        span<const Index>,                       \
        RemapVerticesOptions);
LA_SURFACE_MESH_X(remap_vertices, 0)

} // namespace lagrange
