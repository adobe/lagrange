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

#include <lagrange/mesh_convert.h>

#include <lagrange/IndexedAttribute.h>
#include <lagrange/MeshTrait.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/create_mesh.h>
#include <lagrange/foreach_attribute.h>
#include <lagrange/utils/Error.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/strings.h>
#include <lagrange/views.h>
#include <lagrange/attribute_names.h>
#include <lagrange/internal/fast_edge_sort.h>

#include <type_traits>

namespace lagrange {

namespace mesh_convert_detail {

///
/// Mesh conversion policy for attribute buffers.
///
/// @note       Not an enum class so it can be passed as template parameter
///
enum Policy {
    Copy, ///< Attribute buffers are copied to the new mesh object.
    Wrap, ///< Attribute buffers are wrapped as external buffers. The legacy mesh object must be
    ///< kept alive while the new surface mesh object exist
};

template <Policy policy, typename Scalar, typename Index, typename InputMeshType>
SurfaceMesh<Scalar, Index> to_surface_mesh_internal(InputMeshType&& mesh)
{
    using CMeshType = std::remove_reference_t<InputMeshType>;
    using MeshType = std::decay_t<InputMeshType>;
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");
    using AttributeArray = typename MeshType::AttributeArray;
    using IndexArray = typename MeshType::IndexArray;
    using MeshScalar = typename MeshType::Scalar;
    using MeshIndex = typename MeshType::Index;

    if constexpr (policy == Policy::Wrap) {
        using VertexArray = typename MeshType::VertexArray;
        using FacetArray = typename MeshType::FacetArray;
        static_assert(
            std::is_lvalue_reference_v<InputMeshType&&>,
            "Input mesh type must be a lvalue reference when wrapping a mesh object.");
        static_assert(VertexArray::IsRowMajor, "Input vertex array must have row major storage");
        static_assert(FacetArray::IsRowMajor, "Input facet array must have row major storage");
    }

#define LA_X_to_surface_mesh_scalar(_, S) || std::is_same_v<Scalar, S>
    static_assert(
        false LA_SURFACE_MESH_SCALAR_X(to_surface_mesh_scalar, 0),
        "Scalar type should be float or double");
#undef LA_X_to_surface_mesh_scalar

#define LA_X_to_surface_mesh_index(_, I) || std::is_same_v<Index, I>
    static_assert(
        false LA_SURFACE_MESH_INDEX_X(to_surface_mesh_index, 0),
        "Index type should be uint32_t or uint64_t");
#undef LA_X_to_surface_mesh_index

    // 1st - Transfer vertices positions and facets indices
    SurfaceMesh<Scalar, Index> new_mesh(static_cast<Index>(mesh.get_dim()));
    if constexpr (policy == Policy::Copy) {
        // Copy vertex/facet buffer
        new_mesh.add_vertices(static_cast<Index>(mesh.get_num_vertices()));
        new_mesh.add_polygons(static_cast<Index>(mesh.get_num_facets()),
                              static_cast<Index>(mesh.get_vertex_per_facet()));
        if (mesh.get_num_vertices() > 0) {
            vertex_ref(new_mesh) = mesh.get_vertices().template cast<Scalar>();
        }
        if (mesh.get_num_facets() > 0) {
            facet_ref(new_mesh) = mesh.get_facets().template cast<Index>();
        }
    } else if constexpr (policy == Policy::Wrap) {
        // Wrap vertex/facet buffer, but only if types are compatible
        static_assert(std::is_same_v<Scalar, MeshScalar>, "Mesh scalar type mismatch");
        static_assert(std::is_same_v<Index, MeshIndex>, "Mesh index type mismatch");
        if constexpr (std::is_const_v<CMeshType>) {
            const auto& vertices = mesh.get_vertices();
            const auto& facets = mesh.get_facets();
            new_mesh.wrap_as_const_vertices(
                {vertices.data(), safe_cast<size_t>(vertices.size())},
                mesh.get_num_vertices());
            new_mesh.wrap_as_const_facets(
                {facets.data(), safe_cast<size_t>(facets.size())},
                mesh.get_num_facets(),
                mesh.get_vertex_per_facet());
        } else {
            auto& vertices = mesh.ref_vertices();
            auto& facets = mesh.ref_facets();
            new_mesh.wrap_as_vertices(
                {vertices.data(), safe_cast<size_t>(vertices.size())},
                mesh.get_num_vertices());
            new_mesh.wrap_as_facets(
                {facets.data(), safe_cast<size_t>(facets.size())},
                mesh.get_num_facets(),
                mesh.get_vertex_per_facet());
        }
    }

    // 2nd - Transfer edge indices
    if (mesh.is_edge_data_initialized()) {
        new_mesh.initialize_edges(
            static_cast<Index>(mesh.get_num_edges()),
            [&](Index e) -> std::array<Index, 2> {
                auto v = mesh.get_edge_vertices(static_cast<typename MeshType::Index>(e));
                return {static_cast<Index>(v[0]), static_cast<Index>(v[1])};
            });
    }

    // 3rd - Transfer attributes
    auto usage_from_name = [](std::string_view name) {
        if (starts_with(name, AttributeName::normal)) {
            return AttributeUsage::Normal;
        }
        // "uv" is for legacy code with hardcoded "uv" name.
        if (starts_with(name, AttributeName::texcoord) || starts_with(name, "uv")) {
            return AttributeUsage::UV;
        }
        if (starts_with(name, AttributeName::color)) {
            return AttributeUsage::Color;
        }
        return AttributeUsage::Vector;
    };

    auto get_unique_name = [&](auto name) -> std::string {
        if (!new_mesh.has_attribute(name)) {
            return name;
        } else {
            std::string new_name;
            for (int cnt = 0; cnt < 1000; ++cnt) {
                new_name = fmt::format("{}.{}", name, cnt);
                if (!new_mesh.has_attribute(new_name)) {
                    return new_name;
                }
            }
            throw Error(fmt::format("Could not assign a unique attribute name for: {}", name));
        }
    };

    auto transfer_attribute = [&](auto&& name, auto&& array, AttributeElement elem) {
        std::string new_name = get_unique_name(name);
        decltype(auto) attr = array->template get<AttributeArray>();
        if constexpr (policy == Policy::Copy) {
            new_mesh.template create_attribute<MeshScalar>(
                new_name,
                elem,
                usage_from_name(name),
                attr.cols());
            attribute_matrix_ref<MeshScalar>(new_mesh, new_name) = attr;
        } else if constexpr (policy == Policy::Wrap) {
            if constexpr (std::is_const_v<CMeshType>) {
                new_mesh.template wrap_as_const_attribute<MeshScalar>(
                    new_name,
                    elem,
                    usage_from_name(name),
                    attr.cols(),
                    {attr.data(), static_cast<size_t>(attr.size())});

            } else {
                new_mesh.template wrap_as_attribute<MeshScalar>(
                    new_name,
                    elem,
                    usage_from_name(name),
                    attr.cols(),
                    {attr.data(), static_cast<size_t>(attr.size())});
            }
        }
    };

    // Transfer vertex attributes
    for (const auto& name : mesh.get_vertex_attribute_names()) {
        transfer_attribute(name, mesh.get_vertex_attribute_array(name), AttributeElement::Vertex);
    }

    // Transfer facet attributes
    for (const auto& name : mesh.get_facet_attribute_names()) {
        transfer_attribute(name, mesh.get_facet_attribute_array(name), AttributeElement::Facet);
    }

    // Transfer corner attributes
    for (const auto& name : mesh.get_corner_attribute_names()) {
        transfer_attribute(name, mesh.get_corner_attribute_array(name), AttributeElement::Corner);
    }

    // Transfer edge attributes
    for (const auto& name : mesh.get_edge_attribute_names()) {
        transfer_attribute(name, mesh.get_edge_attribute_array(name), AttributeElement::Edge);
    }

    // Transfer indexed attributes
    for (const auto& name : mesh.get_indexed_attribute_names()) {
        decltype(auto) attr = mesh.get_indexed_attribute_array(name);
        decltype(auto) values = std::get<0>(attr)->template get<AttributeArray>();
        decltype(auto) indices = std::get<1>(attr)->template get<IndexArray>();
        std::string new_name = get_unique_name(name);

        if constexpr (policy == Policy::Wrap) {
            static_assert(std::is_same_v<Index, MeshIndex>, "Mesh attribute index type mismatch");
            if (std::is_const_v<CMeshType>) {
                new_mesh.template wrap_as_const_indexed_attribute<MeshScalar>(
                    new_name,
                    usage_from_name(name),
                    values.rows(),
                    values.cols(),
                    {values.data(), static_cast<size_t>(values.size())},
                    {indices.data(), static_cast<size_t>(indices.size())});
            } else {
                new_mesh.template wrap_as_indexed_attribute<MeshScalar>(
                    new_name,
                    usage_from_name(name),
                    values.rows(),
                    values.cols(),
                    {values.data(), static_cast<size_t>(values.size())},
                    {indices.data(), static_cast<size_t>(indices.size())});
            }
        } else if constexpr (policy == Policy::Copy) {
            auto id = new_mesh.template create_attribute<MeshScalar>(
                new_name,
                AttributeElement::Indexed,
                usage_from_name(name),
                values.cols());
            auto& new_attr = new_mesh.template ref_indexed_attribute<MeshScalar>(id);
            new_attr.values().resize_elements(values.rows());
            matrix_ref(new_attr.values()) = values;
            reshaped_ref(new_attr.indices(), indices.cols()) = indices.template cast<Index>();
        }
    }

    return new_mesh;
}

} // namespace mesh_convert_detail

template <typename Scalar, typename Index, typename MeshType>
SurfaceMesh<Scalar, Index> to_surface_mesh_copy(const MeshType& mesh)
{
    return mesh_convert_detail::
        to_surface_mesh_internal<mesh_convert_detail::Policy::Copy, Scalar, Index>(mesh);
}

template <typename Scalar, typename Index, typename MeshType>
SurfaceMesh<Scalar, Index> to_surface_mesh_wrap(MeshType&& mesh)
{
    return mesh_convert_detail::
        to_surface_mesh_internal<mesh_convert_detail::Policy::Wrap, Scalar, Index>(
            std::forward<MeshType>(mesh));
}

template <typename MeshType, typename Scalar, typename Index>
std::unique_ptr<MeshType> to_legacy_mesh(const SurfaceMesh<Scalar, Index>& mesh)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");
    using VertexArray = typename MeshType::VertexArray;
    using FacetArray = typename MeshType::FacetArray;
    using IndexArray = typename MeshType::IndexArray;
    using AttributeArray = typename MeshType::AttributeArray;
    using MeshScalar = typename MeshType::Scalar;
    using MeshIndex = typename MeshType::Index;

#define LA_X_to_legacy_mesh_scalar(_, S) || std::is_same_v<Scalar, S>
    static_assert(
        false LA_SURFACE_MESH_SCALAR_X(to_legacy_mesh_scalar, 0),
        "Scalar type should be float or double");
#undef LA_X_to_legacy_mesh_scalar

#define LA_X_to_legacy_mesh_index(_, I) || std::is_same_v<Index, I>
    static_assert(
        false LA_SURFACE_MESH_INDEX_X(to_legacy_mesh_index, 0),
        "Index type should be uint32_t or uint64_t");
#undef LA_X_to_legacy_mesh_index

    la_runtime_assert(mesh.is_regular(), "Input polygonal mesh is not regular");

    LA_IGNORE(mesh);
    auto new_mesh = [&] {
        VertexArray vertices;
        FacetArray facets;
        if (mesh.get_num_vertices() > 0) {
            vertices = vertex_view(mesh).template cast<MeshScalar>();
        }
        if (mesh.get_num_facets() > 0) {
            facets = facet_view(mesh).template cast<MeshIndex>();
        }
        return create_mesh<VertexArray, FacetArray>(std::move(vertices), std::move(facets));
    }();

    // If mesh contains edges, attempt to transfer them as well
    std::vector<Index> old_edge_ids;
    std::vector<MeshIndex> new_edge_ids;
    if (mesh.has_edges()) {
        logger().warn("Mesh contains edges information. A possible reordering may occur.");
        new_mesh->initialize_edge_data();
        la_runtime_assert(
            mesh.get_num_edges() == static_cast<Index>(new_mesh->get_num_edges()),
            "Number of edges do not match");
        const auto num_vertices = static_cast<size_t>(mesh.get_num_vertices());
        auto buffer = std::make_unique<uint8_t[]>(
            (num_vertices + 1) * std::max(sizeof(Index), sizeof(MeshIndex)));
        old_edge_ids = internal::fast_edge_sort(
            mesh.get_num_edges(),
            mesh.get_num_vertices(),
            [&](Index e) -> std::array<Index, 2> { return mesh.get_edge_vertices(e); },
            {reinterpret_cast<Index*>(buffer.get()), num_vertices + 1});
        new_edge_ids = internal::fast_edge_sort(
            new_mesh->get_num_edges(),
            new_mesh->get_num_vertices(),
            [&](MeshIndex e) -> std::array<MeshIndex, 2> { return new_mesh->get_edge_vertices(e); },
            {reinterpret_cast<MeshIndex*>(buffer.get()), num_vertices + 1});
    }

    // Transfer non-indexed attributes
    seq_foreach_named_attribute_read<~AttributeElement::Indexed>(
        mesh,
        [&](std::string_view name_view, auto&& attr) {
            using AttributeType = std::decay_t<decltype(attr)>;
            using ValueType = typename AttributeType::ValueType;
            if (mesh.attr_name_is_reserved(name_view)) {
                return;
            }
            std::string name(name_view);
            AttributeArray vals =
                attribute_matrix_view<ValueType>(mesh, name).template cast<MeshScalar>();
            switch (attr.get_element_type()) {
            case AttributeElement::Vertex: {
                new_mesh->add_vertex_attribute(name);
                new_mesh->import_vertex_attribute(name, std::move(vals));
                break;
            }
            case AttributeElement::Facet: {
                new_mesh->add_facet_attribute(name);
                new_mesh->import_facet_attribute(name, std::move(vals));
                break;
            }
            case AttributeElement::Corner: {
                new_mesh->add_corner_attribute(name);
                new_mesh->import_corner_attribute(name, std::move(vals));
                break;
            }
            case AttributeElement::Edge: {
                // Permute rows
                auto old_vals = attribute_matrix_view<ValueType>(mesh, name);
                for (Index e = 0; e < mesh.get_num_edges(); ++e) {
                    vals.row(new_edge_ids[e]) =
                        old_vals.row(old_edge_ids[e]).template cast<MeshScalar>();
                }
                new_mesh->add_edge_attribute(name);
                new_mesh->import_edge_attribute(name, std::move(vals));
                break;
            }
            case AttributeElement::Value:
                logger().warn("Cannot transfer value attribute: {}", name);
                break;
            case AttributeElement::Indexed:
                // Not possible by construction
                break;
            }
        });

    // Transfer indexed attributes
    const auto nvpf = static_cast<size_t>(new_mesh->get_vertex_per_facet());
    seq_foreach_named_attribute_read<AttributeElement::Indexed>(
        mesh,
        [&](std::string_view name_view, auto&& attr) {
            if (mesh.attr_name_is_reserved(name_view)) {
                return;
            }
            std::string name(name_view);
            IndexArray indices = reshaped_view(attr.indices(), nvpf).template cast<MeshIndex>();
            AttributeArray values = matrix_view(attr.values()).template cast<MeshScalar>();
            new_mesh->add_indexed_attribute(name);
            new_mesh->import_indexed_attribute(name, std::move(values), std::move(indices));
        });

    return new_mesh;
}

} // namespace lagrange
