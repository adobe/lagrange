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
#include <lagrange/internal/find_attribute_utils.h>

#include <lagrange/Attribute.h>
#include <lagrange/AttributeTypes.h>
#include <lagrange/IndexedAttribute.h>
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/foreach_attribute.h>
#include <lagrange/internal/attribute_string_utils.h>
#include <lagrange/internal/string_from_scalar.h>
#include <lagrange/utils/assert.h>

namespace lagrange::internal {

namespace {

enum class ShouldBeWritable { Yes, No };

template <typename ExpectedValueType, typename Scalar, typename Index>
void check_attribute(
    const SurfaceMesh<Scalar, Index>& mesh,
    AttributeId id,
    AttributeElement expected_element,
    AttributeUsage expected_usage,
    size_t expected_channels,
    ShouldBeWritable expected_writable)
{
    la_runtime_assert(
        mesh.template is_attribute_type<ExpectedValueType>(id),
        fmt::format("Attribute type should be {}", string_from_scalar<ExpectedValueType>()));

    {
        const auto& attr = mesh.get_attribute_base(id);
        la_runtime_assert(
            attr.get_usage() == expected_usage,
            fmt::format(
                "Attribute usage should be {}, not {}",
                to_string(expected_usage),
                to_string(attr.get_usage())));
        la_runtime_assert(
            attr.get_element_type() == expected_element,
            fmt::format(
                "Attribute element type should be {}, not {}",
                to_string(expected_element),
                to_string(attr.get_element_type())));
        if (expected_channels != 0) {
            la_runtime_assert(
                attr.get_num_channels() == expected_channels,
                fmt::format(
                    "Attribute should have {} channels, not {}",
                    expected_channels,
                    attr.get_num_channels()));
        }
    }

    if (expected_writable == ShouldBeWritable::Yes) {
        if (expected_element != Indexed) {
            const auto& attr = mesh.template get_attribute<Scalar>(id);
            la_runtime_assert(!attr.is_read_only(), "Attribute is read only");
        } else {
            const auto& attr = mesh.template get_indexed_attribute<Scalar>(id);
            la_runtime_assert(!attr.values().is_read_only(), "Attribute is read only");
            la_runtime_assert(!attr.indices().is_read_only(), "Attribute is read only");
        }
    }
}

} // namespace

template <typename ExpectedValueType, typename Scalar, typename Index>
AttributeId find_matching_attribute(
    const SurfaceMesh<Scalar, Index>& mesh,
    std::string_view name,
    AttributeElement expected_element,
    AttributeUsage expected_usage,
    size_t expected_channels)
{
    AttributeId id = invalid_attribute_id();
    if (name.empty()) {
        // No attribute name provided. Iterate until we find the first matching attribute.
        seq_foreach_named_attribute_read(mesh, [&](auto name, auto&& attr) {
            using AttributeType = std::decay_t<decltype(attr)>;
            using ValueType = typename AttributeType::ValueType;
            if (id == invalid_attribute_id() && attr.get_usage() == expected_usage &&
                attr.get_element_type() == expected_element &&
                (expected_channels == 0 || attr.get_num_channels() == expected_channels) &&
                std::is_same_v<ValueType, ExpectedValueType>) {
                logger().trace(
                    "Found attribute '{}' with element type {}, usage {}, value type {}.",
                    name,
                    to_string(expected_element),
                    to_string(expected_usage),
                    string_from_scalar<ValueType>());
                id = mesh.get_attribute_id(name);
            }
        });
    } else {
        // Check user-provided attribute compatibility
        id = mesh.get_attribute_id(name);
        check_attribute<ExpectedValueType>(
            mesh,
            id,
            expected_element,
            expected_usage,
            expected_channels,
            ShouldBeWritable::No);
    }

    return id;
}

template <typename ExpectedValueType, typename Scalar, typename Index>
AttributeId find_attribute(
    const SurfaceMesh<Scalar, Index>& mesh,
    std::string_view name,
    AttributeElement expected_element,
    AttributeUsage expected_usage,
    size_t expected_channels)
{
    AttributeId id = invalid_attribute_id();
    la_runtime_assert(!name.empty(), "Attribute name must not be empty!");

    // Check user-provided attribute compatibility
    id = mesh.get_attribute_id(name);
    check_attribute<ExpectedValueType>(
        mesh,
        id,
        expected_element,
        expected_usage,
        expected_channels,
        ShouldBeWritable::No);

    return id;
}

template <typename ExpectedValueType, typename Scalar, typename Index>
AttributeId find_or_create_attribute(
    SurfaceMesh<Scalar, Index>& mesh,
    std::string_view name,
    AttributeElement expected_element,
    AttributeUsage expected_usage,
    size_t expected_channels,
    ResetToDefault reset_tag)
{
    // TODO: Maybe this should become a method of the SurfaceMesh class? (e.g.
    // `retrieve_attribute<...>`)
    AttributeId id;
    la_runtime_assert(!name.empty(), "Attribute name cannot be empty");
    if (!mesh.has_attribute(name)) {
        id = mesh.template create_attribute<Scalar>(
            name,
            expected_element,
            expected_usage,
            expected_channels);
    } else {
        // Sanity checks on user-given attribute name
        id = mesh.get_attribute_id(name);
        check_attribute<ExpectedValueType>(
            mesh,
            id,
            expected_element,
            expected_usage,
            expected_channels,
            ShouldBeWritable::Yes);
        if (reset_tag == ResetToDefault::Yes) {
            if (expected_element != Indexed) {
                auto& attr = mesh.template ref_attribute<Scalar>(id);
                std::fill(attr.ref_all().begin(), attr.ref_all().end(), attr.get_default_value());
            } else {
                auto& attr = mesh.template ref_indexed_attribute<Scalar>(id).values();
                std::fill(attr.ref_all().begin(), attr.ref_all().end(), attr.get_default_value());
            }
        }
        logger().debug("Attribute {} already exists, reusing it.", name);
    }
    return id;
}

// Iterate over attribute types x mesh (scalar, index) types
#define LA_X_find_attribute(ExpectedValueType, Scalar, Index)                        \
    template AttributeId find_matching_attribute<ExpectedValueType, Scalar, Index>(  \
        const SurfaceMesh<Scalar, Index>& mesh,                                      \
        std::string_view name,                                                       \
        AttributeElement expected_element,                                           \
        AttributeUsage expected_usage,                                               \
        size_t expected_channels);                                                   \
    template AttributeId find_attribute<ExpectedValueType, Scalar, Index>(           \
        const SurfaceMesh<Scalar, Index>& mesh,                                      \
        std::string_view name,                                                       \
        AttributeElement expected_element,                                           \
        AttributeUsage expected_usage,                                               \
        size_t expected_channels);                                                   \
    template AttributeId find_or_create_attribute<ExpectedValueType, Scalar, Index>( \
        SurfaceMesh<Scalar, Index> & mesh,                                           \
        std::string_view name,                                                       \
        AttributeElement expected_element,                                           \
        AttributeUsage expected_usage,                                               \
        size_t expected_channels,                                                    \
        ResetToDefault reset_tag);
#define LA_X_find_attribute_aux(_, ExpectedValueType) \
    LA_SURFACE_MESH_X(find_attribute, ExpectedValueType)
LA_ATTRIBUTE_X(find_attribute_aux, 0)

} // namespace lagrange::internal
