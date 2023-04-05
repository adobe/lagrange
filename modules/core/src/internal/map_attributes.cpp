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
#include <lagrange/AttributeFwd.h>
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/foreach_attribute.h>
#include <lagrange/internal/find_attribute_utils.h>
#include <lagrange/internal/map_attributes.h>
#include <lagrange/utils/Error.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/invalid.h>
#include <lagrange/views.h>

namespace lagrange::internal {

template <AttributeElement element, typename Scalar, typename Index>
void map_attributes(
    const SurfaceMesh<Scalar, Index>& source_mesh,
    SurfaceMesh<Scalar, Index>& target_mesh,
    span<const Index> mapping_data,
    span<const Index> mapping_offsets,
    const MapAttributesOptions& options)
{
    static_assert(element != Indexed, "Indexed attribute mapping is not supported.");
    static_assert(element != Value, "Value attribute mapping is not supported.");

    la_runtime_assert(
        mapping_offsets.empty() ||
        static_cast<Index>(mapping_offsets.size()) == target_mesh.get_num_vertices() + 1);

    auto map_attribute_no_offset = [&](std::string_view name, auto&& attr) {
        using AttributeType = std::decay_t<decltype(attr)>;
        using ValueType = typename AttributeType::ValueType;

        AttributeId id = internal::find_or_create_attribute<ValueType>(
            target_mesh,
            name,
            element,
            attr.get_usage(),
            attr.get_num_channels(),
            internal::ResetToDefault::No);

        auto source_data = matrix_view(attr);
        auto target_data = attribute_matrix_ref<ValueType>(target_mesh, id);
        target_data.setZero();

        Index num_elements = static_cast<Index>(mapping_data.size());
        for (Index i = 0; i < num_elements; i++) {
            target_data.row(i) = source_data.row(mapping_data[i]);
        }
    };

    auto map_attribute_average = [&](std::string_view name, auto&& attr) {
        using AttributeType = std::decay_t<decltype(attr)>;
        using ValueType = typename AttributeType::ValueType;

        auto usage = attr.get_usage();
        if (usage == AttributeUsage::VertexIndex || usage == AttributeUsage::FacetIndex ||
            usage == AttributeUsage::CornerIndex) {
            throw Error("Cannot average indices!");
        }

        AttributeId id = internal::find_or_create_attribute<ValueType>(
            target_mesh,
            name,
            element,
            usage,
            attr.get_num_channels(),
            internal::ResetToDefault::No);

        auto source_data = matrix_view(attr);
        auto target_data = attribute_matrix_ref<ValueType>(target_mesh, id);
        target_data.setZero();

        const auto num_target_elements = static_cast<Index>(mapping_offsets.size() - 1);
        for (Index i = 0; i < num_target_elements; i++) {
            la_debug_assert(mapping_offsets[i] <= mapping_offsets[i + 1]);
            for (Index j = mapping_offsets[i]; j < mapping_offsets[i + 1]; j++) {
                target_data.row(i) += source_data.row(mapping_data[j]);
            }
            Index s = mapping_offsets[i + 1] - mapping_offsets[i];
            if (s > 1) {
                target_data.row(i) /= static_cast<ValueType>(s);
            }
        }
    };

    auto map_attribute_keep_first = [&](std::string_view name, auto&& attr) {
        using AttributeType = std::decay_t<decltype(attr)>;
        using ValueType = typename AttributeType::ValueType;

        AttributeId id = internal::find_or_create_attribute<ValueType>(
            target_mesh,
            name,
            element,
            attr.get_usage(),
            attr.get_num_channels(),
            internal::ResetToDefault::No);

        auto source_data = matrix_view(attr);
        auto target_data = attribute_matrix_ref<ValueType>(target_mesh, id);
        target_data.setZero();

        const auto num_target_elements = static_cast<Index>(mapping_offsets.size() - 1);
        for (Index i = 0; i < num_target_elements; i++) {
            la_debug_assert(mapping_offsets[i] <= mapping_offsets[i + 1]);
            if (mapping_offsets[i] < mapping_offsets[i + 1]) {
                Index j = mapping_offsets[i];
                target_data.row(i) = source_data.row(mapping_data[j]);
            }
        }
    };

    auto map_attribute_injective = [&](std::string_view name, auto&& attr) {
        using AttributeType = std::decay_t<decltype(attr)>;
        using ValueType = typename AttributeType::ValueType;

        auto usage = attr.get_usage();
        AttributeId id = internal::find_or_create_attribute<ValueType>(
            target_mesh,
            name,
            element,
            usage,
            attr.get_num_channels(),
            internal::ResetToDefault::No);

        auto source_data = matrix_view(attr);
        auto target_data = attribute_matrix_ref<ValueType>(target_mesh, id);
        target_data.setZero();

        const auto num_target_elements = static_cast<Index>(mapping_offsets.size() - 1);
        for (Index i = 0; i < num_target_elements; i++) {
            la_runtime_assert(
                mapping_offsets[i] + 1 == mapping_offsets[i + 1],
                "Mapping is not injective");
            Index j = mapping_offsets[i];
            target_data.row(i) = source_data.row(mapping_data[j]);
        }
    };

    auto map_attribute = [&](std::string_view name, auto&& attr) {
        using AttributeType = std::decay_t<decltype(attr)>;
        using ValueType = typename AttributeType::ValueType;
        if (source_mesh.attr_name_is_reserved(name)) return;

        if (mapping_offsets.empty()) {
            map_attribute_no_offset(name, std::forward<const AttributeType>(attr));
        } else {
            MappingPolicy collision_policy;
            if constexpr (std::is_integral_v<ValueType>) {
                collision_policy = options.collision_policy_integral;
            } else {
                collision_policy = options.collision_policy_float;
            }

            switch (collision_policy) {
            case MappingPolicy::Average:
                map_attribute_average(name, std::forward<const AttributeType>(attr));
                break;
            case MappingPolicy::KeepFirst:
                map_attribute_keep_first(name, std::forward<const AttributeType>(attr));
                break;
            case MappingPolicy::Error:
                map_attribute_injective(name, std::forward<const AttributeType>(attr));
                break;
            default:
                throw Error(fmt::format(
                    "Unsupported collision policy {}",
                    int(options.collision_policy_integral)));
            }
        }
    };

    details::internal_foreach_named_attribute<
        element,
        details::Ordering::Sequential,
        details::Access::Read>(source_mesh, map_attribute, options.selected_attributes);
}

#define LA_X_map_attributes(_, Scalar, Index)            \
    template void map_attributes<Vertex, Scalar, Index>( \
        const SurfaceMesh<Scalar, Index>&,               \
        SurfaceMesh<Scalar, Index>&,                     \
        span<const Index>,                               \
        span<const Index>,                               \
        const MapAttributesOptions&);                    \
    template void map_attributes<Facet, Scalar, Index>(  \
        const SurfaceMesh<Scalar, Index>&,               \
        SurfaceMesh<Scalar, Index>&,                     \
        span<const Index>,                               \
        span<const Index>,                               \
        const MapAttributesOptions&);                    \
    template void map_attributes<Corner, Scalar, Index>( \
        const SurfaceMesh<Scalar, Index>&,               \
        SurfaceMesh<Scalar, Index>&,                     \
        span<const Index>,                               \
        span<const Index>,                               \
        const MapAttributesOptions&);                    \
    template void map_attributes<Edge, Scalar, Index>(   \
        const SurfaceMesh<Scalar, Index>&,               \
        SurfaceMesh<Scalar, Index>&,                     \
        span<const Index>,                               \
        span<const Index>,                               \
        const MapAttributesOptions&);
LA_SURFACE_MESH_X(map_attributes, 0)

} // namespace lagrange::internal
