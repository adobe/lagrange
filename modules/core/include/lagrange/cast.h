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
#pragma once

#include <lagrange/AttributeFwd.h>
#include <lagrange/SurfaceMesh.h>
#include <lagrange/foreach_attribute.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/span.h>

#include <algorithm>

namespace lagrange {

///
/// @defgroup   group-surfacemesh-utils Mesh utilities
/// @ingroup    group-surfacemesh
///
/// Various attribute processing utilities
///
/// @{
///

///
/// Cast a mesh to a mesh of different scalar and/or index type.
///
/// @tparam ToScalar     Scalar type of the output mesh.
/// @tparam ToIndex      Index type of the output mesh.
/// @tparam FromScalar   Scalar type of the input mesh.
/// @tparam FromIndex    Index type of the input mesh.
///
/// @param mesh          Input mesh.
///
/// @return              Output mesh.
///
template <typename ToScalar, typename ToIndex, typename FromScalar, typename FromIndex>
SurfaceMesh<ToScalar, ToIndex> cast(const SurfaceMesh<FromScalar, FromIndex>& mesh)
{
    SurfaceMesh<ToScalar, ToIndex> result(static_cast<ToIndex>(mesh.get_dimension()));

    auto cast_buffer = [](auto&& from_buffer, auto&& to_buffer) {
        using T = typename std::decay_t<decltype(from_buffer)>::value_type;
        la_debug_assert(from_buffer.size() == to_buffer.size());
        if constexpr (std::is_same_v<FromScalar, T>) {
            std::transform(from_buffer.begin(), from_buffer.end(), to_buffer.begin(), [](auto&& v) {
                return static_cast<ToScalar>(v);
            });
        } else if constexpr (std::is_same_v<FromIndex, T>) {
            std::transform(from_buffer.begin(), from_buffer.end(), to_buffer.begin(), [](auto&& v) {
                return static_cast<ToIndex>(v);
            });
        } else {
            using T2 = typename std::decay_t<decltype(to_buffer)>::value_type;
            static_assert(std::is_convertible_v<T, T2>);
            std::transform(from_buffer.begin(), from_buffer.end(), to_buffer.begin(), [](auto&& v) {
                return static_cast<T2>(v);
            });
        }
    };

    auto cast_non_indexed_attribute = [&](auto&& from_attr, auto&& to_attr) {
        using FromAttrType = typename std::decay_t<decltype(from_attr)>;
        using ToAttrType = typename std::decay_t<decltype(to_attr)>;
        static_assert(!FromAttrType::IsIndexed);
        static_assert(!ToAttrType::IsIndexed);
        la_debug_assert(from_attr.get_element_type() == to_attr.get_element_type());
        la_debug_assert(from_attr.get_num_channels() == to_attr.get_num_channels());
        la_debug_assert(from_attr.get_usage() == to_attr.get_usage());

        if (from_attr.empty()) return;

        auto from_buffer = from_attr.get_all();
        to_attr.resize_elements(from_attr.get_num_elements());
        auto to_buffer = to_attr.ref_all();
        cast_buffer(from_buffer, to_buffer);
    };

    auto cast_attribute = [&](std::string_view name, auto&& attr, auto&& zero) {
        using FromType = typename std::decay_t<decltype(attr)>::ValueType;
        using ToType = typename std::decay_t<decltype(zero)>;
        using AttrType = typename std::decay_t<decltype(attr)>;
        if constexpr (std::is_same_v<FromType, ToType>) {
            result.create_attribute_from(name, mesh, name);
        } else if constexpr (AttrType::IsIndexed) {
            // Map indexed attribute.
            auto id = result.template create_attribute<ToType>(
                name,
                attr.get_element_type(),
                attr.get_num_channels(),
                attr.get_usage());
            auto& to_attr = result.template ref_indexed_attribute<ToType>(id);
            auto& from_values = attr.values();
            auto& from_indices = attr.indices();
            auto& to_values = to_attr.values();
            auto& to_indices = to_attr.indices();
            cast_non_indexed_attribute(from_values, to_values);
            cast_non_indexed_attribute(from_indices, to_indices);
        } else {
            // Map non-indexed attribute.
            result.template create_attribute<ToScalar>(
                name,
                attr.get_element_type(),
                attr.get_num_channels(),
                attr.get_usage());
            auto& to_attr = result.template ref_attribute<ToType>(name);
            cast_non_indexed_attribute(attr, to_attr);
        }
    };

    // Copy vertices.
    result.add_vertices(static_cast<ToIndex>(mesh.get_num_vertices()));
    cast_non_indexed_attribute(mesh.get_vertex_to_position(), result.ref_vertex_to_position());

    // Copy faces
    result.add_hybrid(
        static_cast<ToIndex>(mesh.get_num_facets()),
        [&](ToIndex fi) {
            return static_cast<ToIndex>(mesh.get_facet_size(static_cast<FromIndex>(fi)));
        },
        [&](ToIndex fi, span<ToIndex> to_buffer) {
            auto from_buffer = mesh.get_facet_vertices(static_cast<FromIndex>(fi));
            la_debug_assert(from_buffer.size() == to_buffer.size());
            size_t size = from_buffer.size();
            for (size_t i = 0; i < size; i++) {
                to_buffer[i] = static_cast<ToIndex>(from_buffer[i]);
            }
        });
    result.compress_if_regular();

    // Copy all attributes
    seq_foreach_named_attribute_read(mesh, [&](std::string_view name, auto&& attr) {
        if (mesh.attr_name_is_reserved(name)) return;
        switch (attr.get_usage()) {
        case AttributeUsage::Position:
        case AttributeUsage::UV:
        case AttributeUsage::Normal:
        case AttributeUsage::Tangent:
        case AttributeUsage::Bitangent: cast_attribute(name, attr, ToScalar(0)); break;
        case AttributeUsage::VertexIndex:
        case AttributeUsage::FacetIndex:
        case AttributeUsage::CornerIndex:
        case AttributeUsage::EdgeIndex: cast_attribute(name, attr, ToIndex(0)); break;
        default: result.create_attribute_from(name, mesh, name);
        }
    });

    return result;
}

///
/// @}
///

} // namespace lagrange
