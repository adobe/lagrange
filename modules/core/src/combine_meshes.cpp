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
#include <lagrange/combine_meshes.h>

#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/foreach_attribute.h>
#include <lagrange/utils/assert.h>
#include <lagrange/views.h>

#include <algorithm>

namespace lagrange {

namespace {

template <typename Scalar, typename Index, typename ValueType>
bool validate_attribute_metadata(
    size_t num_meshes,
    function_ref<const SurfaceMesh<Scalar, Index>(size_t)> get_mesh,
    std::string_view name,
    AttributeElement target_element,
    size_t target_num_channels,
    AttributeUsage target_usage)
{
    for (size_t i = 0; i < num_meshes; ++i) {
        const auto& mesh = get_mesh(i);
        if (!mesh.has_attribute(name)) {
            logger().warn("combine_meshes: not all meshes contain attribute {}, skipping!", name);
            return false;
        }
        if (!mesh.template is_attribute_type<ValueType>(name)) {
            logger().warn("combine_meshes: attribute {} has inconsistent ValueType.", name);
            return false;
        }
        const auto& local_attr = mesh.get_attribute_base(name);
        if (local_attr.get_usage() != target_usage) {
            logger().warn("combine_meshes: attribute {} has inconsistent usage!", name);
            return false;
        }
        if (local_attr.get_element_type() != target_element) {
            logger().warn("combine_meshes: attribute {} has inconsistent element type!", name);
            return false;
        }
        if (local_attr.get_num_channels() != target_num_channels) {
            logger().warn(
                "combine_meshes: attribute {} has inconsistent number of channels. Skipping!",
                name);
            return false;
        }
    }
    return true;
}

template <typename Scalar, typename Index>
Index offset_from_usage(const SurfaceMesh<Scalar, Index>& mesh, AttributeUsage usage)
{
    switch (usage) {
    case AttributeUsage::VertexIndex: return mesh.get_num_vertices();
    case AttributeUsage::FacetIndex: return mesh.get_num_facets();
    case AttributeUsage::CornerIndex: return mesh.get_num_corners();
    case AttributeUsage::EdgeIndex: return mesh.get_num_edges();
    default: return Index(0);
    }
};

template <typename Scalar, typename Index>
void combine_attributes(
    size_t num_meshes,
    function_ref<const SurfaceMesh<Scalar, Index>&(size_t)> get_mesh,
    SurfaceMesh<Scalar, Index>& out_mesh)
{
    la_debug_assert(num_meshes > 0);

    auto resize_value_attribute = [&](auto&& attr, auto mesh_to_attr) {
        size_t num_elements = 0;
        for (size_t i = 0; i < num_meshes; ++i) {
            const auto& local_attr = mesh_to_attr(i);
            num_elements += local_attr.get_num_elements();
        }
        attr.resize_elements(num_elements);
    };

    auto combine_attribute = [&](std::string_view name, auto&& attr) {
        using AttributeType = std::decay_t<decltype(attr)>;
        using ValueType = typename AttributeType::ValueType;

        AttributeUsage usage = attr.get_usage();

        if (attr.get_element_type() == Value) {
            resize_value_attribute(attr, [&](auto i) -> decltype(auto) {
                return get_mesh(i).template get_attribute<ValueType>(name);
            });
        }

        auto attr_view = matrix_ref(attr);
        size_t element_count = 0;
        Index offset = 0;
        for (size_t i = 0; i < num_meshes; ++i) {
            const auto& mesh = get_mesh(i);
            la_debug_assert(!mesh.is_attribute_indexed(name));
            const auto& local_attr = mesh.template get_attribute<ValueType>(name);
            auto local_view = matrix_view(local_attr);
            attr_view.middleRows(element_count, local_view.rows()) =
                local_view.array() + static_cast<ValueType>(offset);
            element_count += local_view.rows();
            offset += offset_from_usage(mesh, usage);
        }
    };

    auto combine_indexed_attribute = [&](std::string_view name, auto&& attr) {
        using AttributeType = std::decay_t<decltype(attr)>;
        using ValueType = typename AttributeType::ValueType;

        AttributeUsage usage = attr.get_usage();
        auto& value_attr = attr.values();
        auto& index_attr = attr.indices();
        la_debug_assert(index_attr.get_num_elements() == out_mesh.get_num_corners());

        resize_value_attribute(value_attr, [&](auto i) -> decltype(auto) {
            return get_mesh(i).template get_indexed_attribute<ValueType>(name).values();
        });

        auto value_view = matrix_ref(value_attr);
        auto index_view = matrix_ref(index_attr);
        size_t value_count = 0;
        size_t index_count = 0;
        Index offset = 0;
        for (size_t i = 0; i < num_meshes; ++i) {
            const auto& mesh = get_mesh(i);
            const auto& local_attr = mesh.template get_indexed_attribute<ValueType>(name);
            auto local_value_view = matrix_view(local_attr.values());
            auto local_index_view = matrix_view(local_attr.indices());
            value_view.middleRows(value_count, local_value_view.rows()) =
                local_value_view.array() + static_cast<ValueType>(offset);
            index_view.middleRows(index_count, local_index_view.rows()) =
                local_index_view.array() + static_cast<Index>(value_count);
            value_count += local_value_view.rows();
            index_count += local_index_view.rows();
            offset += offset_from_usage(mesh, usage);
        }
    };

    // When combining meshes with edge attributes, we cannot assume the new default edge ordering
    // will be compatible with the user-provided mesh edges. So we need to specify explicitly the
    // new edge ordering for the combined mesh.
    auto combined_edge_vertices = [&]() {
        size_t num_edges = 0;
        for (size_t i = 0; i < num_meshes; ++i) {
            num_edges += get_mesh(i).get_num_edges();
        }
        std::vector<std::array<Index, 2>> edges;
        edges.reserve(num_edges);
        Index current_v = 0;
        for (size_t i = 0; i < num_meshes; ++i) {
            const auto& mesh = get_mesh(i);
            for (Index e = 0; e < mesh.get_num_edges(); ++e) {
                auto v = mesh.get_edge_vertices(e);
                edges.push_back({v[0] + current_v, v[1] + current_v});
            }
            current_v += mesh.get_num_vertices();
        }
        return edges;
    };

    seq_foreach_named_attribute_read(get_mesh(0), [&](std::string_view name, const auto& attr) {
        if (SurfaceMesh<Scalar, Index>::attr_name_is_reserved(name)) return;

        using AttributeType = std::decay_t<decltype(attr)>;
        using ValueType = typename AttributeType::ValueType;

        AttributeElement element = attr.get_element_type();
        AttributeUsage usage = attr.get_usage();
        size_t num_channels = attr.get_num_channels();

        if (!validate_attribute_metadata<Scalar, Index, ValueType>(
                num_meshes,
                get_mesh,
                name,
                element,
                num_channels,
                usage)) {
            return;
        }
        if (element == Edge && !out_mesh.has_edges()) {
            auto edges = combined_edge_vertices();
            out_mesh.initialize_edges({&edges[0][0], 2 * edges.size()});
        }

        auto id = out_mesh.template create_attribute<ValueType>(name, element, usage, num_channels);

        if constexpr (AttributeType::IsIndexed) {
            auto& out_attr = out_mesh.template ref_indexed_attribute<ValueType>(id);
            combine_indexed_attribute(name, out_attr);
        } else {
            auto& out_attr = out_mesh.template ref_attribute<ValueType>(id);
            combine_attribute(name, out_attr);
        }
    });
}

} // namespace

template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> combine_meshes(
    size_t num_meshes,
    function_ref<const SurfaceMesh<Scalar, Index>&(size_t)> get_mesh,
    bool preserve_attributes)
{
    if (num_meshes == 0) return SurfaceMesh<Scalar, Index>{};

    // Count combined mesh size
    Index total_num_vertices = 0;
    Index total_num_facets = 0;
    Index total_num_corners = 0;
    Index vertex_per_facet = 0;
    Index dimension = 0;
    bool is_regular = true;
    for (size_t i = 0; i < num_meshes; ++i) {
        const auto& mesh = get_mesh(i);
        if (dimension == 0) {
            dimension = mesh.get_dimension();
        } else if (mesh.get_dimension() != dimension) {
            throw std::runtime_error("combine_meshes: Incompatible mesh dimensions");
        }
        total_num_vertices += mesh.get_num_vertices();
        total_num_facets += mesh.get_num_facets();
        total_num_corners += mesh.get_num_corners();
        if (is_regular && mesh.is_regular()) {
            if (vertex_per_facet == 0) {
                vertex_per_facet = mesh.get_vertex_per_facet();
            } else if (mesh.get_vertex_per_facet() != vertex_per_facet) {
                is_regular = false;
            }
        } else {
            is_regular = false;
        }
    }
    SurfaceMesh<Scalar, Index> combined_mesh(dimension);

    // Allocate combined mesh
    combined_mesh.add_vertices(total_num_vertices);
    if (is_regular) {
        // Fast path for combining meshes with the same facet sizes
        combined_mesh.add_polygons(total_num_facets, vertex_per_facet);
    } else {
        // Slow path for hybrid cases
        for (size_t i = 0; i < num_meshes; ++i) {
            const auto& mesh = get_mesh(i);
            combined_mesh.add_hybrid(
                mesh.get_num_facets(),
                [&](Index f) { return mesh.get_facet_size(f); },
                []([[maybe_unused]] Index f, [[maybe_unused]] span<Index> t) {});
        }
    }

    // Assign positions + offset vertex indices
    auto vertices = vertex_ref(combined_mesh);
    auto corners = vector_ref<Index>(combined_mesh.ref_corner_to_vertex());

    Index current_v = 0;
    Index current_c = 0;
    for (size_t i = 0; i < num_meshes; ++i) {
        const auto& mesh = get_mesh(i);
        vertices.middleRows(current_v, mesh.get_num_vertices()) = vertex_view(mesh);
        corners.segment(current_c, mesh.get_num_corners()) =
            vector_view<Index>(mesh.get_corner_to_vertex()).array() + current_v;

        current_v += mesh.get_num_vertices();
        current_c += mesh.get_num_corners();
        if (is_regular) {
            la_debug_assert(mesh.get_num_corners() == mesh.get_num_facets() * vertex_per_facet);
        }
    }
    la_debug_assert(current_v == combined_mesh.get_num_vertices());
    la_debug_assert(current_c == combined_mesh.get_num_corners());

    if (preserve_attributes) {
        combine_attributes(num_meshes, get_mesh, combined_mesh);
    }

    return combined_mesh;
}

template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> combine_meshes(
    std::initializer_list<const SurfaceMesh<Scalar, Index>*> meshes,
    bool preserve_attributes)
{
    return combine_meshes<Scalar, Index>(
        meshes.size(),
        [&](size_t i) -> const SurfaceMesh<Scalar, Index>& {
            return **std::next(meshes.begin(), i);
        },
        preserve_attributes);
}

template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> combine_meshes(
    span<const SurfaceMesh<Scalar, Index>> meshes,
    bool preserve_attributes)
{
    return combine_meshes<Scalar, Index>(
        meshes.size(),
        [&](size_t i) -> const SurfaceMesh<Scalar, Index>& { return meshes[i]; },
        preserve_attributes);
}

#define LA_X_combine_meshes(_, Scalar, Index)                          \
    template SurfaceMesh<Scalar, Index> combine_meshes<Scalar, Index>( \
        std::initializer_list<const SurfaceMesh<Scalar, Index>*>,      \
        bool);                                                         \
    template SurfaceMesh<Scalar, Index> combine_meshes<Scalar, Index>( \
        span<const SurfaceMesh<Scalar, Index>>,                        \
        bool);                                                         \
    template SurfaceMesh<Scalar, Index> combine_meshes<Scalar, Index>( \
        size_t,                                                        \
        function_ref<const SurfaceMesh<Scalar, Index>&(size_t)>,       \
        bool);
LA_SURFACE_MESH_X(combine_meshes, 0)

} // namespace lagrange
