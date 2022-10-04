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
#include "retrieve_normal_attribute.h"

#include <lagrange/Attribute.h>
#include <lagrange/IndexedAttribute.h>
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/internal/string_from_scalar.h>
#include <lagrange/utils/assert.h>

namespace lagrange::internal {

template <typename Scalar, typename Index>
AttributeId retrieve_normal_attribute(
    SurfaceMesh<Scalar, Index>& mesh,
    std::string_view name,
    AttributeElement element)
{
    // TODO: Maybe this should become a method of the SurfaceMesh class? (e.g.
    // `retrieve_attribute<...>`)
    AttributeId id;
    if (!mesh.has_attribute(name)) {
        id = mesh.template create_attribute<Scalar>(name, element, AttributeUsage::Normal, 3);
    } else {
        // Sanity checks on user-given attribute name
        id = mesh.get_attribute_id(name);
        la_runtime_assert(
            mesh.template is_attribute_type<Scalar>(id),
            fmt::format("Attribute type should be {}", internal::string_from_scalar<Scalar>()));
        if (element != Indexed) {
            la_runtime_assert(!mesh.is_attribute_indexed(id), "Attribute should not be indexed");
            const auto& attr = mesh.template get_attribute<Scalar>(id);
            la_runtime_assert(
                attr.get_num_channels() == 3,
                fmt::format("Attribute should have 3 channels, not {}", attr.get_num_channels()));
            la_runtime_assert(
                attr.get_usage() == AttributeUsage::Normal,
                "Attribute usage should be normal");
            la_runtime_assert(!attr.is_read_only(), "Attribute is read only");
        } else {
            la_runtime_assert(mesh.is_attribute_indexed(id), "Attribute should be indexed");
            const auto& attr = mesh.template get_indexed_attribute<Scalar>(id);
            la_runtime_assert(
                attr.get_num_channels() == 3,
                fmt::format("Attribute should have 3 channels, not {}", attr.get_num_channels()));
            la_runtime_assert(
                attr.get_usage() == AttributeUsage::Normal,
                "Attribute usage should be normal");
            la_runtime_assert(!attr.values().is_read_only(), "Attribute is read only");
            la_runtime_assert(!attr.indices().is_read_only(), "Attribute is read only");
        }
        logger().debug("Attribute {} already exists, overwriting it.", name);
    }
    return id;
}

#define LA_X_retrieve_normal_attribute(_, Scalar, Index)           \
    template AttributeId retrieve_normal_attribute<Scalar, Index>( \
        SurfaceMesh<Scalar, Index>&,                               \
        std::string_view,                                          \
        AttributeElement);
LA_SURFACE_MESH_X(retrieve_normal_attribute, 0)

} // namespace lagrange::internal
