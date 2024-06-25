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
#include <lagrange/find_matching_attributes.h>

#include <lagrange/Attribute.h>
#include <lagrange/SurfaceMeshTypes.h>

namespace lagrange {

namespace {

template <typename Index>
bool is_matching(
    const AttributeBase& attr,
    const BitField<AttributeUsage>& usages,
    const BitField<AttributeElement>& element_types,
    Index num_channels)
{
    return usages.test(attr.get_usage()) && element_types.test(attr.get_element_type()) &&
           (num_channels == 0 || attr.get_num_channels() == num_channels);
}

} // namespace

// -----------------------------------------------------------------------------

template <typename Scalar, typename Index>
std::optional<AttributeId> find_matching_attribute(
    const SurfaceMesh<Scalar, Index>& mesh,
    const AttributeMatcher& options)
{
    std::optional<AttributeId> result;
    mesh.seq_foreach_attribute_id([&](AttributeId id) {
        if (result.has_value()) {
            return;
        }
        if (mesh.attr_name_is_reserved(mesh.get_attribute_name(id))) {
            return;
        }
        const auto& attr = mesh.get_attribute_base(id);
        if (is_matching(attr, options.usages, options.element_types, options.num_channels)) {
            result = id;
        }
    });
    return result;
}

template <typename Scalar, typename Index>
std::optional<AttributeId> find_matching_attribute(
    const SurfaceMesh<Scalar, Index>& mesh,
    AttributeUsage usage)
{
    AttributeMatcher options;
    options.usages = usage;
    return find_matching_attribute(mesh, options);
}

template <typename Scalar, typename Index>
std::optional<AttributeId> find_matching_attribute(
    const SurfaceMesh<Scalar, Index>& mesh,
    BitField<AttributeElement> element_types)
{
    AttributeMatcher options;
    options.element_types = element_types;
    return find_matching_attribute(mesh, options);
}

// -----------------------------------------------------------------------------

template <typename Scalar, typename Index>
std::vector<AttributeId> find_matching_attributes(
    const SurfaceMesh<Scalar, Index>& mesh,
    const AttributeMatcher& options)
{
    std::vector<AttributeId> result;
    mesh.seq_foreach_attribute_id([&](AttributeId id) {
        if (mesh.attr_name_is_reserved(mesh.get_attribute_name(id))) {
            return;
        }
        const auto& attr = mesh.get_attribute_base(id);
        if (is_matching(attr, options.usages, options.element_types, options.num_channels)) {
            result.push_back(id);
        }
    });
    return result;
}

template <typename Scalar, typename Index>
std::vector<AttributeId> find_matching_attributes(
    const SurfaceMesh<Scalar, Index>& mesh,
    AttributeUsage usage)
{
    AttributeMatcher options;
    options.usages = usage;
    return find_matching_attributes(mesh, options);
}

template <typename Scalar, typename Index>
std::vector<AttributeId> find_matching_attributes(
    const SurfaceMesh<Scalar, Index>& mesh,
    BitField<AttributeElement> element_types)
{
    AttributeMatcher options;
    options.element_types = element_types;
    return find_matching_attributes(mesh, options);
}

// -----------------------------------------------------------------------------

#define LA_X_find_matching_attribute(_, Scalar, Index)                       \
    template LA_CORE_API std::optional<AttributeId> find_matching_attribute( \
        const SurfaceMesh<Scalar, Index>& mesh,                              \
        const AttributeMatcher& options);                                    \
    template LA_CORE_API std::optional<AttributeId> find_matching_attribute( \
        const SurfaceMesh<Scalar, Index>& mesh,                              \
        AttributeUsage usage);                                               \
    template LA_CORE_API std::optional<AttributeId> find_matching_attribute( \
        const SurfaceMesh<Scalar, Index>& mesh,                              \
        BitField<AttributeElement> element_types);                           \
    template LA_CORE_API std::vector<AttributeId> find_matching_attributes(  \
        const SurfaceMesh<Scalar, Index>& mesh,                              \
        const AttributeMatcher& options);                                    \
    template LA_CORE_API std::vector<AttributeId> find_matching_attributes(  \
        const SurfaceMesh<Scalar, Index>& mesh,                              \
        AttributeUsage usage);                                               \
    template LA_CORE_API std::vector<AttributeId> find_matching_attributes(  \
        const SurfaceMesh<Scalar, Index>& mesh,                              \
        BitField<AttributeElement> element_types);

LA_SURFACE_MESH_X(find_matching_attribute, 0)

} // namespace lagrange
