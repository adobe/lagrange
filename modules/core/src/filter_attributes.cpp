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
#include <lagrange/filter_attributes.h>

#include <lagrange/Attribute.h>
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/internal/attribute_string_utils.h>

namespace lagrange {

namespace {

bool contains(
    const std::vector<AttributeFilter::AttributeNameOrId>& items,
    std::string_view name,
    AttributeId id)
{
    for (const auto& item : items) {
        if (std::holds_alternative<AttributeId>(item)) {
            if (std::get<AttributeId>(item) == id) {
                return true;
            }
        } else {
            if (std::get<std::string>(item) == name) {
                return true;
            }
        }
    }
    return false;
}

} // namespace

template <typename Scalar, typename Index>
std::vector<AttributeId> filtered_attribute_ids(
    const SurfaceMesh<Scalar, Index>& mesh,
    const AttributeFilter& options)
{
    std::vector<AttributeId> result;
    mesh.seq_foreach_attribute_id([&](std::string_view name, AttributeId id) {
        if (mesh.attr_name_is_reserved(name)) {
            return;
        }
        const auto& attr = mesh.get_attribute_base(id);
        if (!options.included_usages.test(attr.get_usage())) {
            logger().debug(
                "Skipping attribute {} with usage {}",
                name,
                internal::to_string(attr.get_usage()));
            return;
        }
        if (!options.included_element_types.test(attr.get_element_type())) {
            logger().debug(
                "Skipping attribute {} with element type {}",
                name,
                internal::to_string(attr.get_element_type()));
            return;
        }
        if (options.excluded_attributes.has_value() &&
            contains(options.excluded_attributes.value(), name, id)) {
            logger().debug("Skipping attribute {} (id: {}) which is explicitly excluded", name, id);
            return;
        }
        if (options.included_attributes.has_value() &&
            !contains(options.included_attributes.value(), name, id)) {
            logger().debug(
                "Skipping attribute {} (id: {}) which is not explicitly included",
                name,
                id);
            return;
        }
        result.push_back(id);
    });
    return result;
}

template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> filter_attributes(
    SurfaceMesh<Scalar, Index> source_mesh,
    const AttributeFilter& filter)
{
    if (!filter.included_element_types.test(AttributeElement::Edge) && source_mesh.has_edges()) {
        source_mesh.clear_edges();
    }
    auto target_mesh = SurfaceMesh<Scalar, Index>::stripped_move(std::move(source_mesh));

    auto ids = filtered_attribute_ids(source_mesh, filter);
    for (auto id : ids) {
        target_mesh.create_attribute_from(source_mesh.get_attribute_name(id), source_mesh);
    }

    return target_mesh;
}

#define LA_X_filter_attributes(_, Scalar, Index)              \
    template LA_CORE_API std::vector<AttributeId> filtered_attribute_ids( \
        const SurfaceMesh<Scalar, Index>& mesh,               \
        const AttributeFilter& options);                      \
    template LA_CORE_API SurfaceMesh<Scalar, Index> filter_attributes(    \
        SurfaceMesh<Scalar, Index>,                           \
        const AttributeFilter&);

LA_SURFACE_MESH_X(filter_attributes, 0)

} // namespace lagrange
