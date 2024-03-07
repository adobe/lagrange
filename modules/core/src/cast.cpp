/*
 * Copyright 2024 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include <lagrange/cast.h>

#include <lagrange/AttributeFwd.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/foreach_attribute.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/span.h>

#include <algorithm>

namespace lagrange {

namespace {

bool contains(const std::vector<AttributeId>& v, AttributeId x)
{
    return std::find(v.begin(), v.end(), x) != v.end();
}

} // namespace

template <typename TargetScalar, typename TargetIndex, typename SourceScalar, typename SourceIndex>
SurfaceMesh<TargetScalar, TargetIndex> cast(
    const SurfaceMesh<SourceScalar, SourceIndex>& source_mesh,
    const AttributeFilter& convertible_attributes,
    std::vector<std::string>* converted_attributes_names)
{
    if (converted_attributes_names) {
        converted_attributes_names->clear();
    }

    SurfaceMesh<TargetScalar, TargetIndex> target_mesh =
        SurfaceMesh<TargetScalar, TargetIndex>::stripped_copy(source_mesh);

    auto cast_attribute = [&](std::string_view name, auto&& source_attr, auto&& target_zero) {
        using SourceType = typename std::decay_t<decltype(source_attr)>::ValueType;
        using TargetType = typename std::decay_t<decltype(target_zero)>;
        using AttrType = typename std::decay_t<decltype(source_attr)>;
        if constexpr (
            std::is_same_v<SourceType, TargetType> &&
            (std::is_same_v<TargetIndex, SourceIndex> || !AttrType::IsIndexed)) {
            target_mesh.create_attribute_from(name, source_mesh, name);
        } else {
            auto id = target_mesh.template create_attribute<TargetType>(
                name,
                source_attr.get_element_type(),
                source_attr.get_num_channels(),
                source_attr.get_usage());
            if (converted_attributes_names) {
                converted_attributes_names->emplace_back(name);
            }
            if constexpr (AttrType::IsIndexed) {
                auto& target_attr = target_mesh.template ref_indexed_attribute<TargetType>(id);
                target_attr.values().cast_assign(source_attr.values());
                target_attr.indices().cast_assign(source_attr.indices());
            } else {
                auto& target_attr = target_mesh.template ref_attribute<TargetType>(id);
                target_attr.cast_assign(source_attr);
            }
        }
    };

    auto convertible_ids = filtered_attribute_ids(source_mesh, convertible_attributes);
    seq_foreach_named_attribute_read(source_mesh, [&](std::string_view name, auto&& attr) {
        if (source_mesh.attr_name_is_reserved(name)) return;
        if (!contains(convertible_ids, source_mesh.get_attribute_id(name))) {
            target_mesh.create_attribute_from(name, source_mesh);
            return;
        }

        using AttributeType = std::decay_t<decltype(attr)>;
        using SourceValueType = typename AttributeType::ValueType;

        if constexpr (std::is_same_v<SourceValueType, SourceScalar>) {
            cast_attribute(name, attr, TargetScalar(0));
        } else if constexpr (std::is_same_v<SourceValueType, SourceIndex>) {
            cast_attribute(name, attr, TargetIndex(0));
        } else {
            target_mesh.create_attribute_from(name, source_mesh);
        }
    });

    return target_mesh;
}

// NOTE: This is a dirty workaround because nesting two LA_SURFACE_MESH_X macros doesn't quite work.
// I am not sure if there is a proper way to do this.
// clang-format off
#define LA_SURFACE_MESH2_X(mode, data)  \
    LA_X_##mode(data, float, uint32_t)  \
    LA_X_##mode(data, double, uint32_t) \
    LA_X_##mode(data, float, uint64_t)  \
    LA_X_##mode(data, double, uint64_t)
// clang-format on

// Iterate over mesh (scalar, index) x (scalar, index) types
#define fst(first, second) first
#define snd(first, second) second
#define LA_X_cast_mesh_to(SourceScalarIndex, TargetScalar, TargetIndex)               \
    template SurfaceMesh<TargetScalar, TargetIndex> cast(                             \
        const SurfaceMesh<fst SourceScalarIndex, snd SourceScalarIndex>& source_mesh, \
        const AttributeFilter& convertible_attributes,                                \
        std::vector<std::string>* converted_attributes_names);
#define LA_X_cast_mesh_from(_, SourceScalar, SourceIndex) \
    LA_SURFACE_MESH2_X(cast_mesh_to, (SourceScalar, SourceIndex))
LA_SURFACE_MESH_X(cast_mesh_from, 0)

} // namespace lagrange
