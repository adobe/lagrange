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
#include <lagrange/cast_attribute.h>

#include <lagrange/Attribute.h>
#include <lagrange/AttributeTypes.h>
#include <lagrange/AttributeValueType.h>
#include <lagrange/IndexedAttribute.h>
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/internal/visit_attribute.h>

namespace lagrange {

template <typename TargetValueType, typename Scalar, typename Index>
AttributeId cast_attribute(
    SurfaceMesh<Scalar, Index>& mesh,
    AttributeId source_id,
    std::string_view target_name)
{
    const auto& source_attr_base = mesh.get_attribute_base(source_id);

    if (source_attr_base.get_value_type() == make_attribute_value_type<TargetValueType>()) {
        logger().warn("Target value type is the same as source value type. Creating shallow copy.");
        return mesh.create_attribute_from(target_name, mesh, mesh.get_attribute_name(source_id));
    }

    const auto target_id = mesh.template create_attribute<TargetValueType>(
        target_name,
        source_attr_base.get_element_type(),
        source_attr_base.get_usage(),
        source_attr_base.get_num_channels());

    internal::visit_attribute_read(mesh, source_id, [&](auto& source_attr) {
        using AttributeType = std::decay_t<decltype(source_attr)>;
        if constexpr (AttributeType::IsIndexed) {
            auto& target_attr = mesh.template ref_indexed_attribute<TargetValueType>(target_id);
            target_attr.values().cast_assign(source_attr.values());
            target_attr.indices() = source_attr.indices();
        } else {
            auto& target_attr = mesh.template ref_attribute<TargetValueType>(target_id);
            target_attr.cast_assign(source_attr);
        }
    });

    return target_id;
}

template <typename TargetValueType, typename Scalar, typename Index>
AttributeId cast_attribute(
    SurfaceMesh<Scalar, Index>& mesh,
    std::string_view source_name,
    std::string_view target_name)
{
    return cast_attribute<TargetValueType>(mesh, mesh.get_attribute_id(source_name), target_name);
}

template <typename TargetValueType, typename Scalar, typename Index>
AttributeId cast_attribute_in_place(SurfaceMesh<Scalar, Index>& mesh, AttributeId source_id)
{
    const auto& source_attr_base = mesh.get_attribute_base(source_id);

    if (source_attr_base.get_value_type() == make_attribute_value_type<TargetValueType>()) {
        logger().warn("Target value type is the same as source value type. Nothing to do.");
        return source_id;
    }

    std::string target_name(mesh.get_attribute_name(source_id));

    AttributeId target_id = invalid_attribute_id();
    internal::visit_attribute_read(mesh, source_id, [&](auto& source_attr_) {
        using AttributeType = std::decay_t<decltype(source_attr_)>;
        using SourceValueType = typename AttributeType::ValueType;

        if constexpr (AttributeType::IsIndexed) {
            auto source_attr =
                mesh.template delete_and_export_const_indexed_attribute<SourceValueType>(
                    mesh.get_attribute_name(source_id),
                    AttributeExportPolicy::KeepExternalPtr);

            target_id = mesh.template create_attribute<TargetValueType>(
                target_name,
                source_attr->get_element_type(),
                source_attr->get_usage(),
                source_attr->get_num_channels());

            auto& target_attr = mesh.template ref_indexed_attribute<TargetValueType>(target_id);
            target_attr.values().cast_assign(source_attr->values());
            target_attr.indices() = std::move(source_attr->indices());
        } else {
            auto source_attr = mesh.template delete_and_export_const_attribute<SourceValueType>(
                mesh.get_attribute_name(source_id),
                AttributeDeletePolicy::ErrorIfReserved,
                AttributeExportPolicy::KeepExternalPtr);

            target_id = mesh.template create_attribute<TargetValueType>(
                target_name,
                source_attr->get_element_type(),
                source_attr->get_usage(),
                source_attr->get_num_channels());

            auto& target_attr = mesh.template ref_attribute<TargetValueType>(target_id);
            target_attr.cast_assign(*source_attr);
        }
    });

    return target_id;
}

template <typename TargetValueType, typename Scalar, typename Index>
AttributeId cast_attribute_in_place(SurfaceMesh<Scalar, Index>& mesh, std::string_view name)
{
    return cast_attribute_in_place<TargetValueType>(mesh, mesh.get_attribute_id(name));
}

#define LA_X_cast_attribute(ValueType, Scalar, Index)                    \
    template LA_CORE_API AttributeId cast_attribute<ValueType>(          \
        SurfaceMesh<Scalar, Index> & mesh,                               \
        AttributeId source_id,                                           \
        std::string_view target_name);                                   \
    template LA_CORE_API AttributeId cast_attribute<ValueType>(          \
        SurfaceMesh<Scalar, Index> & mesh,                               \
        std::string_view source_name,                                    \
        std::string_view target_name);                                   \
    template LA_CORE_API AttributeId cast_attribute_in_place<ValueType>( \
        SurfaceMesh<Scalar, Index> & mesh,                               \
        AttributeId source_id);                                          \
    template LA_CORE_API AttributeId cast_attribute_in_place<ValueType>( \
        SurfaceMesh<Scalar, Index> & mesh,                               \
        std::string_view source_name);
#define LA_X_cast_attribute_aux(_, ValueType) LA_SURFACE_MESH_X(cast_attribute, ValueType)
LA_ATTRIBUTE_X(cast_attribute_aux, 0)

} // namespace lagrange
