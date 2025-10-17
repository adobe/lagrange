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
#include <lagrange/internal/fast_edge_sort.h>
#include <lagrange/internal/invert_mapping.h>
#include <lagrange/internal/visit_attribute.h>
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

namespace {

template <typename Index>
using InverseMapping = internal::InverseMapping<Index>;

// Remap an attribute. If multiple entries are mapped to the same slot, they will be averaged.
template <typename ValueType, typename Index>
void remap_average(
    Attribute<ValueType>& attr,
    const InverseMapping<Index>& new_to_old,
    Index num_out_elements)
{
    auto usage = attr.get_usage();
    if (usage == AttributeUsage::VertexIndex || usage == AttributeUsage::FacetIndex ||
        usage == AttributeUsage::CornerIndex || usage == AttributeUsage::EdgeIndex) {
        throw Error("remap_vertices cannot average indices!");
    }

    auto data = matrix_ref(attr);
    // A temp copy is necessary as we cannot make any assumptions about `old_to_new` order.
    Eigen::Matrix<ValueType, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> data_copy = data;

    for (Index i = 0; i < num_out_elements; i++) {
        data.row(i).setZero();
        for (Index j = new_to_old.offsets[i]; j < new_to_old.offsets[i + 1]; j++) {
            data.row(i) += data_copy.row(new_to_old.data[j]);
        }
        data.row(i) /= static_cast<ValueType>(new_to_old.offsets[i + 1] - new_to_old.offsets[i]);
    }
    attr.resize_elements(num_out_elements);
}

// Remap an attribute. If multiple entries are mapped to the same slot, keep the first.
template <typename ValueType, typename Index>
auto remap_keep_first(
    Attribute<ValueType>& attr,
    const InverseMapping<Index>& new_to_old,
    Index num_out_elements)
{
    auto data = matrix_ref(attr);
    // A temp copy is necessary as we cannot make any assumptions about `old_to_new` order.
    Eigen::Matrix<ValueType, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> data_copy = data;

    for (Index i = 0; i < num_out_elements; i++) {
        const Index j = new_to_old.offsets[i];
        data.row(i) = data_copy.row(new_to_old.data[j]);
    }
    attr.resize_elements(num_out_elements);
}

// Remap an attribute. If multiple entries are mapped to the same slot, throw error.
template <typename ValueType, typename Index>
auto remap_injective(
    Attribute<ValueType>& attr,
    const InverseMapping<Index>& new_to_old,
    Index num_out_elements)
{
    auto data = matrix_ref(attr);
    // A temp copy is necessary as we cannot make any assumptions about `old_to_new` order.
    Eigen::Matrix<ValueType, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> data_copy = data;

    for (Index i = 0; i < num_out_elements; i++) {
        const Index j = new_to_old.offsets[i];
        la_runtime_assert(
            new_to_old.offsets[i + 1] == j + 1,
            "Vertex mapping policy does not allow collision.");
        data.row(i) = data_copy.row(new_to_old.data[j]);
    }
    attr.resize_elements(num_out_elements);
}

template <typename ValueType, typename Index>
void remap_attribute(
    Attribute<ValueType>& attr,
    const InverseMapping<Index>& new_to_old,
    Index num_out_elements,
    RemapVerticesOptions options)
{
    if constexpr (std::is_integral_v<ValueType>) {
        switch (options.collision_policy_integral) {
        case MappingPolicy::Average: remap_average(attr, new_to_old, num_out_elements); break;
        case MappingPolicy::KeepFirst: remap_keep_first(attr, new_to_old, num_out_elements); break;
        case MappingPolicy::Error: remap_injective(attr, new_to_old, num_out_elements); break;
        default:
            throw Error(
                fmt::format(
                    "Unsupported integer collision policy {}",
                    static_cast<int>(options.collision_policy_integral)));
        }
    } else {
        switch (options.collision_policy_float) {
        case MappingPolicy::Average: remap_average(attr, new_to_old, num_out_elements); break;
        case MappingPolicy::KeepFirst: remap_keep_first(attr, new_to_old, num_out_elements); break;
        case MappingPolicy::Error: remap_injective(attr, new_to_old, num_out_elements); break;
        default:
            throw Error(
                fmt::format(
                    "Unsupported float collision policy {}",
                    static_cast<int>(options.collision_policy_float)));
        }
    }
}

} // namespace

template <typename Scalar, typename Index>
void remap_vertices(
    SurfaceMesh<Scalar, Index>& mesh,
    span<const Index> old_to_new,
    RemapVerticesOptions options)
{
    const Index num_vertices = mesh.get_num_vertices();
    la_runtime_assert((Index)old_to_new.size() == num_vertices);

    // Compute the backward order.
    auto new_to_old = internal::invert_mapping(old_to_new);
    la_debug_assert(new_to_old.offsets.back() == num_vertices);
    const Index num_out_vertices = static_cast<Index>(new_to_old.offsets.size() - 1);

    // Surjective check!
    for (Index i = 0; i < num_out_vertices; ++i) {
        la_runtime_assert(
            new_to_old.offsets[i] < new_to_old.offsets[i + 1],
            "old_to_new mapping is not surjective!");
    }

    std::vector<AttributeId> attr_with_edge_element_type;
    std::vector<AttributeId> attr_with_edge_index_usage;
    std::vector<internal::UnorientedEdge<Index>> old_edges;
    bool had_edges = mesh.has_edges();
    if (mesh.has_edges()) {
        // To preserve edge attributes we do the following:
        // 1. Turn every non-reserved attribute into a "value" attribute.
        // 2. Keep track of attributes with EdgeIndex usage tag and update their tag to `Scalar`.
        // 3. Clear edges to remove internal connectivity information (it's much easier/safer to
        //    rebuild it from scratch!)
        // 4. After vertex deletion, recompute edge connectivity.
        // 5. Resize out value attributes to the new number of edges, and remap/average values
        //    accordingly.
        // 6. Update their element type to "edge".

        seq_foreach_named_attribute_read(mesh, [&](std::string_view name, auto&& attr) {
            using AttributeType = std::decay_t<decltype(attr)>;
            using ValueType = typename AttributeType::ValueType;
            if (!mesh.attr_name_is_reserved(name)) {
                // Re-tag existing non-reserved edge attributes as "value" attributes.
                if (attr.get_element_type() == AttributeElement::Edge) {
                    auto id = mesh.get_attribute_id(name);
                    attr_with_edge_element_type.push_back(id);
                    auto& attr_ref = mesh.template ref_attribute<ValueType>(id);
                    attr_ref.unsafe_set_element_type(AttributeElement::Value);
                }
                // Keep track of attributes with EdgeIndex usage tag. Update tag to `Scalar` to
                // avoid all values being remapped to `0` by our call to `clear_edges()`.
                if (attr.get_usage() == AttributeUsage::EdgeIndex) {
                    auto id = mesh.get_attribute_id(name);
                    attr_with_edge_index_usage.push_back(id);
                    if constexpr (AttributeType::IsIndexed) {
                        auto& attr_ref = mesh.template ref_indexed_attribute<ValueType>(id);
                        attr_ref.unsafe_set_usage(AttributeUsage::Scalar);
                    } else {
                        auto& attr_ref = mesh.template ref_attribute<ValueType>(id);
                        attr_ref.unsafe_set_usage(AttributeUsage::Scalar);
                    }
                }
            }
        });

        old_edges.reserve(mesh.get_num_edges());
        for (Index e = 0; e < mesh.get_num_edges(); ++e) {
            auto v = mesh.get_edge_vertices(e);
            old_edges.emplace_back(old_to_new[v[0]], old_to_new[v[1]], e);
        }

        // No we can clear edges and remove reserved connectivity-related attributes.
        mesh.clear_edges();
    }

    // Remap per-vertex attributes.
    // TODO: We may want to cache the tmp copy buffers to avoid repeated allocations.
    par_foreach_named_attribute_write<AttributeElement::Vertex>(
        mesh,
        [&](std::string_view name, auto&& attr) {
            using AttributeType = std::decay_t<decltype(attr)>;
            using ValueType = typename AttributeType::ValueType;
            if (name == mesh.attr_name_vertex_to_first_corner() ||
                name == mesh.attr_name_next_corner_around_vertex()) {
                return;
            }

            remap_attribute<ValueType, Index>(attr, new_to_old, num_out_vertices, options);
        });

    // Update attributes with VertexIndex usage tag in other element types.
    par_foreach_named_attribute_read(mesh, [&](std::string_view name, auto&& attr) {
        using AttributeType = std::decay_t<decltype(attr)>;
        using ValueType = typename AttributeType::ValueType;
        if (attr.get_usage() == AttributeUsage::VertexIndex) {
            auto& attr_ref = mesh.template ref_attribute<ValueType>(name);
            auto data = attr_ref.ref_all();
            std::transform(data.begin(), data.end(), data.begin(), [&](ValueType i) {
                return static_cast<ValueType>(old_to_new[static_cast<size_t>(i)]);
            });
        }
    });

    // Remap vertices. This must be done last because `remove_vertices` also delete facets adjacent
    // to the vertices.
    //
    // TODO: we have already resized the vertex-to-position attribute, but there is no way of
    // changing `SurfaceMesh::m_num_vertices`. Using `SurfaceMesh::remove_vertices` works, but it
    // feels like an overkill and a fragile solution...
    mesh.remove_vertices([&](Index i) { return i >= num_out_vertices; });

    // Re-create connectivity
    if (had_edges) {
        mesh.initialize_edges();

        tbb::parallel_sort(old_edges.begin(), old_edges.end());

        std::vector<Index> old_to_new_edges(old_edges.size());

        Index new_num_edges = 0;
        for (auto it_begin = old_edges.begin(); it_begin != old_edges.end();) {
            Index edge_id = new_num_edges;
            // First the first edge after it_begin that has a different key
            auto it_end =
                std::find_if(it_begin, old_edges.end(), [&](auto e) { return (e != *it_begin); });
            // Process range of similar edges
            for (auto it = it_begin; it != it_end; ++it) {
                old_to_new_edges[it->id] = edge_id;
            }
            ++new_num_edges;
            it_begin = it_end;
        }
        auto new_to_old_edges = internal::invert_mapping<Index>(old_to_new_edges);

        // Remap values and resize per-edge attributes.
        for (auto id : attr_with_edge_element_type) {
            internal::visit_attribute_write(mesh, id, [&](auto&& attr) {
                using AttributeType = std::decay_t<decltype(attr)>;
                using ValueType = typename AttributeType::ValueType;

                if constexpr (AttributeType::IsIndexed) {
                    la_debug_assert(false, "Logic error in remap_vertices.");
                } else {
                    remap_attribute<ValueType, Index>(
                        attr,
                        new_to_old_edges,
                        mesh.get_num_edges(),
                        options);
                }

                attr.unsafe_set_element_type(AttributeElement::Edge);
            });
        }

        // Update attributes with EdgeIndex usage tag in other element types.
        for (auto id : attr_with_edge_index_usage) {
            internal::visit_attribute_write(mesh, id, [&](auto&& attr) {
                using AttributeType = std::decay_t<decltype(attr)>;
                using ValueType = typename AttributeType::ValueType;
                attr.unsafe_set_usage(AttributeUsage::EdgeIndex);
                auto data = [&]() -> decltype(auto) {
                    if constexpr (AttributeType::IsIndexed) {
                        return attr.values().ref_all();
                    } else {
                        return attr.ref_all();
                    }
                }();
                std::transform(data.begin(), data.end(), data.begin(), [&](ValueType i) {
                    return static_cast<ValueType>(old_to_new_edges[static_cast<size_t>(i)]);
                });
            });
        }
    }
}

#define LA_X_remap_vertices(_, Scalar, Index)                \
    template LA_CORE_API void remap_vertices<Scalar, Index>( \
        SurfaceMesh<Scalar, Index>&,                         \
        span<const Index>,                                   \
        RemapVerticesOptions);
LA_SURFACE_MESH_X(remap_vertices, 0)

} // namespace lagrange
