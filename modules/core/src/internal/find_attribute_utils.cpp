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
#include <lagrange/utils/Error.h>
#include <lagrange/utils/assert.h>


namespace lagrange::internal {

namespace {

enum class ShouldBeWritable { Yes, No };

struct Result
{
    bool success = true;
    std::string msg;
};

#define check_that(x, msh)         \
    if (!(x)) {                    \
        return Result{false, msh}; \
    }

template <typename ExpectedValueType, typename Scalar, typename Index>
Result check_attribute(
    const SurfaceMesh<Scalar, Index>& mesh,
    AttributeId id,
    BitField<AttributeElement> expected_element,
    AttributeUsage expected_usage,
    size_t expected_channels,
    ShouldBeWritable expected_writable)
{
    check_that(
        mesh.template is_attribute_type<ExpectedValueType>(id),
        fmt::format("Attribute type should be {}", string_from_scalar<ExpectedValueType>()));

    {
        const auto& attr = mesh.get_attribute_base(id);
        check_that(
            attr.get_usage() == expected_usage,
            fmt::format(
                "Attribute usage should be {}, not {}",
                to_string(expected_usage),
                to_string(attr.get_usage())));
        check_that(
            expected_element.test(attr.get_element_type()),
            fmt::format(
                "Attribute element type should be {}, not {}",
                to_string(expected_element),
                to_string(attr.get_element_type())));
        if (expected_channels != 0) {
            check_that(
                attr.get_num_channels() == expected_channels,
                fmt::format(
                    "Attribute should have {} channels, not {}",
                    expected_channels,
                    attr.get_num_channels()));
        }
    }

    if (expected_writable == ShouldBeWritable::Yes) {
        if (mesh.is_attribute_indexed(id)) {
            const auto& attr = mesh.template get_indexed_attribute<ExpectedValueType>(id);
            check_that(!attr.values().is_read_only(), "Attribute is read only");
            check_that(!attr.indices().is_read_only(), "Attribute is read only");
        } else {
            const auto& attr = mesh.template get_attribute<ExpectedValueType>(id);
            check_that(!attr.is_read_only(), "Attribute is read only");
        }
    }

    return {true, ""};
}

} // namespace

template <typename ExpectedValueType, typename Scalar, typename Index>
AttributeId find_matching_attribute(
    const SurfaceMesh<Scalar, Index>& mesh,
    std::string_view name,
    BitField<AttributeElement> expected_element,
    AttributeUsage expected_usage,
    size_t expected_channels)
{
    AttributeId id = invalid_attribute_id();
    if (name.empty()) {
        // No attribute name provided. Iterate until we find the first matching attribute.
        id = find_matching_attribute<ExpectedValueType>(
            mesh,
            span<AttributeId>(),
            expected_element,
            expected_usage,
            expected_channels);
    } else {
        // Check user-provided attribute compatibility
        id = mesh.get_attribute_id(name);
        auto [ok, msg] = check_attribute<ExpectedValueType>(
            mesh,
            id,
            expected_element,
            expected_usage,
            expected_channels,
            ShouldBeWritable::No);
        la_runtime_assert(ok, msg);
    }

    return id;
}

template <typename ExpectedValueType, typename Scalar, typename Index>
AttributeId find_matching_attribute(
    const SurfaceMesh<Scalar, Index>& mesh,
    span<const AttributeId> selected_ids,
    BitField<AttributeElement> expected_element,
    AttributeUsage expected_usage,
    size_t expected_channels)
{
    AttributeId output_id = invalid_attribute_id();
    auto match_attribute = [&](AttributeId id) {
        if (output_id != invalid_attribute_id()) return;
        if (!mesh.template is_attribute_type<ExpectedValueType>(id)) return;

        auto& attr = mesh.get_attribute_base(id);
        if (!expected_element.test(attr.get_element_type())) return;
        if (attr.get_usage() != expected_usage) return;
        if (expected_channels != 0 && attr.get_num_channels() != expected_channels) return;

        output_id = id;
    };

    if (selected_ids.empty()) {
        mesh.seq_foreach_attribute_id(match_attribute);
    } else {
        std::for_each(selected_ids.begin(), selected_ids.end(), match_attribute);
    }
    return output_id;
}


template <typename ExpectedValueType, typename Scalar, typename Index>
AttributeId find_attribute(
    const SurfaceMesh<Scalar, Index>& mesh,
    std::string_view name,
    BitField<AttributeElement> expected_element,
    AttributeUsage expected_usage,
    size_t expected_channels)
{
    AttributeId id = invalid_attribute_id();
    la_runtime_assert(!name.empty(), "Attribute name must not be empty!");

    // Check user-provided attribute compatibility
    id = mesh.get_attribute_id(name);
    auto [ok, msg] = check_attribute<ExpectedValueType>(
        mesh,
        id,
        expected_element,
        expected_usage,
        expected_channels,
        ShouldBeWritable::No);
    la_runtime_assert(ok, msg);

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
        id = mesh.template create_attribute<ExpectedValueType>(
            name,
            expected_element,
            expected_usage,
            expected_channels);
    } else {
        // Sanity checks on user-given attribute name
        id = mesh.get_attribute_id(name);
        auto [ok, msg] = check_attribute<ExpectedValueType>(
            mesh,
            id,
            expected_element,
            expected_usage,
            expected_channels,
            ShouldBeWritable::Yes);
        if (!ok) {
            logger().debug(
                "Attribute {} already exists, but is not compatible with the requested attribute. "
                "Deleting it and creating a new one.",
                name);
            mesh.delete_attribute(name);
            id = mesh.template create_attribute<ExpectedValueType>(
                name,
                expected_element,
                expected_usage,
                expected_channels);
        }
        if (reset_tag == ResetToDefault::Yes) {
            if (expected_element != Indexed) {
                auto& attr = mesh.template ref_attribute<ExpectedValueType>(id);
                std::fill(attr.ref_all().begin(), attr.ref_all().end(), attr.get_default_value());
            } else {
                auto& attr = mesh.template ref_indexed_attribute<ExpectedValueType>(id).values();
                std::fill(attr.ref_all().begin(), attr.ref_all().end(), attr.get_default_value());
            }
        }
        logger().debug("Attribute {} already exists, reusing it.", name);
    }
    return id;
}

// Iterate over attribute types x mesh (scalar, index) types
#define LA_X_find_attribute(ExpectedValueType, Scalar, Index)                                    \
    template LA_CORE_API AttributeId find_matching_attribute<ExpectedValueType, Scalar, Index>(  \
        const SurfaceMesh<Scalar, Index>& mesh,                                                  \
        std::string_view name,                                                                   \
        BitField<AttributeElement> expected_element,                                             \
        AttributeUsage expected_usage,                                                           \
        size_t expected_channels);                                                               \
    template LA_CORE_API AttributeId find_matching_attribute<ExpectedValueType, Scalar, Index>(  \
        const SurfaceMesh<Scalar, Index>& mesh,                                                  \
        span<const AttributeId>,                                                                 \
        BitField<AttributeElement> expected_element,                                             \
        AttributeUsage expected_usage,                                                           \
        size_t expected_channels);                                                               \
    template LA_CORE_API AttributeId find_attribute<ExpectedValueType, Scalar, Index>(           \
        const SurfaceMesh<Scalar, Index>& mesh,                                                  \
        std::string_view name,                                                                   \
        BitField<AttributeElement> expected_element,                                             \
        AttributeUsage expected_usage,                                                           \
        size_t expected_channels);                                                               \
    template LA_CORE_API AttributeId find_or_create_attribute<ExpectedValueType, Scalar, Index>( \
        SurfaceMesh<Scalar, Index> & mesh,                                                       \
        std::string_view name,                                                                   \
        AttributeElement expected_element,                                                       \
        AttributeUsage expected_usage,                                                           \
        size_t expected_channels,                                                                \
        ResetToDefault reset_tag);
#define LA_X_find_attribute_aux(_, ExpectedValueType) \
    LA_SURFACE_MESH_X(find_attribute, ExpectedValueType)
LA_ATTRIBUTE_X(find_attribute_aux, 0)

} // namespace lagrange::internal
