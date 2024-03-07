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
#include <lagrange/SurfaceMesh.h>

#include <lagrange/Attribute.h>
#include <lagrange/AttributeTypes.h>
#include <lagrange/AttributeValueType.h>
#include <lagrange/IndexedAttribute.h>
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/foreach_attribute.h>
#include <lagrange/internal/attribute_string_utils.h>
#include <lagrange/utils/Error.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/copy_on_write_ptr.h>
#include <lagrange/utils/invalid.h>
#include <lagrange/utils/strings.h>
#include <lagrange/utils/warning.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <tbb/parallel_for.h>
#include <tbb/parallel_sort.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <array>
#include <map>
#include <string>

namespace lagrange {

////////////////////////////////////////////////////////////////////////////////
// Mesh attributes
////////////////////////////////////////////////////////////////////////////////

///
/// @cond LA_INTERNAL_DOCS
/// Hidden attribute manager class.
///
template <typename Scalar, typename Index>
struct SurfaceMesh<Scalar, Index>::AttributeManager
{
    template <typename ValueType>
    AttributeId create(
        std::string_view name,
        AttributeElement element,
        AttributeUsage usage,
        size_t num_channels)
    {
        la_debug_assert(element != AttributeElement::Indexed);
        auto id = create_id(name);
        m_attributes.at(id).first = name;
        m_attributes.at(id).second = copy_on_write_ptr<AttributeBase>(
            internal::make_shared<Attribute<ValueType>>(element, usage, num_channels));
        return id;
    }

    template <typename ValueType>
    AttributeId create_indexed(std::string_view name, AttributeUsage usage, size_t num_channels)
    {
        auto id = create_id(name);
        m_attributes.at(id).first = name;
        m_attributes.at(id).second = copy_on_write_ptr<AttributeBase>(
            internal::make_shared<IndexedAttribute<ValueType, Index>>(usage, num_channels));
        return id;
    }

    template <typename TargetValueType, typename SourceValueType>
    AttributeId cast_from(std::string_view name, const Attribute<SourceValueType>& attr)
    {
        auto id = create_id(name);
        m_attributes.at(id).first = name;
        m_attributes.at(id).second =
            copy_on_write_ptr<AttributeBase>(internal::make_shared<Attribute<TargetValueType>>(
                Attribute<TargetValueType>::cast_copy(attr)));
        return id;
    }

    AttributeId create(std::string_view name, copy_on_write_ptr<AttributeBase> attr)
    {
        auto id = create_id(name);
        m_attributes.at(id).first = name;
        m_attributes.at(id).second = std::move(attr);
        return id;
    }

    [[nodiscard]] AttributeId get_id(std::string_view name) const
    {
        std::string key(name);
        auto it = m_name_to_id.find(key);
        if (it != m_name_to_id.end()) {
            return it->second;
        } else {
            return invalid_attribute_id();
        }
    }

    [[nodiscard]] std::string_view get_name(AttributeId id) const
    {
        return m_attributes.at(id).first;
    }

    [[nodiscard]] bool contains(std::string_view name) const
    {
        std::string key(name);
        return m_name_to_id.count(key) > 0;
    }

    void seq_foreach_attribute_id(function_ref<void(AttributeId)> func) const
    {
        for (auto& kv : m_name_to_id) {
            func(kv.second);
        }
    }

    void seq_foreach_attribute_id(function_ref<void(std::string_view, AttributeId)> func) const
    {
        for (auto& kv : m_name_to_id) {
            func(kv.first, kv.second);
        }
    }

    void par_foreach_attribute_id(function_ref<void(AttributeId)> func) const
    {
        size_t num_attributes = m_attributes.size();
        tbb::parallel_for(size_t(0), num_attributes, [&](size_t i) {
            if (m_attributes[i].second.read()) {
                func(AttributeId(i));
            }
        });
    }

    void par_foreach_attribute_id(function_ref<void(std::string_view, AttributeId)> func) const
    {
        size_t num_attributes = m_attributes.size();
        tbb::parallel_for(size_t(0), num_attributes, [&](size_t i) {
            if (m_attributes[i].second.read()) {
                func(m_attributes[i].first, AttributeId(i));
            }
        });
    }

    size_t erase(std::string_view name)
    {
        std::string key(name);
        auto it = m_name_to_id.find(key);
        if (it != m_name_to_id.end()) {
            const AttributeId id = it->second;
            m_free_ids.push_back(id);
            m_attributes.at(id) = std::pair<std::string, copy_on_write_ptr<AttributeBase>>();
            m_name_to_id.erase(it);
            return 1;
        } else {
            return 0;
        }
    }

    void rename(std::string_view old_name, std::string_view new_name)
    {
        std::string old_key(old_name);
        std::string new_key(new_name);
        auto it_old = m_name_to_id.find(old_key);
        auto it_new = m_name_to_id.find(new_key);
        if (it_old == m_name_to_id.end()) {
            throw Error(fmt::format("Source attribute '{}' does not exist", old_name));
        }
        if (it_new != m_name_to_id.end()) {
            throw Error(fmt::format("Target attribute '{}' already exist", new_name));
        } else {
            AttributeId id = it_old->second;
            m_name_to_id.erase(it_old);
            m_name_to_id.emplace_hint(it_new, new_key, id);
            m_attributes.at(id).first = new_name;
        }
    }

    [[nodiscard]] copy_on_write_ptr<AttributeBase> copy_ptr(AttributeId id) const
    {
        return m_attributes.at(id).second;
    }

    [[nodiscard]] copy_on_write_ptr<AttributeBase> move_ptr(AttributeId id)
    {
        return std::move(m_attributes.at(id).second);
    }

    [[nodiscard]] const AttributeBase& read_base(AttributeId id) const
    {
        auto& ptr = m_attributes.at(id).second;
        la_debug_assert(ptr);
        return *ptr.read();
    }

    template <typename ValueType>
    [[nodiscard]] Attribute<ValueType>& write(AttributeId id)
    {
        auto& ptr = m_attributes.at(id).second;
        la_debug_assert(ptr);
        return *ptr.template static_write<Attribute<ValueType>>();
    }

    template <typename ValueType>
    [[nodiscard]] IndexedAttribute<ValueType, Index>& write_indexed(AttributeId id)
    {
        auto& ptr = m_attributes.at(id).second;
        la_debug_assert(ptr);
        return *ptr.template static_write<IndexedAttribute<ValueType, Index>>();
    }

    template <typename ValueType>
    [[nodiscard]] const Attribute<ValueType>& read(AttributeId id) const
    {
        auto& ptr = m_attributes.at(id).second;
        la_debug_assert(ptr);
        return *ptr.template static_read<Attribute<ValueType>>();
    }

    template <typename ValueType>
    [[nodiscard]] const IndexedAttribute<ValueType, Index>& read_indexed(AttributeId id) const
    {
        auto& ptr = m_attributes.at(id).second;
        la_debug_assert(ptr);
        return *ptr.template static_read<IndexedAttribute<ValueType, Index>>();
    }

    template <typename ValueType>
    [[nodiscard]] std::shared_ptr<Attribute<ValueType>> release_ptr(AttributeId id)
    {
        auto& ptr = m_attributes.at(id).second;
        la_debug_assert(ptr);
        return ptr.template release_ptr<Attribute<ValueType>>();
    }

    template <typename ValueType>
    [[nodiscard]] std::shared_ptr<IndexedAttribute<ValueType, Index>> release_indexed_ptr(
        AttributeId id)
    {
        auto& ptr = m_attributes.at(id).second;
        la_debug_assert(ptr);
        return ptr.template release_ptr<IndexedAttribute<ValueType, Index>>();
    }

    [[nodiscard]] internal::weak_ptr<const AttributeBase> _get_weak_ptr(AttributeId id) const
    {
        auto& ptr = m_attributes.at(id).second;
        la_debug_assert(ptr);
        return ptr._get_weak_ptr();
    }

    [[nodiscard]] internal::weak_ptr<AttributeBase> _ref_weak_ptr(AttributeId id)
    {
        auto& ptr = m_attributes.at(id).second;
        la_debug_assert(ptr);
        return ptr._get_weak_ptr();
    }

protected:
    AttributeId create_id(std::string_view name)
    {
        // Note: heterogenous lookup is only available in C++20!
        // https://en.cppreference.com/w/cpp/container/unordered_map/find
        std::string key(name);
        auto [it, insertion_successful] = m_name_to_id.emplace(key, 0);
        if (insertion_successful) {
            // Key didn't exist, we need to allocate a new id
            if (!m_free_ids.empty()) {
                // Reuse an existing slot
                it->second = m_free_ids.back();
                m_free_ids.pop_back();
            } else {
                // Allocate a new slot
                it->second = static_cast<AttributeId>(m_attributes.size());
                m_attributes.emplace_back();
            }
        } else {
            la_runtime_assert(false, fmt::format("Attribute '{}' already exist!", name));
        }
        return it->second;
    }

protected:
    // NOTE: We store the attribute name twice:
    // - In the map name -> attr id
    // - In the list id -> name x attr data
    //
    // The reason for that is to make it easily to iterate in parallel over all attributes without
    // having to create a temporary buffer of names/ids. Maybe this means we should store the name
    // inside the attribute class directly, but we would still need to store duplicated storage for
    // the map name -> attr id.

    ///
    /// Map attribute names -> attribute ids.
    ///
    /// @note       We use an ordered map to guarantee a consistent traversal order by our
    ///             `seq_foreach_attribute_id()` method. If performance becomes an issue, we should
    ///             switch to a third-party implementation of an unordered_map (but we should avoid
    ///             the STL implementation due to inconsistencies between macOS/Linux/Windows).
    ///
    std::map<std::string, AttributeId> m_name_to_id;

    /// Direct addressing of attributes using attribute ids.
    std::vector<std::pair<std::string, copy_on_write_ptr<AttributeBase>>> m_attributes;

    /// List of previously erased attribute ids.
    std::vector<AttributeId> m_free_ids;
};

namespace {

template <typename ValueType>
bool ensure_positive(ValueType x)
{
    if constexpr (std::is_unsigned_v<ValueType>) {
        LA_IGNORE(x);
        return true;
    } else {
        return x >= 0;
    }
}

} // namespace

/// @endcond

////////////////////////////////////////////////////////////////////////////////
// Mesh attributes
////////////////////////////////////////////////////////////////////////////////

template <typename Scalar, typename Index>
AttributeId SurfaceMesh<Scalar, Index>::get_attribute_id(std::string_view name) const
{
    auto ret = m_attributes->get_id(name);
    if (ret == invalid_attribute_id()) {
        throw Error(fmt::format("Attribute '{}' does not exist.", name));
    }
    return ret;
}

template <typename Scalar, typename Index>
std::string_view SurfaceMesh<Scalar, Index>::get_attribute_name(AttributeId id) const
{
    return m_attributes->get_name(id);
}

template <typename Scalar, typename Index>
size_t SurfaceMesh<Scalar, Index>::get_num_elements_internal(AttributeElement element) const
{
    switch (element) {
    case AttributeElement::Vertex: return static_cast<size_t>(get_num_vertices());
    case AttributeElement::Facet: return static_cast<size_t>(get_num_facets());
    case AttributeElement::Edge: return static_cast<size_t>(get_num_edges());
    case AttributeElement::Corner: return static_cast<size_t>(get_num_corners());
    case AttributeElement::Indexed: return static_cast<size_t>(get_num_corners());
    case AttributeElement::Value: return 0;
    default: la_runtime_assert(false, "Invalid element type");
    }
}

template <typename Scalar, typename Index>
template <typename ValueType>
void SurfaceMesh<Scalar, Index>::set_attribute_default_internal(std::string_view name)
{
    auto& attr = m_attributes->template write<ValueType>(get_attribute_id(name));
    bool is_reserved = starts_with(name, "$");
    if constexpr (std::is_arithmetic_v<ValueType>) {
        if (is_reserved) {
            if (name == s_reserved_names.vertex_to_position() ||
                name == s_reserved_names.corner_to_vertex()) {
                attr.set_default_value(ValueType(0));
            } else if (
                name == s_reserved_names.facet_to_first_corner() ||
                name == s_reserved_names.corner_to_facet() ||
                name == s_reserved_names.corner_to_edge() ||
                name == s_reserved_names.edge_to_first_corner() ||
                name == s_reserved_names.next_corner_around_edge() ||
                name == s_reserved_names.vertex_to_first_corner() ||
                name == s_reserved_names.next_corner_around_vertex()) {
                attr.set_default_value(invalid<ValueType>());
            } else {
                throw Error(fmt::format(
                    "Attribute name '{}' is not a valid reserved attribute name",
                    name));
            }
        }
    }
}

template <typename Scalar, typename Index>
template <typename ValueType>
AttributeId SurfaceMesh<Scalar, Index>::create_attribute(
    std::string_view name,
    AttributeElement element,
    size_t num_channels,
    AttributeUsage usage,
    span<const ValueType> initial_values,
    span<const Index> initial_indices,
    AttributeCreatePolicy policy)
{
    if (policy == AttributeCreatePolicy::ErrorIfReserved) {
        la_runtime_assert(
            !starts_with(name, "$"),
            fmt::format("Attribute name is reserved: {}", name));
    }
    return create_attribute_internal(
        name,
        element,
        usage,
        num_channels,
        initial_values,
        initial_indices);
}

template <typename Scalar, typename Index>
template <typename ValueType>
AttributeId SurfaceMesh<Scalar, Index>::create_attribute(
    std::string_view name,
    AttributeElement element,
    AttributeUsage usage,
    size_t num_channels,
    span<const ValueType> initial_values,
    span<const Index> initial_indices,
    AttributeCreatePolicy policy)
{
    return create_attribute(
        name,
        element,
        num_channels,
        usage,
        initial_values,
        initial_indices,
        policy);
}

template <typename Scalar, typename Index>
template <typename ValueType>
AttributeId SurfaceMesh<Scalar, Index>::create_attribute_internal(
    std::string_view name,
    AttributeElement element,
    AttributeUsage usage,
    size_t num_channels,
    span<const ValueType> initial_values,
    span<const Index> initial_indices)
{
    // If usage tag indicates a "position", check that num_channels == dim
    if (usage == AttributeUsage::Position) {
        la_runtime_assert(
            num_channels == get_dimension(),
            fmt::format(
                "Invalid number of channels for {} attribute: should be {}.",
                internal::to_string(usage),
                get_dimension()));
    }
    // If usage tag indicates a "normal/tangent/bitangent", check that num_channels == dim or dim + 1
    if (usage == AttributeUsage::Normal || usage == AttributeUsage::Tangent ||
        usage == AttributeUsage::Bitangent) {
        la_runtime_assert(
            num_channels == get_dimension() || num_channels == get_dimension() + 1,
            fmt::format(
                "Invalid number of channels for {} attribute: should be {} or {} + 1.",
                internal::to_string(usage),
                get_dimension(),
                get_dimension()));
    }

    // If usage is an element index type, then ValueType must be the same as the mesh's Index type.
    if (usage == AttributeUsage::VertexIndex || usage == AttributeUsage::FacetIndex ||
        usage == AttributeUsage::CornerIndex || usage == AttributeUsage::EdgeIndex) {
        la_runtime_assert((std::is_same_v<ValueType, Index>));
    }

    if (element == AttributeElement::Indexed) {
        // Indexed attribute
        const size_t num_corners = get_num_elements_internal(element);
        la_runtime_assert(initial_values.size() % num_channels == 0);
        la_runtime_assert(initial_indices.empty() || initial_indices.size() == num_corners);
        auto id = m_attributes->template create_indexed<ValueType>(name, usage, num_channels);
        auto& attr = m_attributes->template write_indexed<ValueType>(id);
        if (!initial_values.empty()) {
            attr.values().insert_elements(initial_values);
        }
        if (initial_indices.empty()) {
            attr.indices().insert_elements(num_corners);
        } else {
            attr.indices().insert_elements(initial_indices);
        }
        la_debug_assert(
            get_indexed_attribute<ValueType>(id).indices().get_num_elements() == num_corners);
        return id;
    } else {
        // Non-indexed attribute
        const size_t num_elements = get_num_elements_internal(element);
        if (element != AttributeElement::Value) {
            la_runtime_assert(
                initial_values.empty() || initial_values.size() == num_elements * num_channels);
        }
        la_runtime_assert(
            initial_indices.empty(),
            "Cannot provide non-empty index buffer for non-indexed attribute");
        auto id = m_attributes->template create<ValueType>(name, element, usage, num_channels);
        set_attribute_default_internal<ValueType>(name);
        auto& attr = m_attributes->template write<ValueType>(id);
        if (initial_values.empty()) {
            attr.insert_elements(num_elements);
        } else {
            attr.insert_elements(initial_values);
        }
        if (element != AttributeElement::Value) {
            la_debug_assert(get_attribute<ValueType>(id).get_num_elements() == num_elements);
        }
        return id;
    }
}

template <typename Scalar, typename Index>
template <typename OtherScalar, typename OtherIndex>
AttributeId SurfaceMesh<Scalar, Index>::create_attribute_from(
    std::string_view name,
    const SurfaceMesh<OtherScalar, OtherIndex>& source_mesh,
    std::string_view source_name)
{
    la_runtime_assert(!starts_with(name, "$"), fmt::format("Attribute name is reserved: {}", name));
    if (source_name.empty()) source_name = name;
    const AttributeId source_id = source_mesh.get_attribute_id(source_name);
    const AttributeBase& source_attr = source_mesh.m_attributes->read_base(source_id);
    const size_t source_num_elements =
        source_mesh.get_num_elements_internal(source_attr.get_element_type());
    const size_t target_num_elements =
        this->get_num_elements_internal(source_attr.get_element_type());
    la_runtime_assert(source_num_elements == target_num_elements);

    return m_attributes->create(name, source_mesh.m_attributes->copy_ptr(source_id));
}

template <typename Scalar, typename Index>
template <typename ValueType>
AttributeId SurfaceMesh<Scalar, Index>::wrap_as_attribute(
    std::string_view name,
    AttributeElement element,
    AttributeUsage usage,
    size_t num_channels,
    span<ValueType> values_view)
{
    la_runtime_assert(element != AttributeElement::Indexed, "Element type must not be Indexed");
    la_runtime_assert(!starts_with(name, "$"), fmt::format("Attribute name is reserved: {}", name));

    const size_t num_elements = get_num_elements_internal(element);
    return wrap_as_attribute_internal(
        name,
        element,
        usage,
        num_elements,
        num_channels,
        values_view);
}

template <typename Scalar, typename Index>
template <typename ValueType>
AttributeId SurfaceMesh<Scalar, Index>::wrap_as_attribute(
    std::string_view name,
    AttributeElement element,
    AttributeUsage usage,
    size_t num_channels,
    SharedSpan<ValueType> shared_values)
{
    la_runtime_assert(element != AttributeElement::Indexed, "Element type must not be Indexed");
    la_runtime_assert(!starts_with(name, "$"), fmt::format("Attribute name is reserved: {}", name));

    const size_t num_elements = get_num_elements_internal(element);
    return wrap_as_attribute_internal(
        name,
        element,
        usage,
        num_elements,
        num_channels,
        shared_values);
}

template <typename Scalar, typename Index>
template <typename ValueType>
AttributeId SurfaceMesh<Scalar, Index>::wrap_as_const_attribute(
    std::string_view name,
    AttributeElement element,
    AttributeUsage usage,
    size_t num_channels,
    span<const ValueType> values_view)
{
    la_runtime_assert(element != AttributeElement::Indexed, "Element type must not be Indexed");
    la_runtime_assert(!starts_with(name, "$"), fmt::format("Attribute name is reserved: {}", name));

    const size_t num_elements = get_num_elements_internal(element);
    return wrap_as_attribute_internal(
        name,
        element,
        usage,
        num_elements,
        num_channels,
        values_view);
}

template <typename Scalar, typename Index>
template <typename ValueType>
AttributeId SurfaceMesh<Scalar, Index>::wrap_as_const_attribute(
    std::string_view name,
    AttributeElement element,
    AttributeUsage usage,
    size_t num_channels,
    SharedSpan<const ValueType> shared_values)
{
    la_runtime_assert(element != AttributeElement::Indexed, "Element type must not be Indexed");
    la_runtime_assert(!starts_with(name, "$"), fmt::format("Attribute name is reserved: {}", name));

    const size_t num_elements = get_num_elements_internal(element);
    return wrap_as_attribute_internal(
        name,
        element,
        usage,
        num_elements,
        num_channels,
        shared_values);
}

template <typename Scalar, typename Index>
template <typename ValueType>
AttributeId SurfaceMesh<Scalar, Index>::wrap_as_indexed_attribute(
    std::string_view name,
    AttributeUsage usage,
    size_t num_values,
    size_t num_channels,
    span<ValueType> values_view,
    span<Index> indices_view)
{
    la_runtime_assert(!starts_with(name, "$"), fmt::format("Attribute name is reserved: {}", name));
    return wrap_as_attribute_internal(
        name,
        AttributeElement::Indexed,
        usage,
        num_values,
        num_channels,
        values_view,
        indices_view);
}

template <typename Scalar, typename Index>
template <typename ValueType>
AttributeId SurfaceMesh<Scalar, Index>::wrap_as_indexed_attribute(
    std::string_view name,
    AttributeUsage usage,
    size_t num_values,
    size_t num_channels,
    SharedSpan<ValueType> shared_values,
    SharedSpan<Index> shared_indices)
{
    la_runtime_assert(!starts_with(name, "$"), fmt::format("Attribute name is reserved: {}", name));
    return wrap_as_attribute_internal(
        name,
        AttributeElement::Indexed,
        usage,
        num_values,
        num_channels,
        shared_values,
        shared_indices);
}

template <typename Scalar, typename Index>
template <typename ValueType>
AttributeId SurfaceMesh<Scalar, Index>::wrap_as_indexed_attribute(
    std::string_view name,
    AttributeUsage usage,
    size_t num_values,
    size_t num_channels,
    span<ValueType> values_view,
    SharedSpan<Index> shared_indices)
{
    la_runtime_assert(!starts_with(name, "$"), fmt::format("Attribute name is reserved: {}", name));
    return wrap_as_attribute_internal(
        name,
        AttributeElement::Indexed,
        usage,
        num_values,
        num_channels,
        values_view,
        shared_indices);
}

template <typename Scalar, typename Index>
template <typename ValueType>
AttributeId SurfaceMesh<Scalar, Index>::wrap_as_indexed_attribute(
    std::string_view name,
    AttributeUsage usage,
    size_t num_values,
    size_t num_channels,
    SharedSpan<ValueType> shared_values,
    span<Index> indices_view)
{
    la_runtime_assert(!starts_with(name, "$"), fmt::format("Attribute name is reserved: {}", name));
    return wrap_as_attribute_internal(
        name,
        AttributeElement::Indexed,
        usage,
        num_values,
        num_channels,
        shared_values,
        indices_view);
}

template <typename Scalar, typename Index>
template <typename ValueType>
AttributeId SurfaceMesh<Scalar, Index>::wrap_as_const_indexed_attribute(
    std::string_view name,
    AttributeUsage usage,
    size_t num_values,
    size_t num_channels,
    span<const ValueType> values_view,
    span<const Index> indices_view)
{
    la_runtime_assert(!starts_with(name, "$"), fmt::format("Attribute name is reserved: {}", name));
    return wrap_as_attribute_internal(
        name,
        AttributeElement::Indexed,
        usage,
        num_values,
        num_channels,
        values_view,
        indices_view);
}

template <typename Scalar, typename Index>
template <typename ValueType>
AttributeId SurfaceMesh<Scalar, Index>::wrap_as_const_indexed_attribute(
    std::string_view name,
    AttributeUsage usage,
    size_t num_values,
    size_t num_channels,
    SharedSpan<const ValueType> shared_values,
    SharedSpan<const Index> shared_indices)
{
    la_runtime_assert(!starts_with(name, "$"), fmt::format("Attribute name is reserved: {}", name));
    return wrap_as_attribute_internal(
        name,
        AttributeElement::Indexed,
        usage,
        num_values,
        num_channels,
        shared_values,
        shared_indices);
}

template <typename Scalar, typename Index>
template <typename ValueType>
AttributeId SurfaceMesh<Scalar, Index>::wrap_as_const_indexed_attribute(
    std::string_view name,
    AttributeUsage usage,
    size_t num_values,
    size_t num_channels,
    span<const ValueType> values_view,
    SharedSpan<const Index> shared_indices)
{
    la_runtime_assert(!starts_with(name, "$"), fmt::format("Attribute name is reserved: {}", name));
    return wrap_as_attribute_internal(
        name,
        AttributeElement::Indexed,
        usage,
        num_values,
        num_channels,
        values_view,
        shared_indices);
}

template <typename Scalar, typename Index>
template <typename ValueType>
AttributeId SurfaceMesh<Scalar, Index>::wrap_as_const_indexed_attribute(
    std::string_view name,
    AttributeUsage usage,
    size_t num_values,
    size_t num_channels,
    SharedSpan<const ValueType> shared_values,
    span<const Index> indices_view)
{
    la_runtime_assert(!starts_with(name, "$"), fmt::format("Attribute name is reserved: {}", name));
    return wrap_as_attribute_internal(
        name,
        AttributeElement::Indexed,
        usage,
        num_values,
        num_channels,
        shared_values,
        indices_view);
}

template <typename Scalar, typename Index>
AttributeId SurfaceMesh<Scalar, Index>::wrap_as_vertices(
    span<Scalar> vertices_view,
    Index num_vertices)
{
    la_runtime_assert(vertices_view.size() >= num_vertices * get_dimension());
    auto& attr = m_attributes->template write<Scalar>(m_reserved_ids.vertex_to_position());
    attr.wrap(vertices_view, num_vertices);
    resize_vertices_internal(num_vertices);
    return m_reserved_ids.vertex_to_position();
}

template <typename Scalar, typename Index>
AttributeId SurfaceMesh<Scalar, Index>::wrap_as_vertices(
    SharedSpan<Scalar> shared_vertices,
    Index num_vertices)
{
    la_runtime_assert(shared_vertices.size() >= num_vertices * get_dimension());
    auto& attr = m_attributes->template write<Scalar>(m_reserved_ids.vertex_to_position());
    attr.wrap(std::move(shared_vertices), num_vertices);
    resize_vertices_internal(num_vertices);
    return m_reserved_ids.vertex_to_position();
}

template <typename Scalar, typename Index>
AttributeId SurfaceMesh<Scalar, Index>::wrap_as_const_vertices(
    span<const Scalar> vertices_view,
    Index num_vertices)
{
    la_runtime_assert(vertices_view.size() >= num_vertices * get_dimension());
    auto& attr = m_attributes->template write<Scalar>(m_reserved_ids.vertex_to_position());
    attr.wrap_const(vertices_view, num_vertices);
    resize_vertices_internal(num_vertices);
    return m_reserved_ids.vertex_to_position();
}

template <typename Scalar, typename Index>
AttributeId SurfaceMesh<Scalar, Index>::wrap_as_const_vertices(
    SharedSpan<const Scalar> shared_vertices,
    Index num_vertices)
{
    la_runtime_assert(shared_vertices.size() >= num_vertices * get_dimension());
    auto& attr = m_attributes->template write<Scalar>(m_reserved_ids.vertex_to_position());
    attr.wrap_const(std::move(shared_vertices), num_vertices);
    resize_vertices_internal(num_vertices);
    return m_reserved_ids.vertex_to_position();
}

template <typename Scalar, typename Index>
AttributeId SurfaceMesh<Scalar, Index>::wrap_as_facets(
    span<Index> facets_view,
    Index num_facets,
    Index vertex_per_facet)
{
    la_runtime_assert(facets_view.size() >= num_facets * vertex_per_facet);
    if (m_reserved_ids.facet_to_first_corner() != invalid_attribute_id()) {
        // Mesh is regular, so delete hybrid storage attributes
        la_debug_assert(m_reserved_ids.corner_to_facet() != invalid_attribute_id());
        delete_attribute(s_reserved_names.facet_to_first_corner(), AttributeDeletePolicy::Force);
        delete_attribute(s_reserved_names.corner_to_facet(), AttributeDeletePolicy::Force);
    }
    m_vertex_per_facet = vertex_per_facet;
    auto& attr = m_attributes->template write<Index>(m_reserved_ids.corner_to_vertex());
    attr.wrap(facets_view, num_facets * vertex_per_facet);
    resize_facets_internal(num_facets);
    resize_corners_internal(num_facets * vertex_per_facet);
    return m_reserved_ids.corner_to_vertex();
}

template <typename Scalar, typename Index>
AttributeId SurfaceMesh<Scalar, Index>::wrap_as_facets(
    SharedSpan<Index> shared_facets,
    Index num_facets,
    Index vertex_per_facet)
{
    la_runtime_assert(shared_facets.size() >= num_facets * vertex_per_facet);
    if (m_reserved_ids.facet_to_first_corner() != invalid_attribute_id()) {
        // Mesh is regular, so delete hybrid storage attributes
        la_debug_assert(m_reserved_ids.corner_to_facet() != invalid_attribute_id());
        delete_attribute(s_reserved_names.facet_to_first_corner(), AttributeDeletePolicy::Force);
        delete_attribute(s_reserved_names.corner_to_facet(), AttributeDeletePolicy::Force);
    }
    m_vertex_per_facet = vertex_per_facet;
    auto& attr = m_attributes->template write<Index>(m_reserved_ids.corner_to_vertex());
    attr.wrap(std::move(shared_facets), num_facets * vertex_per_facet);
    resize_facets_internal(num_facets);
    resize_corners_internal(num_facets * vertex_per_facet);
    return m_reserved_ids.corner_to_vertex();
}

template <typename Scalar, typename Index>
AttributeId SurfaceMesh<Scalar, Index>::wrap_as_const_facets(
    span<const Index> facets_view,
    Index num_facets,
    Index vertex_per_facet)
{
    la_runtime_assert(facets_view.size() >= num_facets * vertex_per_facet);
    if (m_reserved_ids.facet_to_first_corner() != invalid_attribute_id()) {
        // Mesh is regular, so delete hybrid storage attributes
        la_debug_assert(m_reserved_ids.corner_to_facet() != invalid_attribute_id());
        delete_attribute(s_reserved_names.facet_to_first_corner(), AttributeDeletePolicy::Force);
        delete_attribute(s_reserved_names.corner_to_facet(), AttributeDeletePolicy::Force);
    }
    m_vertex_per_facet = vertex_per_facet;
    auto& attr = m_attributes->template write<Index>(m_reserved_ids.corner_to_vertex());
    attr.wrap_const(facets_view, num_facets * vertex_per_facet);
    resize_facets_internal(num_facets);
    resize_corners_internal(num_facets * vertex_per_facet);
    return m_reserved_ids.corner_to_vertex();
}

template <typename Scalar, typename Index>
AttributeId SurfaceMesh<Scalar, Index>::wrap_as_const_facets(
    SharedSpan<const Index> shared_facets,
    Index num_facets,
    Index vertex_per_facet)
{
    la_runtime_assert(shared_facets.size() >= num_facets * vertex_per_facet);
    if (m_reserved_ids.facet_to_first_corner() != invalid_attribute_id()) {
        // Mesh is regular, so delete hybrid storage attributes
        la_debug_assert(m_reserved_ids.corner_to_facet() != invalid_attribute_id());
        delete_attribute(s_reserved_names.facet_to_first_corner(), AttributeDeletePolicy::Force);
        delete_attribute(s_reserved_names.corner_to_facet(), AttributeDeletePolicy::Force);
    }
    m_vertex_per_facet = vertex_per_facet;
    auto& attr = m_attributes->template write<Index>(m_reserved_ids.corner_to_vertex());
    attr.wrap_const(std::move(shared_facets), num_facets * vertex_per_facet);
    resize_facets_internal(num_facets);
    resize_corners_internal(num_facets * vertex_per_facet);
    return m_reserved_ids.corner_to_vertex();
}

template <typename Scalar, typename Index>
AttributeId SurfaceMesh<Scalar, Index>::wrap_as_facets(
    span<Index> offsets_view,
    Index num_facets,
    span<Index> facets_view,
    Index num_corners)
{
    return wrap_as_facets_internal(offsets_view, num_facets, facets_view, num_corners);
}

template <typename Scalar, typename Index>
AttributeId SurfaceMesh<Scalar, Index>::wrap_as_facets(
    SharedSpan<Index> shared_offsets,
    Index num_facets,
    SharedSpan<Index> shared_facets,
    Index num_corners)
{
    return wrap_as_facets_internal(shared_offsets, num_facets, shared_facets, num_corners);
}

template <typename Scalar, typename Index>
AttributeId SurfaceMesh<Scalar, Index>::wrap_as_facets(
    span<Index> offsets_view,
    Index num_facets,
    SharedSpan<Index> shared_facets,
    Index num_corners)
{
    return wrap_as_facets_internal(offsets_view, num_facets, shared_facets, num_corners);
}

template <typename Scalar, typename Index>
AttributeId SurfaceMesh<Scalar, Index>::wrap_as_facets(
    SharedSpan<Index> shared_offsets,
    Index num_facets,
    span<Index> facets_view,
    Index num_corners)
{
    return wrap_as_facets_internal(shared_offsets, num_facets, facets_view, num_corners);
}

template <typename Scalar, typename Index>
AttributeId SurfaceMesh<Scalar, Index>::wrap_as_const_facets(
    span<const Index> offsets_view,
    Index num_facets,
    span<const Index> facets_view,
    Index num_corners)
{
    return wrap_as_facets_internal(offsets_view, num_facets, facets_view, num_corners);
}

template <typename Scalar, typename Index>
AttributeId SurfaceMesh<Scalar, Index>::wrap_as_const_facets(
    SharedSpan<const Index> shared_offsets,
    Index num_facets,
    SharedSpan<const Index> shared_facets,
    Index num_corners)
{
    return wrap_as_facets_internal(shared_offsets, num_facets, shared_facets, num_corners);
}

template <typename Scalar, typename Index>
AttributeId SurfaceMesh<Scalar, Index>::wrap_as_const_facets(
    span<const Index> offsets_view,
    Index num_facets,
    SharedSpan<const Index> shared_facets,
    Index num_corners)
{
    return wrap_as_facets_internal(offsets_view, num_facets, shared_facets, num_corners);
}

template <typename Scalar, typename Index>
AttributeId SurfaceMesh<Scalar, Index>::wrap_as_const_facets(
    SharedSpan<const Index> shared_offsets,
    Index num_facets,
    span<const Index> facets_view,
    Index num_corners)
{
    return wrap_as_facets_internal(shared_offsets, num_facets, facets_view, num_corners);
}

template <typename Scalar, typename Index>
AttributeId SurfaceMesh<Scalar, Index>::duplicate_attribute(
    std::string_view old_name,
    std::string_view new_name)
{
    la_runtime_assert(
        !starts_with(new_name, "$"),
        fmt::format("Attribute name is reserved: {}", new_name));
    return create_attribute_from(new_name, *this, old_name);
}

template <typename Scalar, typename Index>
void SurfaceMesh<Scalar, Index>::rename_attribute(
    std::string_view old_name,
    std::string_view new_name)
{
    la_runtime_assert(
        !starts_with(new_name, "$"),
        fmt::format("Attribute name is reserved: {}", new_name));
    m_attributes->rename(old_name, new_name);
}

template <typename Scalar, typename Index>
void SurfaceMesh<Scalar, Index>::delete_attribute(
    std::string_view name,
    AttributeDeletePolicy policy)
{
    bool is_reserved = starts_with(name, "$");
    if (is_reserved && policy == AttributeDeletePolicy::ErrorIfReserved) {
        throw Error("Cannot delete a reserved attribute without the force=true option.");
    }
    if (is_reserved) {
        if (name == s_reserved_names.vertex_to_position()) {
            m_reserved_ids.vertex_to_position() = invalid_attribute_id();
        } else if (name == s_reserved_names.corner_to_vertex()) {
            m_reserved_ids.corner_to_vertex() = invalid_attribute_id();
        } else if (name == s_reserved_names.facet_to_first_corner()) {
            m_reserved_ids.facet_to_first_corner() = invalid_attribute_id();
        } else if (name == s_reserved_names.corner_to_facet()) {
            m_reserved_ids.corner_to_facet() = invalid_attribute_id();
        } else if (name == s_reserved_names.corner_to_edge()) {
            m_reserved_ids.corner_to_edge() = invalid_attribute_id();
        } else if (name == s_reserved_names.edge_to_first_corner()) {
            m_reserved_ids.edge_to_first_corner() = invalid_attribute_id();
        } else if (name == s_reserved_names.next_corner_around_edge()) {
            m_reserved_ids.next_corner_around_edge() = invalid_attribute_id();
        } else if (name == s_reserved_names.vertex_to_first_corner()) {
            m_reserved_ids.vertex_to_first_corner() = invalid_attribute_id();
        } else if (name == s_reserved_names.next_corner_around_vertex()) {
            m_reserved_ids.next_corner_around_vertex() = invalid_attribute_id();
        } else {
            throw Error(
                fmt::format("Attribute name '{}' is not a valid reserved attribute name", name));
        }
    }
    size_t num_deleted = m_attributes->erase(name);
    la_runtime_assert(num_deleted == 1, fmt::format("Attribute {} does not exist", name));
}

namespace {

template <typename ValueType>
void check_export_policy(Attribute<ValueType>& attr, AttributeExportPolicy policy)
{
    if (attr.is_external()) {
        switch (policy) {
        case AttributeExportPolicy::CopyIfExternal: attr.create_internal_copy(); return;
        case AttributeExportPolicy::CopyIfUnmanaged:
            if (!attr.is_managed()) {
                attr.create_internal_copy();
            }
            return;
        case AttributeExportPolicy::KeepExternalPtr:
            logger().warn("Exporting an Attribute pointing to an external buffer. It is the user's "
                          "responsibility to guarantee the lifetime of the pointed data in that "
                          "situation.");
            return;
        case AttributeExportPolicy::ErrorIfExternal:
            throw Error("Cannot export an Attribute pointing to an external buffer");
        default: throw Error("Unsupported policy");
        }
    }
}

} // namespace

template <typename Scalar, typename Index>
template <typename ValueType>
std::shared_ptr<Attribute<ValueType>> SurfaceMesh<Scalar, Index>::delete_and_export_attribute(
    std::string_view name,
    AttributeDeletePolicy delete_policy,
    AttributeExportPolicy export_policy)
{
    auto id = get_attribute_id(name);
    auto attr_ptr = m_attributes->template release_ptr<ValueType>(id);
    check_export_policy(*attr_ptr, export_policy);
    delete_attribute(name, delete_policy);
    la_debug_assert(attr_ptr.use_count() == 1);
    return attr_ptr;
}

template <typename Scalar, typename Index>
template <typename ValueType>
std::shared_ptr<const Attribute<ValueType>>
SurfaceMesh<Scalar, Index>::delete_and_export_const_attribute(
    std::string_view name,
    AttributeDeletePolicy delete_policy,
    AttributeExportPolicy export_policy)
{
    auto id = get_attribute_id(name);
    auto attr_ptr = m_attributes->template release_ptr<ValueType>(id);
    check_export_policy(*attr_ptr, export_policy);
    delete_attribute(name, delete_policy);
    la_debug_assert(attr_ptr.use_count() == 1);
    return attr_ptr;
}

template <typename Scalar, typename Index>
template <typename ValueType>
auto SurfaceMesh<Scalar, Index>::delete_and_export_indexed_attribute(
    std::string_view name,
    AttributeExportPolicy policy) -> std::shared_ptr<IndexedAttribute<ValueType, Index>>
{
    auto id = get_attribute_id(name);
    auto attr_ptr = m_attributes->template release_indexed_ptr<ValueType>(id);
    check_export_policy(attr_ptr->values(), policy);
    check_export_policy(attr_ptr->indices(), policy);
    delete_attribute(name);
    la_debug_assert(attr_ptr.use_count() == 1);
    return attr_ptr;
}

template <typename Scalar, typename Index>
template <typename ValueType>
auto SurfaceMesh<Scalar, Index>::delete_and_export_const_indexed_attribute(
    std::string_view name,
    AttributeExportPolicy policy) -> std::shared_ptr<const IndexedAttribute<ValueType, Index>>
{
    auto id = get_attribute_id(name);
    auto attr_ptr = m_attributes->template release_indexed_ptr<ValueType>(id);
    check_export_policy(attr_ptr->values(), policy);
    check_export_policy(attr_ptr->indices(), policy);
    delete_attribute(name);
    la_debug_assert(attr_ptr.use_count() == 1);
    return attr_ptr;
}

template <typename Scalar, typename Index>
bool SurfaceMesh<Scalar, Index>::has_attribute(std::string_view name) const
{
    return m_attributes->contains(name);
}

template <typename Scalar, typename Index>
template <typename ValueType>
bool SurfaceMesh<Scalar, Index>::is_attribute_type(std::string_view name) const
{
    return is_attribute_type<ValueType>(get_attribute_id(name));
}

template <typename Scalar, typename Index>
template <typename ValueType>
bool SurfaceMesh<Scalar, Index>::is_attribute_type(AttributeId id) const
{
    auto& attr = m_attributes->read_base(id);
    return attr.get_value_type() == make_attribute_value_type<ValueType>();
}

template <typename Scalar, typename Index>
bool SurfaceMesh<Scalar, Index>::is_attribute_indexed(std::string_view name) const
{
    return is_attribute_indexed(get_attribute_id(name));
}

template <typename Scalar, typename Index>
bool SurfaceMesh<Scalar, Index>::is_attribute_indexed(AttributeId id) const
{
    auto& attr = m_attributes->read_base(id);
    return attr.get_element_type() == AttributeElement::Indexed;
}

template <typename Scalar, typename Index>
void SurfaceMesh<Scalar, Index>::seq_foreach_attribute_id(
    function_ref<void(AttributeId)> func) const
{
    m_attributes->seq_foreach_attribute_id(func);
}

template <typename Scalar, typename Index>
void SurfaceMesh<Scalar, Index>::seq_foreach_attribute_id(
    function_ref<void(std::string_view, AttributeId)> func) const
{
    m_attributes->seq_foreach_attribute_id(func);
}

template <typename Scalar, typename Index>
void SurfaceMesh<Scalar, Index>::par_foreach_attribute_id(
    function_ref<void(AttributeId)> func) const
{
    m_attributes->par_foreach_attribute_id(func);
}

template <typename Scalar, typename Index>
void SurfaceMesh<Scalar, Index>::par_foreach_attribute_id(
    function_ref<void(std::string_view, AttributeId)> func) const
{
    m_attributes->par_foreach_attribute_id(func);
}

template <typename Scalar, typename Index>
const AttributeBase& SurfaceMesh<Scalar, Index>::get_attribute_base(std::string_view name) const
{
    return get_attribute_base(get_attribute_id(name));
}

template <typename Scalar, typename Index>
const AttributeBase& SurfaceMesh<Scalar, Index>::get_attribute_base(AttributeId id) const
{
    la_debug_assert(id != invalid_attribute_id());
    return m_attributes->read_base(id);
}

template <typename Scalar, typename Index>
template <typename ValueType>
const Attribute<ValueType>& SurfaceMesh<Scalar, Index>::get_attribute(std::string_view name) const
{
    return get_attribute<ValueType>(get_attribute_id(name));
}

template <typename Scalar, typename Index>
template <typename ValueType>
const Attribute<ValueType>& SurfaceMesh<Scalar, Index>::get_attribute(AttributeId id) const
{
    la_debug_assert(id != invalid_attribute_id());
    la_debug_assert(!is_attribute_indexed(id));
    return m_attributes->template read<ValueType>(id);
}

template <typename Scalar, typename Index>
internal::weak_ptr<const AttributeBase> SurfaceMesh<Scalar, Index>::_get_attribute_ptr(
    std::string_view name) const
{
    return _get_attribute_ptr(get_attribute_id(name));
}

template <typename Scalar, typename Index>
internal::weak_ptr<const AttributeBase> SurfaceMesh<Scalar, Index>::_get_attribute_ptr(
    AttributeId id) const
{
    la_debug_assert(id != invalid_attribute_id());
    la_debug_assert(!is_attribute_indexed(id));
    return m_attributes->_get_weak_ptr(id);
}

template <typename Scalar, typename Index>
template <typename ValueType>
auto SurfaceMesh<Scalar, Index>::get_indexed_attribute(std::string_view name) const
    -> const IndexedAttribute<ValueType, Index>&
{
    return get_indexed_attribute<ValueType>(get_attribute_id(name));
}

template <typename Scalar, typename Index>
template <typename ValueType>
auto SurfaceMesh<Scalar, Index>::get_indexed_attribute(AttributeId id) const
    -> const IndexedAttribute<ValueType, Index>&
{
    la_debug_assert(id != invalid_attribute_id());
    la_debug_assert(is_attribute_indexed(id));
    return m_attributes->template read_indexed<ValueType>(id);
}

template <typename Scalar, typename Index>
template <typename ValueType>
Attribute<ValueType>& SurfaceMesh<Scalar, Index>::ref_attribute(std::string_view name)
{
    return ref_attribute<ValueType>(get_attribute_id(name));
}

template <typename Scalar, typename Index>
template <typename ValueType>
Attribute<ValueType>& SurfaceMesh<Scalar, Index>::ref_attribute(AttributeId id)
{
    la_debug_assert(id != invalid_attribute_id());
    la_debug_assert(!is_attribute_indexed(id));
    return m_attributes->template write<ValueType>(id);
}

template <typename Scalar, typename Index>
internal::weak_ptr<AttributeBase> SurfaceMesh<Scalar, Index>::_ref_attribute_ptr(
    std::string_view name)
{
    return _ref_attribute_ptr(get_attribute_id(name));
}

template <typename Scalar, typename Index>
internal::weak_ptr<AttributeBase> SurfaceMesh<Scalar, Index>::_ref_attribute_ptr(AttributeId id)
{
    la_debug_assert(id != invalid_attribute_id());
    return m_attributes->_ref_weak_ptr(id);
}

template <typename Scalar, typename Index>
template <typename ValueType>
auto SurfaceMesh<Scalar, Index>::ref_indexed_attribute(std::string_view name)
    -> IndexedAttribute<ValueType, Index>&
{
    return ref_indexed_attribute<ValueType>(get_attribute_id(name));
}

template <typename Scalar, typename Index>
template <typename ValueType>
auto SurfaceMesh<Scalar, Index>::ref_indexed_attribute(AttributeId id)
    -> IndexedAttribute<ValueType, Index>&
{
    la_debug_assert(id != invalid_attribute_id());
    la_debug_assert(is_attribute_indexed(id));
    return m_attributes->template write_indexed<ValueType>(id);
}

template <typename Scalar, typename Index>
auto SurfaceMesh<Scalar, Index>::get_vertex_to_position() const -> const Attribute<Scalar>&
{
    return get_attribute<Scalar>(m_reserved_ids.vertex_to_position());
}

template <typename Scalar, typename Index>
auto SurfaceMesh<Scalar, Index>::ref_vertex_to_position() -> Attribute<Scalar>&
{
    return ref_attribute<Scalar>(m_reserved_ids.vertex_to_position());
}

template <typename Scalar, typename Index>
auto SurfaceMesh<Scalar, Index>::get_corner_to_vertex() const -> const Attribute<Index>&
{
    return get_attribute<Index>(m_reserved_ids.corner_to_vertex());
}

template <typename Scalar, typename Index>
auto SurfaceMesh<Scalar, Index>::ref_corner_to_vertex() -> Attribute<Index>&
{
    la_runtime_assert(
        !has_edges(),
        "Cannot retrieve a writeable reference to mesh facets when edge/connectivity is "
        "available.");
    return ref_attribute<Index>(m_reserved_ids.corner_to_vertex());
}

template <typename Scalar, typename Index>
bool SurfaceMesh<Scalar, Index>::attr_name_is_reserved(std::string_view name)
{
    return starts_with(name, "$");
}

////////////////////////////////////////////////////////////////////////////////
// Mesh construction
////////////////////////////////////////////////////////////////////////////////

template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index>::SurfaceMesh(BareMeshTag)
    : m_attributes(make_value_ptr<AttributeManager>())
{}

template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index>::SurfaceMesh(Index dimension)
    : m_dimension(dimension)
    , m_attributes(make_value_ptr<AttributeManager>())
{
    la_runtime_assert(m_dimension > 0, "Vertex dimension must be > 0");
    m_reserved_ids.vertex_to_position() = create_attribute_internal<Scalar>(
        s_reserved_names.vertex_to_position(),
        AttributeElement::Vertex,
        AttributeUsage::Position,
        m_dimension);
    m_reserved_ids.corner_to_vertex() = create_attribute_internal<Index>(
        s_reserved_names.corner_to_vertex(),
        AttributeElement::Corner,
        AttributeUsage::VertexIndex,
        1);
}

template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index>::~SurfaceMesh() = default;

template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index>::SurfaceMesh(SurfaceMesh&&) noexcept = default;

template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index>& SurfaceMesh<Scalar, Index>::operator=(SurfaceMesh&&) noexcept = default;

template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index>::SurfaceMesh(const SurfaceMesh&) = default;

template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index>& SurfaceMesh<Scalar, Index>::operator=(const SurfaceMesh&) = default;

namespace {

template <auto Start, auto End, class F>
constexpr void constexpr_for(F&& f)
{
    if constexpr (Start < End) {
        f(std::integral_constant<decltype(Start), Start>());
        constexpr_for<Start + 1, End>(f);
    }
}

} // namespace

template <typename TargetScalar, typename TargetIndex>
template <typename SourceScalar, typename SourceIndex>
SurfaceMesh<TargetScalar, TargetIndex> SurfaceMesh<TargetScalar, TargetIndex>::stripped_copy(
    const SurfaceMesh<SourceScalar, SourceIndex>& source_mesh)
{
    SurfaceMesh<TargetScalar, TargetIndex> target_mesh(BareMeshTag{});
    target_mesh.m_num_vertices = source_mesh.m_num_vertices;
    target_mesh.m_num_facets = source_mesh.m_num_facets;
    target_mesh.m_num_corners = source_mesh.m_num_corners;
    target_mesh.m_num_edges = source_mesh.m_num_edges;
    target_mesh.m_dimension = source_mesh.m_dimension;
    target_mesh.m_vertex_per_facet = source_mesh.m_vertex_per_facet;

    // TODO: Create empty slots where other attributes should have been to ensure id stability?
    constexpr int N = SurfaceMesh<SourceScalar, SourceIndex>::ReservedAttributeIds::size();
    constexpr_for<0, N>([&](auto i) {
        // Only the first reserved attribute has ValueType == Scalar (vertex_to_position). Every
        // other reserved attribute has ValueType == Index.
        using SourceValue = std::conditional_t<i == 0, SourceScalar, SourceIndex>;
        using TargetValue = std::conditional_t<i == 0, TargetScalar, TargetIndex>;
        AttributeId source_id = source_mesh.m_reserved_ids.items[i];
        if (source_id == invalid_attribute_id()) {
            return;
        }
        if (source_mesh.template is_attribute_type<TargetValue>(source_id)) {
            // Use copy-on-write to avoid copying the data
            target_mesh.m_reserved_ids.items[i] = target_mesh.m_attributes->create(
                s_reserved_names.items[i],
                source_mesh.m_attributes->copy_ptr(source_id));
        } else {
            // Cast the underlying buffer to the correct type
            target_mesh.m_reserved_ids.items[i] =
                target_mesh.m_attributes->template cast_from<TargetValue>(
                    s_reserved_names.items[i],
                    source_mesh.template get_attribute<SourceValue>(source_id));
        }
    });

    return target_mesh;
}

template <typename TargetScalar, typename TargetIndex>
template <typename SourceScalar, typename SourceIndex>
SurfaceMesh<TargetScalar, TargetIndex> SurfaceMesh<TargetScalar, TargetIndex>::stripped_move(
    SurfaceMesh<SourceScalar, SourceIndex>&& source_mesh)
{
    SurfaceMesh<TargetScalar, TargetIndex> target_mesh(BareMeshTag{});
    target_mesh.m_num_vertices = source_mesh.m_num_vertices;
    target_mesh.m_num_facets = source_mesh.m_num_facets;
    target_mesh.m_num_corners = source_mesh.m_num_corners;
    target_mesh.m_num_edges = source_mesh.m_num_edges;
    target_mesh.m_dimension = source_mesh.m_dimension;
    target_mesh.m_vertex_per_facet = source_mesh.m_vertex_per_facet;

    // TODO: Create empty slots where other attributes should have been to ensure id stability?
    constexpr int N = SurfaceMesh<SourceScalar, SourceIndex>::ReservedAttributeIds::size();
    constexpr_for<0, N>([&](auto i) {
        // Only the first reserved attribute has ValueType == Scalar (vertex_to_position). Every
        // other reserved attribute has ValueType == Index.
        using SourceValue = std::conditional_t<i == 0, SourceScalar, SourceIndex>;
        using TargetValue = std::conditional_t<i == 0, TargetScalar, TargetIndex>;
        AttributeId source_id = source_mesh.m_reserved_ids.items[i];
        if (source_id == invalid_attribute_id()) {
            return;
        }
        if (source_mesh.template is_attribute_type<TargetValue>(source_id)) {
            // Use copy-on-write to avoid copying the data
            target_mesh.m_reserved_ids.items[i] = target_mesh.m_attributes->create(
                s_reserved_names.items[i],
                source_mesh.m_attributes->move_ptr(source_id));
        } else {
            // Cast the underlying buffer to the correct type
            target_mesh.m_reserved_ids.items[i] =
                target_mesh.m_attributes->template cast_from<TargetValue>(
                    s_reserved_names.items[i],
                    source_mesh.template get_attribute<SourceValue>(source_id));
        }
    });

    return target_mesh;
}

template <typename Scalar, typename Index>
void SurfaceMesh<Scalar, Index>::add_vertex(span<const Scalar> p)
{
    const Index old_num_vertices = get_num_vertices();
    la_runtime_assert(p.size() == get_dimension());
    resize_vertices_internal(old_num_vertices + 1);
    std::copy(p.begin(), p.end(), ref_vertex_to_position().ref_last(1).begin());
    la_debug_assert(get_num_vertices() == old_num_vertices + 1);
}

template <typename Scalar, typename Index>
void SurfaceMesh<Scalar, Index>::add_vertex(std::initializer_list<const Scalar> p)
{
    const Index old_num_vertices = get_num_vertices();
    la_runtime_assert(p.size() == get_dimension());
    resize_vertices_internal(old_num_vertices + 1);
    std::copy(p.begin(), p.end(), ref_vertex_to_position().ref_last(1).begin());
    la_debug_assert(get_num_vertices() == old_num_vertices + 1);
}

template <typename Scalar, typename Index>
void SurfaceMesh<Scalar, Index>::add_vertices(Index num_vertices, span<const Scalar> coordinates)
{
    const Index old_num_vertices = get_num_vertices();
    resize_vertices_internal(old_num_vertices + num_vertices);
    if (!coordinates.empty()) {
        la_runtime_assert(Index(coordinates.size()) == get_dimension() * num_vertices);
        auto new_vertices = ref_vertex_to_position().ref_last(num_vertices);
        std::copy(coordinates.begin(), coordinates.end(), new_vertices.begin());
    }
    la_debug_assert(get_num_vertices() == old_num_vertices + num_vertices);
}

template <typename Scalar, typename Index>
void SurfaceMesh<Scalar, Index>::add_vertices(
    Index num_vertices,
    std::initializer_list<const Scalar> coordinates)
{
    add_vertices(num_vertices, span<const Scalar>(coordinates.begin(), coordinates.end()));
}

template <typename Scalar, typename Index>
void SurfaceMesh<Scalar, Index>::add_vertices(
    Index num_vertices,
    SetVertexCoordinatesFunction set_vertex_coordinates)
{
    la_runtime_assert(set_vertex_coordinates);
    resize_vertices_internal(get_num_vertices() + num_vertices);
    auto new_vertices = ref_vertex_to_position().ref_last(num_vertices);
    const size_t dim = get_dimension();
    for (Index i = 0; i < num_vertices; ++i) {
        set_vertex_coordinates(i, new_vertices.subspan(i * dim, dim));
    }
}

template <typename Scalar, typename Index>
void SurfaceMesh<Scalar, Index>::add_triangle(Index v0, Index v1, Index v2)
{
    auto new_corners = reserve_indices_internal(1, 3);
    const Index v[] = {v0, v1, v2};
    std::copy(v, v + 3, new_corners.begin());
    update_edges_last_internal(1);
}

template <typename Scalar, typename Index>
void SurfaceMesh<Scalar, Index>::add_quad(Index v0, Index v1, Index v2, Index v3)
{
    auto new_corners = reserve_indices_internal(1, 4);
    const Index v[] = {v0, v1, v2, v3};
    std::copy(v, v + 4, new_corners.begin());
    update_edges_last_internal(1);
}

template <typename Scalar, typename Index>
void SurfaceMesh<Scalar, Index>::add_polygon(Index facet_size)
{
    la_runtime_assert(facet_size > 0);
    reserve_indices_internal(1, facet_size);
    update_edges_last_internal(1);
}

template <typename Scalar, typename Index>
void SurfaceMesh<Scalar, Index>::add_polygon(span<const Index> facet_indices)
{
    la_runtime_assert(facet_indices.size() > 0);
    auto new_corners = reserve_indices_internal(1, Index(facet_indices.size()));
    std::copy(facet_indices.begin(), facet_indices.end(), new_corners.begin());
    update_edges_last_internal(1);
}

template <typename Scalar, typename Index>
void SurfaceMesh<Scalar, Index>::add_polygon(std::initializer_list<const Index> facet_indices)
{
    la_runtime_assert(facet_indices.size() > 0);
    auto new_corners = reserve_indices_internal(1, Index(facet_indices.size()));
    std::copy(facet_indices.begin(), facet_indices.end(), new_corners.begin());
    update_edges_last_internal(1);
}

template <typename Scalar, typename Index>
void SurfaceMesh<Scalar, Index>::add_polygon(
    Index facet_size,
    SetSingleFacetIndicesFunction set_facet_indices)
{
    la_runtime_assert(facet_size > 0);
    la_debug_assert(set_facet_indices);
    auto new_corners = reserve_indices_internal(1, facet_size);
    set_facet_indices(new_corners);
    update_edges_last_internal(1);
}

template <typename Scalar, typename Index>
void SurfaceMesh<Scalar, Index>::add_triangles(Index num_facets, span<const Index> facet_indices)
{
    add_polygons(num_facets, 3, facet_indices);
}

template <typename Scalar, typename Index>
void SurfaceMesh<Scalar, Index>::add_triangles(
    Index num_facets,
    std::initializer_list<const Index> facet_indices)
{
    add_polygons(num_facets, 3, span<const Index>(facet_indices.begin(), facet_indices.end()));
}

template <typename Scalar, typename Index>
void SurfaceMesh<Scalar, Index>::add_triangles(
    Index num_facets,
    SetMultiFacetsIndicesFunction set_facets_indices)
{
    add_polygons(num_facets, 3, set_facets_indices);
}

template <typename Scalar, typename Index>
void SurfaceMesh<Scalar, Index>::add_quads(Index num_facets, span<const Index> facet_indices)
{
    add_polygons(num_facets, 4, facet_indices);
}

template <typename Scalar, typename Index>
void SurfaceMesh<Scalar, Index>::add_quads(
    Index num_facets,
    std::initializer_list<const Index> facet_indices)
{
    add_polygons(num_facets, 4, span<const Index>(facet_indices.begin(), facet_indices.end()));
}

template <typename Scalar, typename Index>
void SurfaceMesh<Scalar, Index>::add_quads(
    Index num_facets,
    SetMultiFacetsIndicesFunction set_facets_indices)
{
    add_polygons(num_facets, 4, set_facets_indices);
}

template <typename Scalar, typename Index>
void SurfaceMesh<Scalar, Index>::add_polygons(
    Index num_facets,
    Index facet_size,
    span<const Index> facet_indices)
{
    // TODO: If num_facets == 0, this should not trigger any write operation to a mesh attr (need to
    // add a unit test for this!).
    la_runtime_assert(facet_size > 0);
    la_runtime_assert(
        !facet_indices.empty() || !has_edges(),
        "Cannot add facets without indices if mesh has edge/connectivity information");
    auto new_corners = reserve_indices_internal(num_facets, facet_size);
    if (!facet_indices.empty()) {
        la_runtime_assert(facet_indices.size() == new_corners.size());
        std::copy(facet_indices.begin(), facet_indices.end(), new_corners.begin());
    }
    update_edges_last_internal(num_facets);
}

template <typename Scalar, typename Index>
void SurfaceMesh<Scalar, Index>::add_polygons(
    Index num_facets,
    Index facet_size,
    std::initializer_list<const Index> facet_indices)
{
    add_polygons(
        num_facets,
        facet_size,
        span<const Index>(facet_indices.begin(), facet_indices.end()));
}

template <typename Scalar, typename Index>
void SurfaceMesh<Scalar, Index>::add_polygons(
    Index num_facets,
    Index facet_size,
    SetMultiFacetsIndicesFunction set_facets_indices)
{
    la_runtime_assert(facet_size > 0);
    la_runtime_assert(set_facets_indices);
    auto new_corners = reserve_indices_internal(num_facets, facet_size);
    for (Index i = 0; i < num_facets; ++i) {
        set_facets_indices(i, new_corners.subspan(i * facet_size, facet_size));
    }
    update_edges_last_internal(num_facets);
}

template <typename Scalar, typename Index>
void SurfaceMesh<Scalar, Index>::add_hybrid(
    span<const Index> facet_sizes,
    span<const Index> facet_indices)
{
    la_runtime_assert(!facet_sizes.empty());
    la_runtime_assert(
        !facet_indices.empty() || !has_edges(),
        "Cannot add facets without indices if mesh has edge/connectivity information");
    auto new_corners = reserve_indices_internal(facet_sizes);
    if (!facet_indices.empty()) {
        la_runtime_assert(new_corners.size() == facet_indices.size());
        std::copy(facet_indices.begin(), facet_indices.end(), new_corners.begin());
    }
    update_edges_last_internal(Index(facet_sizes.size()));
}

template <typename Scalar, typename Index>
void SurfaceMesh<Scalar, Index>::add_hybrid(
    std::initializer_list<const Index> facet_sizes,
    std::initializer_list<const Index> facet_indices)
{
    add_hybrid(
        span<const Index>(facet_sizes.begin(), facet_sizes.end()),
        span<const Index>(facet_indices.begin(), facet_indices.end()));
}

template <typename Scalar, typename Index>
void SurfaceMesh<Scalar, Index>::add_hybrid(
    Index num_facets,
    GetFacetsSizeFunction facet_sizes,
    SetMultiFacetsIndicesFunction set_facets_indices)
{
    la_runtime_assert(facet_sizes);
    la_runtime_assert(set_facets_indices);
    const auto old_num_facets = get_num_facets();
    auto new_corners = reserve_indices_internal(num_facets, facet_sizes);
    for (Index i = 0, last_offset = 0; i < num_facets; ++i) {
        // We don't want to evaluate facet_sizes twice, so we retrieve its value from the
        // computed offsets.
        const Index facet_size = get_facet_size(old_num_facets + i);
        set_facets_indices(i, new_corners.subspan(last_offset, facet_size));
        last_offset += facet_size;
    }
    update_edges_last_internal(num_facets);
}

template <typename Scalar, typename Index>
void SurfaceMesh<Scalar, Index>::remove_vertices(span<const Index> vertices_to_remove)
{
    if (vertices_to_remove.empty()) {
        return; // nothing to remove
    }

    // 1st step: create index remapping
    const Index num_vertices_old = get_num_vertices();
    Index num_vertices_new = 0;
    std::vector<Index> old_to_new(num_vertices_old, invalid<Index>());
    Index v_first = 0;
    for (Index v_last : vertices_to_remove) {
        la_runtime_assert(v_first <= v_last, "Indices to remove should be sorted");
        la_runtime_assert(ensure_positive(v_last) && v_last < num_vertices_old);
        for (Index v = v_first; v < v_last; ++v) {
            old_to_new[v] = num_vertices_new;
            ++num_vertices_new;
        }
        v_first = v_last + 1;
    }
    for (Index v = v_first; v < num_vertices_old; ++v) {
        old_to_new[v] = num_vertices_new;
        ++num_vertices_new;
    }

    // 2nd step: perform vertex reindexing and delete extra vertices
    reindex_vertices_internal(old_to_new);
    resize_vertices_internal(num_vertices_new);

    // 3rd step: delete facets pointing to invalid vertices
    remove_facets([&](Index f) {
        for (auto v : get_facet_vertices(f)) {
            if (v == invalid<Index>()) {
                logger().trace("Removing f{}", f);
                return true;
            }
        }
        return false;
    });
}

template <typename Scalar, typename Index>
void SurfaceMesh<Scalar, Index>::remove_vertices(
    std::initializer_list<const Index> vertices_to_remove)
{
    remove_vertices(span<const Index>(vertices_to_remove));
}

template <typename Scalar, typename Index>
void SurfaceMesh<Scalar, Index>::remove_vertices(function_ref<bool(Index)> should_remove_func)
{
    // 1st step: create index remapping
    const Index num_vertices_old = get_num_vertices();
    Index num_vertices_new = 0;
    bool is_identity = true;
    std::vector<Index> old_to_new(num_vertices_old);
    for (Index v = 0; v < num_vertices_old; ++v) {
        if (!should_remove_func(v)) {
            old_to_new[v] = num_vertices_new;
            if (v != num_vertices_new) {
                is_identity = false;
            }
            ++num_vertices_new;
        } else {
            old_to_new[v] = invalid<Index>();
            is_identity = false; // TODO: Test this?
        }
    }
    if (is_identity) {
        return; // nothing to remove
    }

    // 2nd step: perform vertex reindexing and delete extra vertices
    reindex_vertices_internal(old_to_new);
    resize_vertices_internal(num_vertices_new);

    // 3rd step: delete facets pointing to invalid vertices
    remove_facets([&](Index f) {
        for (auto v : get_facet_vertices(f)) {
            if (v == invalid<Index>()) {
                logger().trace("Removing f{}", f);
                return true;
            }
        }
        return false;
    });
}

template <typename Scalar, typename Index>
void SurfaceMesh<Scalar, Index>::remove_facets(span<const Index> facets_to_remove)
{
    if (facets_to_remove.empty()) {
        return; // nothing to remove
    }

    // 1st step: create index remapping
    const Index num_facets_old = get_num_facets();
    Index num_facets_new = 0;
    std::vector<Index> old_to_new(num_facets_old, invalid<Index>());
    Index f_first = 0;
    for (Index f_last : facets_to_remove) {
        la_runtime_assert(f_first <= f_last, "Indices to remove should be sorted");
        la_runtime_assert(ensure_positive(f_last) && f_last < num_facets_old);
        for (Index f = f_first; f < f_last; ++f) {
            old_to_new[f] = num_facets_new;
            ++num_facets_new;
        }
        f_first = f_last + 1;
    }
    for (Index f = f_first; f < num_facets_old; ++f) {
        old_to_new[f] = num_facets_new;
        ++num_facets_new;
    }

    // 2nd step: perform vertex reindexing and delete extra vertices
    auto [num_corners_new, num_edges_new] = reindex_facets_internal(old_to_new);
    logger().debug("New #f {}, #c {}, #e {}", num_facets_new, num_corners_new, num_edges_new);

    resize_facets_internal(num_facets_new);
    resize_corners_internal(num_corners_new);
    resize_edges_internal(num_edges_new);
}

template <typename Scalar, typename Index>
void SurfaceMesh<Scalar, Index>::remove_facets(std::initializer_list<const Index> facets_to_remove)
{
    remove_facets(span<const Index>(facets_to_remove));
}

template <typename Scalar, typename Index>
void SurfaceMesh<Scalar, Index>::remove_facets(function_ref<bool(Index)> should_remove_func)
{
    // 1st step: create index remapping
    const Index num_facets_old = get_num_facets();
    Index num_facets_new = 0;
    bool is_identity = true;
    std::vector<Index> old_to_new(num_facets_old);
    for (Index f = 0; f < num_facets_old; ++f) {
        if (!should_remove_func(f)) {
            old_to_new[f] = num_facets_new;
            if (f != num_facets_new) {
                is_identity = false;
            }
            ++num_facets_new;
        } else {
            old_to_new[f] = invalid<Index>();
            is_identity = false;
        }
    }
    if (is_identity) {
        return; // nothing to remove
    }

    // 2nd step: perform vertex reindexing and delete extra vertices
    auto [num_corners_new, num_edges_new] = reindex_facets_internal(old_to_new);
    logger().debug("New #f {}, #c {}, #e {}", num_facets_new, num_corners_new, num_edges_new);

    resize_facets_internal(num_facets_new);
    resize_corners_internal(num_corners_new);
    resize_edges_internal(num_edges_new);
}

namespace {

template <typename Index, typename Attr>
void clear_element_index(Attr&& attr, AttributeUsage usage)
{
    using AttributeType = std::decay_t<decltype(attr)>;
    using ValueType = typename AttributeType::ValueType;
    if constexpr (std::is_same_v<ValueType, Index>) {
        if (attr.get_usage() == usage) {
            if constexpr (AttributeType::IsIndexed) {
                clear_element_index<Index>(attr.values(), usage);
            } else {
                LA_IGNORE(usage);
                auto t = attr.ref_all();
                std::fill(t.begin(), t.end(), attr.get_default_value());
            }
        }
    } else {
        LA_IGNORE(usage);
    }
}

} // namespace

template <typename Scalar, typename Index>
void SurfaceMesh<Scalar, Index>::clear_vertices()
{
    resize_vertices_internal(0);
    clear_facets();
    seq_foreach_attribute_write(*this, [&](auto&& attr) {
        clear_element_index<Index>(attr, AttributeUsage::VertexIndex);
    });
}

template <typename Scalar, typename Index>
void SurfaceMesh<Scalar, Index>::clear_facets()
{
    resize_facets_internal(0);
    resize_corners_internal(0);
    resize_edges_internal(0);
    seq_foreach_attribute_write(*this, [&](auto&& attr) {
        clear_element_index<Index>(attr, AttributeUsage::FacetIndex);
        clear_element_index<Index>(attr, AttributeUsage::CornerIndex);
        clear_element_index<Index>(attr, AttributeUsage::EdgeIndex);
    });
}

template <typename Scalar, typename Index>
void SurfaceMesh<Scalar, Index>::shrink_to_fit()
{
    seq_foreach_attribute_write(*this, [](auto&& attr) {
        using AttributeType = std::decay_t<decltype(attr)>;
        if constexpr (AttributeType::IsIndexed) {
            attr.values().shrink_to_fit();
            attr.indices().shrink_to_fit();
        } else {
            attr.shrink_to_fit();
        }
    });
}

template <typename Scalar, typename Index>
void SurfaceMesh<Scalar, Index>::compress_if_regular()
{
    if (is_regular()) {
        // Nothing to do
        return;
    }
    // Check whether all facets have the same size
    Index nvpf = 0;
    bool same_size = true;
    for (Index f = 0; f < get_num_facets(); ++f) {
        const Index nv = get_facet_size(f);
        if (nvpf == 0) {
            nvpf = nv;
        }
        if (nvpf != nv) {
            same_size = false;
        }
    }
    // If so, delete hybrid storage attributes
    if (same_size) {
        la_debug_assert(m_reserved_ids.corner_to_facet() != invalid_attribute_id());
        delete_attribute(s_reserved_names.facet_to_first_corner(), AttributeDeletePolicy::Force);
        delete_attribute(s_reserved_names.corner_to_facet(), AttributeDeletePolicy::Force);
        m_vertex_per_facet = nvpf;
    }
    la_debug_assert(is_regular());
}

////////////////////////////////////////////////////////////////////////////////
/// Accessors
////////////////////////////////////////////////////////////////////////////////

template <typename Scalar, typename Index>
bool SurfaceMesh<Scalar, Index>::is_triangle_mesh() const
{
    return is_regular() && (get_corner_to_vertex().empty() || m_vertex_per_facet == 3);
}

template <typename Scalar, typename Index>
bool SurfaceMesh<Scalar, Index>::is_quad_mesh() const
{
    return is_regular() && (get_corner_to_vertex().empty() || m_vertex_per_facet == 4);
}

template <typename Scalar, typename Index>
bool SurfaceMesh<Scalar, Index>::is_regular() const
{
    // Since we can add and remove facets from a mesh, it is possible for an empty mesh to have
    // "hybrid" storage. Thus we define a mesh as "regular" iff it doesn't have an existing
    // mapping facet -> first corner. A hybrid mesh can be turned back to regular by calling the
    // compress_if_regular() method.
    bool reg = (m_reserved_ids.facet_to_first_corner() == invalid_attribute_id());
    if (reg) {
        la_debug_assert(get_corner_to_vertex().empty() || m_vertex_per_facet > 0);
    }
    return reg;
}

template <typename Scalar, typename Index>
auto SurfaceMesh<Scalar, Index>::get_vertex_per_facet() const -> Index
{
    if (is_hybrid()) {
        throw Error("[get_vertex_per_facet] This function is only for regular meshes");
    }
    return m_vertex_per_facet;
}

template <typename Scalar, typename Index>
auto SurfaceMesh<Scalar, Index>::get_position(Index v) const -> span<const Scalar>
{
    return get_vertex_to_position().get_row(v);
}

template <typename Scalar, typename Index>
auto SurfaceMesh<Scalar, Index>::ref_position(Index v) -> span<Scalar>
{
    return ref_vertex_to_position().ref_row(v);
}

template <typename Scalar, typename Index>
auto SurfaceMesh<Scalar, Index>::get_facet_corner_begin(Index f) const -> Index
{
    static_assert(std::is_unsigned_v<Index>);
    la_debug_assert(f < get_num_facets());
    if (is_regular()) {
        return m_vertex_per_facet * f;
    } else {
        return get_attribute<Index>(m_reserved_ids.facet_to_first_corner()).get(f);
    }
}

template <typename Scalar, typename Index>
auto SurfaceMesh<Scalar, Index>::get_facet_corner_end(Index f) const -> Index
{
    static_assert(std::is_unsigned_v<Index>);
    la_debug_assert(f < get_num_facets());
    if (is_regular()) {
        return m_vertex_per_facet * (f + 1);
    } else {
        return f + 1 == get_num_facets()
                   ? Index(get_corner_to_vertex().get_num_elements())
                   : get_attribute<Index>(m_reserved_ids.facet_to_first_corner()).get(f + 1);
    }
}

template <typename Scalar, typename Index>
auto SurfaceMesh<Scalar, Index>::get_corner_vertex(Index c) const -> Index
{
    return get_corner_to_vertex().get(c);
}

template <typename Scalar, typename Index>
auto SurfaceMesh<Scalar, Index>::get_corner_facet(Index c) const -> Index
{
    la_debug_assert(c < get_num_corners());
    if (is_regular()) {
        return c / m_vertex_per_facet;
    } else {
        return get_attribute<Index>(m_reserved_ids.corner_to_facet()).get(c);
    }
}

template <typename Scalar, typename Index>
auto SurfaceMesh<Scalar, Index>::get_facet_vertices(Index f) const -> span<const Index>
{
    return get_corner_to_vertex().get_middle(get_facet_corner_begin(f), size_t(get_facet_size(f)));
}

template <typename Scalar, typename Index>
auto SurfaceMesh<Scalar, Index>::ref_facet_vertices(Index f) -> span<Index>
{
    return ref_corner_to_vertex().ref_middle(get_facet_corner_begin(f), size_t(get_facet_size(f)));
}

////////////////////////////////////////////////////////////////////////////////
/// Mesh edges and connectivity
////////////////////////////////////////////////////////////////////////////////

template <typename Scalar, typename Index>
void SurfaceMesh<Scalar, Index>::initialize_edges(span<const Index> input_edges)
{
    if (input_edges.empty()) {
        initialize_edges_internal(0, nullptr);
    } else {
        la_runtime_assert(
            input_edges.size() % 2 == 0,
            "Input edge array size must be a multiple of two.");
        const Index num_user_edges = static_cast<Index>(input_edges.size()) / 2;
        // We need to be careful to not wrap a temporary lambda into a function_ref<>. Since the
        // function wrapper is non-owning, this would lead to a "stack use after scope" UBSan
        // issues. See the following for an explanation:
        // https://github.com/TartanLlama/function_ref/issues/12
        // https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2019/p0792r4.html#possible-issues
        [&](GetEdgeVertices get_user_edge) {
            initialize_edges_internal(num_user_edges, &get_user_edge);
        }([&](Index e) -> std::array<Index, 2> {
            return {input_edges[2 * e], input_edges[2 * e + 1]};
        });
    }
}

template <typename Scalar, typename Index>
void SurfaceMesh<Scalar, Index>::initialize_edges(
    Index num_user_edges,
    GetEdgeVertices get_user_edge)
{
    initialize_edges_internal(num_user_edges, &get_user_edge);
}

template <typename Scalar, typename Index>
void SurfaceMesh<Scalar, Index>::initialize_edges_internal(
    Index num_user_edges,
    GetEdgeVertices* get_user_edge_ptr)
{
    if (has_edges()) {
        if (get_user_edge_ptr) {
            logger().warn(
                "User-provided edge ordering ignored: mesh already contains edge information");
        }
        return;
    }
    la_runtime_assert(m_reserved_ids.corner_to_edge() == invalid_attribute_id());
    la_runtime_assert(m_reserved_ids.edge_to_first_corner() == invalid_attribute_id());
    la_runtime_assert(m_reserved_ids.vertex_to_first_corner() == invalid_attribute_id());
    la_runtime_assert(m_reserved_ids.next_corner_around_edge() == invalid_attribute_id());
    la_runtime_assert(m_reserved_ids.next_corner_around_vertex() == invalid_attribute_id());
    m_reserved_ids.corner_to_edge() = create_attribute_internal<Index>(
        s_reserved_names.corner_to_edge(),
        AttributeElement::Corner,
        AttributeUsage::EdgeIndex,
        1);
    m_reserved_ids.edge_to_first_corner() = create_attribute_internal<Index>(
        s_reserved_names.edge_to_first_corner(),
        AttributeElement::Edge,
        AttributeUsage::CornerIndex,
        1);
    m_reserved_ids.vertex_to_first_corner() = create_attribute_internal<Index>(
        s_reserved_names.vertex_to_first_corner(),
        AttributeElement::Vertex,
        AttributeUsage::CornerIndex,
        1);
    m_reserved_ids.next_corner_around_edge() = create_attribute_internal<Index>(
        s_reserved_names.next_corner_around_edge(),
        AttributeElement::Corner,
        AttributeUsage::CornerIndex,
        1);
    m_reserved_ids.next_corner_around_vertex() = create_attribute_internal<Index>(
        s_reserved_names.next_corner_around_vertex(),
        AttributeElement::Corner,
        AttributeUsage::CornerIndex,
        1);
    update_edges_last_internal(get_num_facets(), num_user_edges, get_user_edge_ptr);
}

template <typename Scalar, typename Index>
void SurfaceMesh<Scalar, Index>::update_edges_last_internal(
    Index count,
    Index num_user_edges,
    GetEdgeVertices* get_user_edge_ptr)
{
    const Index facet_end = get_num_facets();
    const Index facet_begin = facet_end - count;
    update_edges_range_internal(facet_begin, facet_end, num_user_edges, get_user_edge_ptr);
}

template <typename Scalar, typename Index>
void SurfaceMesh<Scalar, Index>::update_edges_range_internal(
    Index facet_begin,
    Index facet_end,
    Index num_user_edges,
    GetEdgeVertices* get_user_edge_ptr)
{
    // Assumptions: the range of facets given as input doesn't have any connectivity information
    // (i.e. facet corners do not appear in any chain around vertices/edges).

    if (!has_edges()) {
        return;
    }
    if (facet_begin == facet_end) {
        return;
    }
    const Index corner_begin = get_facet_corner_begin(facet_begin);
    const Index corner_end = get_facet_corner_end(facet_end - 1);

    // Compute corner -> edge mapping
    struct UnorientedEdge
    {
        Index v1;
        Index v2;
        Index id;

        UnorientedEdge(Index x, Index y, Index c)
            : v1(std::min(x, y))
            , v2(std::max(x, y))
            , id(c)
        {}

        auto key() const { return std::make_pair(v1, v2); }

        bool operator<(const UnorientedEdge& e) const { return key() < e.key(); }

        bool operator!=(const UnorientedEdge& e) const { return key() != e.key(); }
    };

    auto corner_to_vertex = get_corner_to_vertex().get_all();
    auto corner_to_edge = ref_attribute<Index>(m_reserved_ids.corner_to_edge()).ref_all();
    auto vertex_to_first_corner =
        ref_attribute<Index>(m_reserved_ids.vertex_to_first_corner()).ref_all();
    auto next_corner_around_vertex =
        ref_attribute<Index>(m_reserved_ids.next_corner_around_vertex()).ref_all();
    auto next_corner_around_edge =
        ref_attribute<Index>(m_reserved_ids.next_corner_around_edge()).ref_all();

    // Sort new unoriented edges + assign corner -> edge mapping to previously existing edges
    std::vector<UnorientedEdge> edge_to_corner;
    edge_to_corner.reserve(corner_end - corner_begin);
    const bool whole_mesh = (facet_end - facet_begin == get_num_facets());
    for (Index f = facet_begin; f < facet_end; ++f) {
        const Index c0 = get_facet_corner_begin(f);
        const Index nv = get_facet_size(f);
        for (Index lv = 0; lv < nv; ++lv) {
            auto v1 = corner_to_vertex[c0 + lv];
            auto v2 = corner_to_vertex[c0 + ((lv + 1) % nv)];
            UnorientedEdge edge(v1, v2, c0 + lv);
            Index assigned_e = invalid<Index>();
            if (!whole_mesh) {
                // Check corners around v1 and v2 for existing edges with endpoints {v1, v2}
                // This look is disabled when we are assigning edge ids for the whole mesh, since
                // this adds a significant overhead. We should look into more efficient ways to
                // incrementally add vertices/facets on a mesh with edge id information.
                for (Index v : {v1, v2}) {
                    if (assigned_e == invalid<Index>()) {
                        foreach_edge_around_vertex_with_duplicates(v, [&](Index e) {
                            if (assigned_e != invalid<Index>()) {
                                return;
                            }
                            if (e != invalid<Index>()) {
                                auto w = get_edge_vertices(e);
                                UnorientedEdge other(w[0], w[1], e);
                                if (edge.key() == other.key()) {
                                    assigned_e = e;
                                }
                            }
                        });
                    }
                }
            }
            if (assigned_e != invalid<Index>()) {
                // Corner already has a matching edge, assign it
                corner_to_edge[c0 + lv] = assigned_e;
            } else {
                // No edge incident to v1, v2 has the both (v1, v2) as endpoints, assign a new edge
                // id
                edge_to_corner.emplace_back(edge);
            }
        }
    }
    tbb::parallel_sort(edge_to_corner.begin(), edge_to_corner.end());

    // Sort user-defined edges (if available)
    std::vector<UnorientedEdge> edge_to_id_user;
    if (get_user_edge_ptr != nullptr) {
        edge_to_id_user.reserve(num_user_edges);
        for (Index e = 0; e < num_user_edges; ++e) {
            auto v = (*get_user_edge_ptr)(e);
            edge_to_id_user.emplace_back(v[0], v[1], e);
        }
        tbb::parallel_sort(edge_to_id_user.begin(), edge_to_id_user.end());
    }

    // Assign unique edge ids
    const bool has_custom_edges = !edge_to_id_user.empty();
    const Index old_num_edges = get_num_edges();
    Index new_num_edges = old_num_edges;
    for (auto it_begin = edge_to_corner.begin(); it_begin != edge_to_corner.end();) {
        // Determine id of the current edge
        Index edge_id = new_num_edges;
        if (has_custom_edges) {
            la_runtime_assert(
                new_num_edges < old_num_edges + Index(edge_to_id_user.size()),
                "Incorrect number of edges in user-provided indexing!");
            la_runtime_assert(
                edge_to_id_user[new_num_edges - old_num_edges].key() == it_begin->key(),
                "Mismatched edge vertices!");
            edge_id = edge_to_id_user[new_num_edges - old_num_edges].id;
        }

        // First the first edge after it_begin that has a different key
        auto it_end =
            std::find_if(it_begin, edge_to_corner.end(), [&](auto e) { return (e != *it_begin); });
        for (auto it = it_begin; it != it_end; ++it) {
            corner_to_edge[it->id] = edge_id;
        }
        ++new_num_edges;
        it_begin = it_end;
    }

    resize_edges_internal(new_num_edges);
    auto edge_to_first_corner =
        ref_attribute<Index>(m_reserved_ids.edge_to_first_corner()).ref_all();

    // Chain corners around edges
    for (Index f = facet_begin; f < facet_end; ++f) {
        for (Index c = get_facet_corner_begin(f); c < get_facet_corner_end(f); ++c) {
            Index e = corner_to_edge[c];
            next_corner_around_edge[c] = edge_to_first_corner[e];
            edge_to_first_corner[e] = c;
        }
    }

    // Chain corners around vertices
    for (Index f = facet_begin; f < facet_end; ++f) {
        for (Index c = get_facet_corner_begin(f); c < get_facet_corner_end(f); ++c) {
            Index v = corner_to_vertex[c];
            next_corner_around_vertex[c] = vertex_to_first_corner[v];
            vertex_to_first_corner[v] = c;
        }
    }
}

template <typename Scalar, typename Index>
void SurfaceMesh<Scalar, Index>::clear_edges()
{
    delete_attribute(s_reserved_names.corner_to_edge(), AttributeDeletePolicy::Force);
    delete_attribute(s_reserved_names.edge_to_first_corner(), AttributeDeletePolicy::Force);
    delete_attribute(s_reserved_names.next_corner_around_edge(), AttributeDeletePolicy::Force);
    delete_attribute(s_reserved_names.vertex_to_first_corner(), AttributeDeletePolicy::Force);
    delete_attribute(s_reserved_names.next_corner_around_vertex(), AttributeDeletePolicy::Force);
    resize_edges_internal(0);
}

template <typename Scalar, typename Index>
auto SurfaceMesh<Scalar, Index>::get_edge(Index f, Index lv) const -> Index
{
    la_debug_assert(m_reserved_ids.corner_to_edge() != invalid_attribute_id());
    const auto& c2e = get_attribute<Index>(m_reserved_ids.corner_to_edge());
    return c2e.get(get_facet_corner_begin(f) + lv);
}

template <typename Scalar, typename Index>
auto SurfaceMesh<Scalar, Index>::get_corner_edge(Index c) const -> Index
{
    la_debug_assert(m_reserved_ids.corner_to_edge() != invalid_attribute_id());
    const auto& c2e = get_attribute<Index>(m_reserved_ids.corner_to_edge());
    return c2e.get(c);
}

template <typename Scalar, typename Index>
auto SurfaceMesh<Scalar, Index>::get_edge_from_corner(Index c) const -> Index
{
    return get_corner_edge(c);
}

template <typename Scalar, typename Index>
auto SurfaceMesh<Scalar, Index>::get_edge_vertices(Index e) const -> std::array<Index, 2>
{
    la_debug_assert(m_reserved_ids.edge_to_first_corner() != invalid_attribute_id());
    const Index c = get_attribute<Index>(m_reserved_ids.edge_to_first_corner()).get(e);
    if (c == invalid<Index>()) {
        throw Error(fmt::format("Invalid corner id for edge: {}", e));
    }
    Index f = get_corner_facet(c);
    Index lv = c - get_facet_corner_begin(f);
    Index nv = get_facet_size(f);
    return {{get_facet_vertex(f, lv), get_facet_vertex(f, (lv + 1) % nv)}};
}

template <typename Scalar, typename Index>
Index SurfaceMesh<Scalar, Index>::find_edge_from_vertices(Index v0, Index v1) const
{
    Index ei = invalid<Index>();

    // Look for edge (vi, vj) in facets.
    auto search_edge = [&](Index vi, Index vj) {
        foreach_corner_around_vertex(vi, [&](Index ci) {
            la_debug_assert(get_corner_vertex(ci) == vi);
            Index f = get_corner_facet(ci);
            Index cb = get_facet_corner_begin(f);
            Index lv = ci - cb;
            Index nv = get_facet_size(f);
            Index cj = cb + (lv + 1) % nv;
            Index ck = cb + (lv + nv - 1) % nv;
            if (get_corner_vertex(cj) == vj) {
                ei = get_edge(f, lv);
            } else if (get_corner_vertex(ck) == vj) {
                ei = get_edge(f, (lv + nv - 1) % nv);
            }
        });
    };

    search_edge(v0, v1);
    return ei;
}

template <typename Scalar, typename Index>
auto SurfaceMesh<Scalar, Index>::get_first_corner_around_edge(Index e) const -> Index
{
    return get_attribute<Index>(m_reserved_ids.edge_to_first_corner()).get(e);
}

template <typename Scalar, typename Index>
auto SurfaceMesh<Scalar, Index>::get_next_corner_around_edge(Index c) const -> Index
{
    return get_attribute<Index>(m_reserved_ids.next_corner_around_edge()).get(c);
}

template <typename Scalar, typename Index>
auto SurfaceMesh<Scalar, Index>::get_first_corner_around_vertex(Index v) const -> Index
{
    return get_attribute<Index>(m_reserved_ids.vertex_to_first_corner()).get(v);
}

template <typename Scalar, typename Index>
auto SurfaceMesh<Scalar, Index>::get_next_corner_around_vertex(Index c) const -> Index
{
    return get_attribute<Index>(m_reserved_ids.next_corner_around_vertex()).get(c);
}

template <typename Scalar, typename Index>
auto SurfaceMesh<Scalar, Index>::count_num_corners_around_vertex(Index v) const -> Index
{
    // Count number of incident corners
    Index num_incident_corners = 0;
    foreach_corner_around_vertex(v, [&](Index) noexcept { ++num_incident_corners; });
    return num_incident_corners;
}

template <typename Scalar, typename Index>
auto SurfaceMesh<Scalar, Index>::count_num_corners_around_edge(Index e) const -> Index
{
    // Count number of incident corners
    Index num_incident_corners = 0;
    foreach_corner_around_edge(e, [&](Index) noexcept { ++num_incident_corners; });
    return num_incident_corners;
}

template <typename Scalar, typename Index>
auto SurfaceMesh<Scalar, Index>::get_one_facet_around_edge(Index e) const -> Index
{
    const Index c = get_first_corner_around_edge(e);
    la_runtime_assert(c != invalid<Index>());
    return get_corner_facet(c);
}

template <typename Scalar, typename Index>
auto SurfaceMesh<Scalar, Index>::get_one_corner_around_edge(Index e) const -> Index
{
    return get_first_corner_around_edge(e);
}

template <typename Scalar, typename Index>
auto SurfaceMesh<Scalar, Index>::get_one_corner_around_vertex(Index v) const -> Index
{
    return get_first_corner_around_vertex(v);
}

template <typename Scalar, typename Index>
bool SurfaceMesh<Scalar, Index>::is_boundary_edge(Index e) const
{
    const Index c = get_first_corner_around_edge(e);
    la_debug_assert(c != invalid<Index>());
    return (get_next_corner_around_edge(c) == invalid<Index>());
}

template <typename Scalar, typename Index>
void SurfaceMesh<Scalar, Index>::foreach_facet_around_vertex(
    Index v,
    function_ref<void(Index)> func) const
{
    // Loop over incident facets
    auto next_corner_around_vertex =
        get_attribute<Index>(m_reserved_ids.next_corner_around_vertex()).get_all();
    if (is_regular()) {
        for (Index c = get_first_corner_around_vertex(v); c != invalid<Index>();
             c = next_corner_around_vertex[c]) {
            Index f = c / m_vertex_per_facet;
            func(f);
        }
    } else {
        auto corner_to_facet = get_attribute<Index>(m_reserved_ids.corner_to_facet()).get_all();
        for (Index c = get_first_corner_around_vertex(v); c != invalid<Index>();
             c = next_corner_around_vertex[c]) {
            func(corner_to_facet[c]);
        }
    }
}

template <typename Scalar, typename Index>
void SurfaceMesh<Scalar, Index>::foreach_facet_around_edge(Index e, function_ref<void(Index)> func)
    const
{
    // Loop over incident facets
    auto next_corner_around_edge =
        get_attribute<Index>(m_reserved_ids.next_corner_around_edge()).get_all();
    if (is_regular()) {
        for (Index c = get_first_corner_around_edge(e); c != invalid<Index>();
             c = next_corner_around_edge[c]) {
            Index f = c / m_vertex_per_facet;
            func(f);
        }
    } else {
        auto corner_to_facet = get_attribute<Index>(m_reserved_ids.corner_to_facet()).get_all();
        for (Index c = get_first_corner_around_edge(e); c != invalid<Index>();
             c = next_corner_around_edge[c]) {
            func(corner_to_facet[c]);
        }
    }
}

template <typename Scalar, typename Index>
void SurfaceMesh<Scalar, Index>::foreach_corner_around_vertex(
    Index v,
    function_ref<void(Index)> func) const
{
    // Loop over incident facet corners
    auto next_corner_around_vertex =
        get_attribute<Index>(m_reserved_ids.next_corner_around_vertex()).get_all();
    for (Index c = get_first_corner_around_vertex(v); c != invalid<Index>();
         c = next_corner_around_vertex[c]) {
        func(c);
    }
}

template <typename Scalar, typename Index>
void SurfaceMesh<Scalar, Index>::foreach_corner_around_edge(Index e, function_ref<void(Index)> func)
    const
{
    // Loop over incident facet corners
    auto next_corner_around_edge =
        get_attribute<Index>(m_reserved_ids.next_corner_around_edge()).get_all();
    for (Index c = get_first_corner_around_edge(e); c != invalid<Index>();
         c = next_corner_around_edge[c]) {
        func(c);
    }
}

template <typename Scalar, typename Index>
void SurfaceMesh<Scalar, Index>::foreach_edge_around_vertex_with_duplicates(
    Index v,
    function_ref<void(Index)> func) const
{
    // Loop over incident facet edges
    auto corner_to_edge = get_attribute<Index>(m_reserved_ids.corner_to_edge()).get_all();
    auto next_corner_around_vertex =
        get_attribute<Index>(m_reserved_ids.next_corner_around_vertex()).get_all();
    if (is_regular()) {
        for (Index c = get_first_corner_around_vertex(v); c != invalid<Index>();
             c = next_corner_around_vertex[c]) {
            const Index f = c / m_vertex_per_facet;
            const Index nv = m_vertex_per_facet;
            const Index c_start = f * m_vertex_per_facet;
            const Index lv_curr = c - c_start;
            const Index lv_prev = (lv_curr + nv - 1) % nv;
            const Index e_curr = corner_to_edge[c_start + lv_curr];
            const Index e_prev = corner_to_edge[c_start + lv_prev];
            func(e_curr);
            func(e_prev);
        }
    } else {
        auto corner_to_facet = get_attribute<Index>(m_reserved_ids.corner_to_facet()).get_all();
        auto facet_to_first_corner =
            get_attribute<Index>(m_reserved_ids.facet_to_first_corner()).get_all();
        for (Index c = get_first_corner_around_vertex(v); c != invalid<Index>();
             c = next_corner_around_vertex[c]) {
            const Index f = corner_to_facet[c];
            const Index nv = get_facet_size(f);
            const Index c_start = facet_to_first_corner[f];
            const Index lv_curr = c - c_start;
            const Index lv_prev = (lv_curr + nv - 1) % nv;
            const Index e_curr = corner_to_edge[c_start + lv_curr];
            const Index e_prev = corner_to_edge[c_start + lv_prev];
            func(e_curr);
            func(e_prev);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
/// Private methods
////////////////////////////////////////////////////////////////////////////////

namespace {

template <typename Index, typename Attr>
void update_element_index(
    Attr&& attr,
    AttributeUsage usage,
    Index num_elems,
    function_ref<Index(Index)> old_to_new)
{
    using AttributeType = std::decay_t<decltype(attr)>;
    using ValueType = typename AttributeType::ValueType;
    if constexpr (std::is_same_v<ValueType, Index>) {
        if (attr.get_usage() == usage) {
            if constexpr (AttributeType::IsIndexed) {
                update_element_index<Index>(attr.values(), usage, num_elems, old_to_new);
            } else {
                for (Index i = 0; i < attr.get_num_elements(); ++i) {
                    const Index x = attr.get(i);
                    if (x != invalid<Index>()) {
                        la_runtime_assert(x >= 0 && x < num_elems);
                        attr.ref(i) = old_to_new(x);
                    }
                }
            }
        }
    } else {
        LA_IGNORE(usage);
        LA_IGNORE(num_elems);
        LA_IGNORE(old_to_new);
    }
}

} // namespace

template <typename Scalar, typename Index>
void SurfaceMesh<Scalar, Index>::reindex_vertices_internal(span<const Index> old_to_new_vertices)
{
    const Index num_vertices = get_num_vertices();

    // Update content of VertexIndex attributes
    auto remap_v = [&](Index i) { return old_to_new_vertices[i]; };
    seq_foreach_attribute_write(*this, [&](auto&& attr) {
        update_element_index<Index>(attr, AttributeUsage::VertexIndex, num_vertices, remap_v);
    });

    // Move vertex attributes
    seq_foreach_attribute_write<AttributeElement::Vertex>(*this, [&](auto&& attr) {
        for (Index v = 0; v < num_vertices; ++v) {
            if (old_to_new_vertices[v] != invalid<Index>()) {
                if (old_to_new_vertices[v] != v) {
                    auto from = attr.get_row(v);
                    auto to = attr.ref_row(old_to_new_vertices[v]);
                    std::copy(from.begin(), from.end(), to.begin());
                }
            }
        }
    });
}

template <typename Scalar, typename Index>
auto SurfaceMesh<Scalar, Index>::reindex_facets_internal(span<const Index> old_to_new_facets)
    -> std::pair<Index, Index>
{
    const Index num_edges = get_num_edges();
    const Index num_corners = get_num_corners();
    const Index num_facets = get_num_facets();

    auto reindex_edges = [&](auto old_to_new_edges) {
        // Update content of EdgeIndex attributes
        seq_foreach_attribute_write(*this, [&](auto&& attr) {
            update_element_index<Index>(
                attr,
                AttributeUsage::EdgeIndex,
                num_edges,
                old_to_new_edges);
        });

        // Move edge attributes
        seq_foreach_attribute_write<AttributeElement::Edge>(*this, [&](auto&& attr) {
            for (Index e = 0; e < num_edges; ++e) {
                if (old_to_new_edges(e) != invalid<Index>()) {
                    if (old_to_new_edges(e) != e) {
                        auto from = attr.get_row(e);
                        auto to = attr.ref_row(old_to_new_edges(e));
                        std::copy(from.begin(), from.end(), to.begin());
                    }
                }
            }
        });
    };

    auto reindex_corners = [&](auto old_to_new_corners) {
        // Repair connectivity chains and skip over deleted corners
        std::array<std::array<AttributeId, 2>, 2> ids = {
            {{{m_reserved_ids.vertex_to_first_corner(),
               m_reserved_ids.next_corner_around_vertex()}},
             {{m_reserved_ids.edge_to_first_corner(), m_reserved_ids.next_corner_around_edge()}}}};
        for (auto [id_first, id_next] : ids) {
            if (id_first != invalid_attribute_id()) {
                auto first_corner = ref_attribute<Index>(id_first).ref_all();
                auto next_corner = ref_attribute<Index>(id_next).ref_all();
                auto next_valid_corner = [&](Index ci) {
                    // Find the next corner that is not discarded
                    Index cj = next_corner[ci];
                    while (cj != invalid<Index>() && old_to_new_corners(cj) == invalid<Index>()) {
                        cj = next_corner[cj];
                    }
                    return cj;
                };
                for (Index& c0 : first_corner) {
                    if (id_first == m_reserved_ids.vertex_to_first_corner() &&
                        c0 == invalid<Index>()) {
                        // Isolated vertex, skip
                        continue;
                    }
                    if (old_to_new_corners(c0) == invalid<Index>()) {
                        // Replace head by next valid corner
                        c0 = next_valid_corner(c0);
                    }
                    for (Index c = c0; c != invalid<Index>(); c = next_corner[c]) {
                        // Iterate over the singly linked list and replace invalid items
                        if (next_corner[c] != invalid<Index>()) {
                            next_corner[c] = next_valid_corner(c);
                        }
                    }
                }
            }
        }

        // Update content of CornerIndex attributes
        seq_foreach_attribute_write(*this, [&](auto&& attr) {
            update_element_index<Index>(
                attr,
                AttributeUsage::CornerIndex,
                num_corners,
                old_to_new_corners);
        });

        // Move corner & indexed attributes
        auto move_corner_attributes = [&](auto&& attr) {
            for (Index c = 0; c < num_corners; ++c) {
                if (old_to_new_corners(c) != invalid<Index>()) {
                    if (old_to_new_corners(c) != c) {
                        auto from = attr.get_row(c);
                        auto to = attr.ref_row(old_to_new_corners(c));
                        std::copy(from.begin(), from.end(), to.begin());
                    }
                }
            }
        };
        seq_foreach_attribute_write<AttributeElement::Indexed | AttributeElement::Corner>(
            *this,
            [&](auto&& attr) {
                using AttributeType = std::decay_t<decltype(attr)>;
                if constexpr (AttributeType::IsIndexed) {
                    move_corner_attributes(attr.indices());
                } else {
                    move_corner_attributes(attr);
                }
            });
    };

    // Compute corner remapping
    Index new_num_corners = 0;
    if (is_hybrid()) {
        std::vector<Index> old_to_new_corners(num_corners);
        for (Index f = 0; f < num_facets; ++f) {
            for (Index c = get_facet_corner_begin(f); c < get_facet_corner_end(f); ++c) {
                if (old_to_new_facets[f] == invalid<Index>()) {
                    old_to_new_corners[c] = invalid<Index>();
                } else {
                    old_to_new_corners[c] = new_num_corners;
                    ++new_num_corners;
                }
            }
        }
        reindex_corners([&](Index c) noexcept { return old_to_new_corners[c]; });
    } else {
        const Index nvpf = get_vertex_per_facet();
        for (Index f = 0; f < num_facets; ++f) {
            if (old_to_new_facets[f] != invalid<Index>()) {
                new_num_corners += nvpf;
            }
        }
        reindex_corners([&](Index c) {
            const Index new_f = old_to_new_facets[c / nvpf];
            const Index lv = c % nvpf;
            if (new_f == invalid<Index>()) {
                return invalid<Index>();
            } else {
                return new_f * nvpf + lv;
            }
        });
    }

    // Compute edge remapping
    Index new_num_edges = 0;
    if (has_edges()) {
        auto edge_to_first_corner =
            get_attribute<Index>(m_reserved_ids.edge_to_first_corner()).get_all();
        std::vector<Index> old_to_new_edges(num_edges);
        for (Index e = 0; e < num_edges; ++e) {
            if (edge_to_first_corner[e] != invalid<Index>()) {
                old_to_new_edges[e] = new_num_edges;
                ++new_num_edges;
            } else {
                old_to_new_edges[e] = invalid<Index>();
            }
        }
        reindex_edges([&](Index e) noexcept { return old_to_new_edges[e]; });
    }

    // Update content of FacetIndex attributes
    auto remap_f = [&](Index i) { return old_to_new_facets[i]; };
    seq_foreach_attribute_write(*this, [&](auto&& attr) {
        update_element_index<Index>(attr, AttributeUsage::FacetIndex, num_facets, remap_f);
    });

    // Move facet attributes
    seq_foreach_attribute_write<AttributeElement::Facet>(*this, [&](auto&& attr) {
        for (Index f = 0; f < num_facets; ++f) {
            if (old_to_new_facets[f] != invalid<Index>()) {
                if (old_to_new_facets[f] != f) {
                    auto from = attr.get_row(f);
                    auto to = attr.ref_row(old_to_new_facets[f]);
                    std::copy(from.begin(), from.end(), to.begin());
                }
            }
        }
    });

    return std::make_pair(new_num_corners, new_num_edges);
}

template <typename Scalar, typename Index>
template <AttributeElement element>
void SurfaceMesh<Scalar, Index>::resize_elements_internal(Index num_elements)
{
    static_assert(element != AttributeElement::Indexed);
    static_assert(element != AttributeElement::Value);
    if constexpr (element == AttributeElement::Corner) {
        seq_foreach_attribute_write<AttributeElement::Indexed>(*this, [&](auto&& attr) {
            attr.indices().resize_elements(num_elements);
        });
    }
    seq_foreach_attribute_write<element>(*this, [&](auto&& attr) {
        attr.resize_elements(num_elements);
    });
}

template <typename Scalar, typename Index>
void SurfaceMesh<Scalar, Index>::resize_vertices_internal(Index num_vertices)
{
    m_num_vertices = num_vertices;
    resize_elements_internal<AttributeElement::Vertex>(num_vertices);
}

template <typename Scalar, typename Index>
void SurfaceMesh<Scalar, Index>::resize_facets_internal(Index num_facets)
{
    m_num_facets = num_facets;
    resize_elements_internal<AttributeElement::Facet>(num_facets);
}

template <typename Scalar, typename Index>
void SurfaceMesh<Scalar, Index>::resize_corners_internal(Index num_corners)
{
    m_num_corners = num_corners;
    resize_elements_internal<AttributeElement::Corner>(num_corners);
}

template <typename Scalar, typename Index>
void SurfaceMesh<Scalar, Index>::resize_edges_internal(Index num_edges)
{
    m_num_edges = num_edges;
    resize_elements_internal<AttributeElement::Edge>(num_edges);
}

template <typename Scalar, typename Index>
auto SurfaceMesh<Scalar, Index>::reserve_indices_internal(Index num_facets, Index facet_size)
    -> span<Index>
{
    if (is_regular() && (m_vertex_per_facet == 0 || m_vertex_per_facet == facet_size)) {
        // A faster pass for regular mesh.
        const Index total_num_facets = get_num_facets() + num_facets;
        resize_facets_internal(total_num_facets);
        m_vertex_per_facet = facet_size;
        resize_corners_internal(total_num_facets * facet_size);
        return ref_attribute<Index>(m_reserved_ids.corner_to_vertex())
            .ref_last(num_facets * facet_size);
    } else {
        return reserve_indices_internal(num_facets, [&facet_size](Index i) noexcept -> Index {
            (void)i;
            return facet_size;
        });
    }
}

template <typename Scalar, typename Index>
auto SurfaceMesh<Scalar, Index>::reserve_indices_internal(span<const Index> facet_sizes)
    -> span<Index>
{
    return reserve_indices_internal(
        static_cast<Index>(facet_sizes.size()),
        [&facet_sizes](Index i) noexcept -> Index { return facet_sizes[i]; });
}

template <typename Scalar, typename Index>
auto SurfaceMesh<Scalar, Index>::reserve_indices_internal(
    Index num_facets,
    GetFacetsSizeFunction get_facets_size) -> span<Index>
{
    const Index old_num_corners = get_num_corners();
    const Index old_num_facets = get_num_facets();
    const bool was_regular = is_regular();

    Index last_offset = get_num_corners();
    resize_facets_internal(old_num_facets + num_facets);
    if (is_regular()) {
        la_debug_assert(m_reserved_ids.facet_to_first_corner() == invalid_attribute_id());

        // Mesh is regular (or possibly empty), we need to check if we need to add an "offset"
        // attribute.
        span<Index> new_facet_to_first_corner;
        for (Index i = 0; i < num_facets; ++i) {
            const Index facet_size = get_facets_size(i);
            la_runtime_assert(facet_size > 0);

            // Mesh has been switched to hybrid, need to insert offset index for each newly
            // inserted facet
            if (!new_facet_to_first_corner.empty()) {
                la_debug_assert(m_reserved_ids.facet_to_first_corner() != invalid_attribute_id());
                new_facet_to_first_corner[i] = last_offset;
            }
            last_offset += facet_size;

            // If the mesh was empty, the first new facet will use regular storage
            if (old_num_facets == 0 && i == 0) {
                m_vertex_per_facet = facet_size;
            }

            // Non-regular mesh, switch to hybrid storage
            if (m_vertex_per_facet != 0 && m_vertex_per_facet != facet_size) {
                m_reserved_ids.facet_to_first_corner() = create_attribute_internal<Index>(
                    s_reserved_names.facet_to_first_corner(),
                    AttributeElement::Facet,
                    AttributeUsage::CornerIndex,
                    1);
                m_reserved_ids.corner_to_facet() = create_attribute_internal<Index>(
                    s_reserved_names.corner_to_facet(),
                    AttributeElement::Corner,
                    AttributeUsage::FacetIndex,
                    1);
                auto first_offsets =
                    ref_attribute<Index>(m_reserved_ids.facet_to_first_corner()).ref_all();
                for (Index j = 0; j <= old_num_facets + i; ++j) {
                    first_offsets[j] = j * m_vertex_per_facet;
                }
                new_facet_to_first_corner =
                    ref_attribute<Index>(m_reserved_ids.facet_to_first_corner())
                        .ref_last(num_facets);
                m_vertex_per_facet = 0;
            }
        }

    } else {
        // Mesh is hybrid, so we already have an offset buffer to work with.
        la_debug_assert(m_reserved_ids.facet_to_first_corner() != invalid_attribute_id());
        la_debug_assert(m_vertex_per_facet == 0);

        auto new_facets =
            ref_attribute<Index>(m_reserved_ids.facet_to_first_corner()).ref_last(num_facets);
        for (Index i = 0; i < num_facets; ++i) {
            new_facets[i] = last_offset;
            last_offset += get_facets_size(i);
        }
    }
    resize_corners_internal(last_offset);
    if (is_hybrid()) {
        // If the mesh has regular storage before, we need to update the corner_to_facet attribute
        // for _all_ mesh corners. Otherwise just update for newly inserted facets.
        compute_corner_to_facet_internal(was_regular ? 0 : old_num_facets, get_num_facets());
    }

    return ref_attribute<Index>(m_reserved_ids.corner_to_vertex())
        .ref_last(last_offset - old_num_corners);
}

template <typename Scalar, typename Index>
void SurfaceMesh<Scalar, Index>::compute_corner_to_facet_internal(
    Index facet_begin,
    Index facet_end)
{
    la_debug_assert(is_hybrid());
    auto c2f = ref_attribute<Index>(m_reserved_ids.corner_to_facet()).ref_all();
    for (Index f = facet_begin; f < facet_end; ++f) {
        for (Index c = get_facet_corner_begin(f); c < get_facet_corner_end(f); ++c) {
            c2f[c] = f;
        }
    }
}

template <typename Scalar, typename Index>
template <typename ValueType>
void SurfaceMesh<Scalar, Index>::wrap_as_attribute_internal(
    Attribute<std::decay_t<ValueType>>& attr,
    size_t num_values,
    SharedSpan<ValueType> shared_values)
{
    if constexpr (std::is_const_v<ValueType>) {
        attr.wrap_const(std::move(shared_values), num_values);
    } else {
        attr.wrap(std::move(shared_values), num_values);
    }
}

template <typename Scalar, typename Index>
template <typename ValueType>
void SurfaceMesh<Scalar, Index>::wrap_as_attribute_internal(
    Attribute<std::decay_t<ValueType>>& attr,
    size_t num_values,
    span<ValueType> values_view)
{
    if constexpr (std::is_const_v<ValueType>) {
        attr.wrap_const(values_view, num_values);
    } else {
        attr.wrap(values_view, num_values);
    }
}

template <typename Scalar, typename Index>
template <typename OffsetSpan, typename FacetSpan>
AttributeId SurfaceMesh<Scalar, Index>::wrap_as_facets_internal(
    OffsetSpan offsets,
    Index num_facets,
    FacetSpan facets,
    Index num_corners)
{
    la_runtime_assert(facets.size() >= num_corners);
    la_runtime_assert(offsets.size() >= num_facets);
    m_vertex_per_facet = 0;
    if (m_reserved_ids.facet_to_first_corner() == invalid_attribute_id()) {
        // Create new facet -> first corner index attribute
        m_reserved_ids.facet_to_first_corner() = m_attributes->template create<Index>(
            s_reserved_names.facet_to_first_corner(),
            AttributeElement::Facet,
            AttributeUsage::CornerIndex,
            1);
        set_attribute_default_internal<Index>(s_reserved_names.facet_to_first_corner());
        // Create new corner -> facet index attribute
        la_debug_assert(m_reserved_ids.corner_to_facet() == invalid_attribute_id());
        m_reserved_ids.corner_to_facet() = m_attributes->template create<Index>(
            s_reserved_names.corner_to_facet(),
            AttributeElement::Corner,
            AttributeUsage::FacetIndex,
            1);
        set_attribute_default_internal<Index>(s_reserved_names.corner_to_facet());
    }

    // Wrap facet -> first corner index
    {
        auto& attr = m_attributes->template write<Index>(m_reserved_ids.facet_to_first_corner());
        wrap_as_attribute_internal(attr, num_facets, offsets);
        resize_facets_internal(num_facets);
    }

    // Wrap corner -> vertex index
    {
        auto& attr = m_attributes->template write<Index>(m_reserved_ids.corner_to_vertex());
        wrap_as_attribute_internal(attr, num_corners, facets);
        resize_corners_internal(num_corners);
    }

    // Compute inverse mapping (corner -> facet index)
    compute_corner_to_facet_internal(0, get_num_facets());
    return m_reserved_ids.corner_to_vertex();
}


template <typename Scalar, typename Index>
template <typename ValueSpan, typename IndexSpan>
AttributeId SurfaceMesh<Scalar, Index>::wrap_as_attribute_internal(
    std::string_view name,
    AttributeElement element,
    AttributeUsage usage,
    size_t num_values,
    size_t num_channels,
    ValueSpan values,
    IndexSpan indices)
{
    using _ValueType_ = typename ValueSpan::value_type;
    using _IndexType_ = typename IndexSpan::value_type;
    static_assert(std::is_same_v<Index, _IndexType_>, "Invalid index type");

    // If usage is an element index type, then ValueType must be the same as the mesh's Index type.
    if (usage == AttributeUsage::VertexIndex || usage == AttributeUsage::FacetIndex ||
        usage == AttributeUsage::CornerIndex || usage == AttributeUsage::EdgeIndex) {
        la_runtime_assert((std::is_same_v<_ValueType_, Index>));
    }

    if (element != AttributeElement::Indexed) {
        la_runtime_assert(values.size() >= num_values * num_channels);
        auto id = m_attributes->template create<_ValueType_>(name, element, usage, num_channels);
        set_attribute_default_internal<_ValueType_>(name);
        auto& attr = m_attributes->template write<_ValueType_>(id);
        wrap_as_attribute_internal(attr, num_values, values);
        return id;
    } else {
        const size_t num_corners = get_num_elements_internal(AttributeElement::Corner);
        la_runtime_assert(values.size() >= num_values * num_channels);
        la_runtime_assert(indices.size() >= num_corners);
        auto id = m_attributes->template create_indexed<_ValueType_>(name, usage, num_channels);
        auto& attr = m_attributes->template write_indexed<_ValueType_>(id);
        wrap_as_attribute_internal(attr.values(), num_values, values);
        wrap_as_attribute_internal(attr.indices(), num_corners, indices);
        return id;
    }
}

////////////////////////////////////////////////////////////////////////////////
// Explicit template instantiations
////////////////////////////////////////////////////////////////////////////////

// Explicit instantiation of the templated mesh attribute methods.
#define LA_X_surface_mesh_attr(ValueType, Scalar, Index)                                           \
    template AttributeId SurfaceMesh<Scalar, Index>::create_attribute(                             \
        std::string_view name,                                                                     \
        AttributeElement element,                                                                  \
        size_t num_channels,                                                                       \
        AttributeUsage usage,                                                                      \
        span<const ValueType> initial_values,                                                      \
        span<const Index> initial_indices,                                                         \
        AttributeCreatePolicy policy);                                                             \
    template AttributeId SurfaceMesh<Scalar, Index>::create_attribute(                             \
        std::string_view name,                                                                     \
        AttributeElement element,                                                                  \
        AttributeUsage usage,                                                                      \
        size_t num_channels,                                                                       \
        span<const ValueType> initial_values,                                                      \
        span<const Index> initial_indices,                                                         \
        AttributeCreatePolicy policy);                                                             \
    template AttributeId SurfaceMesh<Scalar, Index>::wrap_as_attribute(                            \
        std::string_view name,                                                                     \
        AttributeElement element,                                                                  \
        AttributeUsage usage,                                                                      \
        size_t num_channels,                                                                       \
        span<ValueType> values_view);                                                              \
    template AttributeId SurfaceMesh<Scalar, Index>::wrap_as_attribute(                            \
        std::string_view name,                                                                     \
        AttributeElement element,                                                                  \
        AttributeUsage usage,                                                                      \
        size_t num_channels,                                                                       \
        SharedSpan<ValueType> shared_values);                                                      \
    template AttributeId SurfaceMesh<Scalar, Index>::wrap_as_const_attribute(                      \
        std::string_view name,                                                                     \
        AttributeElement element,                                                                  \
        AttributeUsage usage,                                                                      \
        size_t num_channels,                                                                       \
        span<const ValueType> values_view);                                                        \
    template AttributeId SurfaceMesh<Scalar, Index>::wrap_as_const_attribute(                      \
        std::string_view name,                                                                     \
        AttributeElement element,                                                                  \
        AttributeUsage usage,                                                                      \
        size_t num_channels,                                                                       \
        SharedSpan<const ValueType> shared_values);                                                \
    template AttributeId SurfaceMesh<Scalar, Index>::wrap_as_indexed_attribute(                    \
        std::string_view name,                                                                     \
        AttributeUsage usage,                                                                      \
        size_t num_values,                                                                         \
        size_t num_channels,                                                                       \
        span<ValueType> values_view,                                                               \
        span<Index> indices_view);                                                                 \
    template AttributeId SurfaceMesh<Scalar, Index>::wrap_as_indexed_attribute(                    \
        std::string_view name,                                                                     \
        AttributeUsage usage,                                                                      \
        size_t num_values,                                                                         \
        size_t num_channels,                                                                       \
        SharedSpan<ValueType> shared_values,                                                       \
        SharedSpan<Index> shared_indices);                                                         \
    template AttributeId SurfaceMesh<Scalar, Index>::wrap_as_indexed_attribute(                    \
        std::string_view name,                                                                     \
        AttributeUsage usage,                                                                      \
        size_t num_values,                                                                         \
        size_t num_channels,                                                                       \
        span<ValueType> values_view,                                                               \
        SharedSpan<Index> shared_indices);                                                         \
    template AttributeId SurfaceMesh<Scalar, Index>::wrap_as_indexed_attribute(                    \
        std::string_view name,                                                                     \
        AttributeUsage usage,                                                                      \
        size_t num_values,                                                                         \
        size_t num_channels,                                                                       \
        SharedSpan<ValueType> shared_values,                                                       \
        span<Index> indices_view);                                                                 \
    template AttributeId SurfaceMesh<Scalar, Index>::wrap_as_const_indexed_attribute(              \
        std::string_view name,                                                                     \
        AttributeUsage usage,                                                                      \
        size_t num_values,                                                                         \
        size_t num_channels,                                                                       \
        span<const ValueType> values_view,                                                         \
        span<const Index> indices_view);                                                           \
    template AttributeId SurfaceMesh<Scalar, Index>::wrap_as_const_indexed_attribute(              \
        std::string_view name,                                                                     \
        AttributeUsage usage,                                                                      \
        size_t num_values,                                                                         \
        size_t num_channels,                                                                       \
        SharedSpan<const ValueType> shared_values,                                                 \
        SharedSpan<const Index> shared_indices);                                                   \
    template AttributeId SurfaceMesh<Scalar, Index>::wrap_as_const_indexed_attribute(              \
        std::string_view name,                                                                     \
        AttributeUsage usage,                                                                      \
        size_t num_values,                                                                         \
        size_t num_channels,                                                                       \
        span<const ValueType> values_view,                                                         \
        SharedSpan<const Index> shared_indices);                                                   \
    template AttributeId SurfaceMesh<Scalar, Index>::wrap_as_const_indexed_attribute(              \
        std::string_view name,                                                                     \
        AttributeUsage usage,                                                                      \
        size_t num_values,                                                                         \
        size_t num_channels,                                                                       \
        SharedSpan<const ValueType> shared_values,                                                 \
        span<const Index> indices_view);                                                           \
    template std::shared_ptr<Attribute<ValueType>>                                                 \
    SurfaceMesh<Scalar, Index>::delete_and_export_attribute(                                       \
        std::string_view name,                                                                     \
        AttributeDeletePolicy delete_policy,                                                       \
        AttributeExportPolicy export_policy);                                                      \
    template std::shared_ptr<const Attribute<ValueType>>                                           \
    SurfaceMesh<Scalar, Index>::delete_and_export_const_attribute(                                 \
        std::string_view name,                                                                     \
        AttributeDeletePolicy delete_policy,                                                       \
        AttributeExportPolicy export_policy);                                                      \
    template std::shared_ptr<IndexedAttribute<ValueType, Index>>                                   \
    SurfaceMesh<Scalar, Index>::delete_and_export_indexed_attribute(                               \
        std::string_view name,                                                                     \
        AttributeExportPolicy policy);                                                             \
    template std::shared_ptr<const IndexedAttribute<ValueType, Index>>                             \
    SurfaceMesh<Scalar, Index>::delete_and_export_const_indexed_attribute(                         \
        std::string_view name,                                                                     \
        AttributeExportPolicy policy);                                                             \
    template bool SurfaceMesh<Scalar, Index>::is_attribute_type<ValueType>(std::string_view name)  \
        const;                                                                                     \
    template bool SurfaceMesh<Scalar, Index>::is_attribute_type<ValueType>(AttributeId id) const;  \
    template const Attribute<ValueType>& SurfaceMesh<Scalar, Index>::get_attribute(                \
        std::string_view name) const;                                                              \
    template const Attribute<ValueType>& SurfaceMesh<Scalar, Index>::get_attribute(AttributeId id) \
        const;                                                                                     \
    template Attribute<ValueType>& SurfaceMesh<Scalar, Index>::ref_attribute(                      \
        std::string_view name);                                                                    \
    template Attribute<ValueType>& SurfaceMesh<Scalar, Index>::ref_attribute(AttributeId id);      \
    template const IndexedAttribute<ValueType, Index>&                                             \
    SurfaceMesh<Scalar, Index>::get_indexed_attribute(std::string_view name) const;                \
    template const IndexedAttribute<ValueType, Index>&                                             \
    SurfaceMesh<Scalar, Index>::get_indexed_attribute(AttributeId id) const;                       \
    template IndexedAttribute<ValueType, Index>&                                                   \
    SurfaceMesh<Scalar, Index>::ref_indexed_attribute(std::string_view name);                      \
    template IndexedAttribute<ValueType, Index>&                                                   \
    SurfaceMesh<Scalar, Index>::ref_indexed_attribute(AttributeId id);

#define LA_X_surface_mesh_aux(_, ValueType) LA_SURFACE_MESH_X(surface_mesh_attr, ValueType)
LA_ATTRIBUTE_X(surface_mesh_aux, 0)

// Explicit instantiation of the SurfaceMesh::create_attribute_from() method.
#define fst(first, second) first
#define snd(first, second) second
#define LA_X_surface_mesh_mesh_other(ScalarIndex, OtherScalar, OtherIndex)                     \
    template SurfaceMesh<fst ScalarIndex, snd ScalarIndex>                                     \
    SurfaceMesh<fst ScalarIndex, snd ScalarIndex>::stripped_copy(                              \
        const SurfaceMesh<OtherScalar, OtherIndex>& other);                                    \
    template SurfaceMesh<fst ScalarIndex, snd ScalarIndex>                                     \
    SurfaceMesh<fst ScalarIndex, snd ScalarIndex>::stripped_move(                              \
        SurfaceMesh<OtherScalar, OtherIndex>&& other);                                         \
    template AttributeId SurfaceMesh<fst ScalarIndex, snd ScalarIndex>::create_attribute_from( \
        std::string_view name,                                                                 \
        const SurfaceMesh<OtherScalar, OtherIndex>& source_mesh,                               \
        std::string_view source_name);

// NOTE: This is a dirty workaround because nesting two LA_SURFACE_MESH_X macros doesn't quite work.
// I am not sure if there is a proper way to do this.
#define LA_SURFACE_MESH2_X(mode, data)                                     \
    LA_X_##mode(data, float, uint32_t) LA_X_##mode(data, double, uint32_t) \
        LA_X_##mode(data, float, uint64_t) LA_X_##mode(data, double, uint64_t)

#define LA_X_surface_mesh_mesh_aux(_, Scalar, Index) \
    LA_SURFACE_MESH2_X(surface_mesh_mesh_other, (Scalar, Index))
LA_SURFACE_MESH_X(surface_mesh_mesh_aux, 0)

// Explicit instantiation of the SurfaceMesh class
#define LA_X_surface_mesh_class(_, Scalar, Index) template class SurfaceMesh<Scalar, Index>;
LA_SURFACE_MESH_X(surface_mesh_class, 0)

} // namespace lagrange
