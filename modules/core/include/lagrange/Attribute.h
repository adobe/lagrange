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
#pragma once

#include <lagrange/AttributeFwd.h>
#include <lagrange/utils/SharedSpan.h>
#include <lagrange/utils/span.h>

#include <vector>

namespace lagrange {

///
/// @defgroup   group-surfacemesh-attr Attributes
/// @ingroup    group-surfacemesh
///
/// Manipulating mesh attributes. Please read our [user guide](../UserGuide/Core/Attributes/) for an
/// introduction.
///
/// @{

///
/// Enum describing at runtime the value type of an attribute. This can be accessed from the base
/// attribute class and enables safe downcasting without global RTTI.
///
enum class AttributeValueType : uint8_t;

///
/// Base handle for attributes. This is a common base class to allow for type erasure.
///
class AttributeBase
{
public:
    ///
    /// Constructs a new instance.
    ///
    /// @param[in]  element       Element type (vertex, facet, etc.).
    /// @param[in]  usage         Usage tag.
    /// @param[in]  num_channels  Number of channels.
    ///
    AttributeBase(AttributeElement element, AttributeUsage usage, size_t num_channels);

    ///
    /// Destroys the object.
    ///
    virtual ~AttributeBase();

    ///
    /// Move constructor.
    ///
    /// @param      other  Instance to move from.
    ///
    AttributeBase(AttributeBase&& other) = default;

    ///
    /// Assignment move operator.
    ///
    /// @param      other  Instance to move from.
    ///
    /// @return     The result of the assignment.
    ///
    AttributeBase& operator=(AttributeBase&& other) = default;

    ///
    /// Copy constructor.
    ///
    /// @param[in]  other  Instance to copy from.
    ///
    AttributeBase(const AttributeBase& other) = default;

    ///
    /// Assignment copy operator.
    ///
    /// @param[in]  other  Instance to copy from.
    ///
    /// @return     The result of the assignment.
    ///
    AttributeBase& operator=(const AttributeBase& other) = default;

    ///
    /// Gets the attribute value type.
    ///
    /// @return     An enum describing the value type.
    ///
    [[nodiscard]] virtual AttributeValueType get_value_type() const = 0;

    ///
    /// Gets the attribute element type.
    ///
    /// @return     The element type.
    ///
    [[nodiscard]] AttributeElement get_element_type() const { return m_element; }

    ///
    /// Gets the attribute usage tag.
    ///
    /// @return     The usage tag.
    ///
    [[nodiscard]] AttributeUsage get_usage() const { return m_usage; }

    ///
    /// Gets the number of channels for the attribute.
    ///
    /// @return     The number of channels.
    ///
    [[nodiscard]] size_t get_num_channels() const { return m_num_channels; }

    ///
    /// Sets the attribute usage tag.
    ///
    /// @note       No check is performed, use with caution!
    ///
    /// @param[in]  usage  New usage tag.
    ///
    void unsafe_set_usage(AttributeUsage usage) { m_usage = usage; }

    ///
    /// Sets the attribute element type.
    ///
    /// @note       No check is performed, use with caution!
    ///
    /// @param[in]  element  New element type.
    ///
    void unsafe_set_element_type(AttributeElement element) { m_element = element; }

protected:
    /// Element type (vertex, facet, indexed, etc.).
    AttributeElement m_element;

    /// Attribute usage tag.
    AttributeUsage m_usage;

    /// Number of channel for each value.
    size_t m_num_channels = 0;
};

///
/// Derived attribute class that stores the actual information.
///
/// @tparam     ValueType_  Attribute value type.
///
template <typename ValueType_>
class Attribute : public AttributeBase
{
public:
    /// Attribute value type.
    using ValueType = ValueType_;

    /// Whether this attribute type is indexed.
    static constexpr bool IsIndexed = false;

    // Static assertions to prevent users from instantiating unsupported types. We do not use the
    // macros from AttributeTypes.h since we do not want to implicitly expose them in the main
    // Attribute.h header.
    static_assert(
        std::is_same_v<ValueType, int8_t> || std::is_same_v<ValueType, int16_t> ||
            std::is_same_v<ValueType, int32_t> || std::is_same_v<ValueType, int64_t> ||
            std::is_same_v<ValueType, uint8_t> || std::is_same_v<ValueType, uint16_t> ||
            std::is_same_v<ValueType, uint32_t> || std::is_same_v<ValueType, uint64_t> ||
            std::is_same_v<ValueType, float> || std::is_same_v<ValueType, double>,
        "Attribute's ValueType template parameter can only be float, double, or a fixed size "
        "integer type.");

public:
    ///
    /// @name Attribute construction
    /// @{

    ///
    /// Constructs a new instance.
    ///
    /// @param[in]  element       Element type (vertex, facet, etc.).
    /// @param[in]  usage         Usage tag.
    /// @param[in]  num_channels  Number of channels.
    ///
    Attribute(AttributeElement element, AttributeUsage usage, size_t num_channels);

    ///
    /// Destroys the object.
    ///
    ~Attribute() override;

    ///
    /// Move constructor.
    ///
    /// @param      other  Instance to move from.
    ///
    Attribute(Attribute&& other) noexcept;

    ///
    /// Assignment move operator.
    ///
    /// @param      other  Instance to move from.
    ///
    /// @return     The result of the assignment.
    ///
    Attribute& operator=(Attribute&& other) noexcept;

    ///
    /// Copy constructor.
    ///
    /// @param[in]  other  Instance to copy from.
    ///
    explicit Attribute(const Attribute& other);

    ///
    /// Assignment copy operator.
    ///
    /// @param[in]  other  Instance to copy from.
    ///
    /// @return     The result of the assignment.
    ///
    Attribute& operator=(const Attribute& other);

    ///
    /// Cast copy operator. Creates an attribute by copying and casting values from another
    /// attribute with a different ValueType. Will print a warning if the source and target value
    /// types are identical.
    ///
    /// @param[in]  other       The other attribute to copy & cast from.
    ///
    /// @tparam     OtherValue  Value type for the other attribute.
    ///
    /// @return     A new attribute object whose values have been cast to the desired type.
    ///
    template <typename OtherValue>
    static Attribute cast_copy(const Attribute<OtherValue>& other);

    ///
    /// Cast assignment operator. Replace the current attribute by copying and casting values from
    /// another attribute with a different ValueType. Will print a warning if the source and target
    /// value types are identical.
    ///
    /// @param[in]  other       The other attribute to copy & cast from.
    ///
    /// @tparam     OtherValue  Value type for the other attribute.
    ///
    /// @return     A reference to the assignment result.
    ///
    template <typename OtherValue>
    Attribute& cast_assign(const Attribute<OtherValue>& other);

    ///
    /// Gets the attribute value type.
    ///
    /// @return     An enum describing the value type.
    ///
    [[nodiscard]] AttributeValueType get_value_type() const override;

    ///
    /// Wraps an external buffer into the attribute. The pointer must remain valid for the lifetime
    /// of the attribute. Note that only the number of element is allowed to change when wrapping an
    /// external buffer (the number of channels is fixed during the attribute construction).
    ///
    /// @param[in]  buffer        Pointer to an external buffer to be used as storage. The pointed
    ///                           buffer must have a capacity (determined by span::size()) that is
    ///                           large enough to store num elements x num channels entries. The
    ///                           pointed buffer can be larger than required, in which case the
    ///                           padding capacity can be used to grow the attribute, as determined
    ///                           by the AttributeGrowthPolicy.
    /// @param[in]  num_elements  New number of elements associated with the attribute.
    ///
    void wrap(span<ValueType> buffer, size_t num_elements);

    ///
    /// Wraps an external buffer into the attribute. The buffer ownership is shared with the
    /// attribute. Note that only the number of element is allowed to change when wrapping an
    /// external buffer (the number of channels is fixed during the attribute construction).
    ///
    /// @param[in]  shared_buffer Pointer to an external buffer managed by a SharedSpan to be used
    ///                           as storage.  This pointer exposes a view of the buffer managed by
    ///                           the owner. The buffer must have a capacity (determined by
    ///                           buffer_ptr.size()) that is large enough to store num elements x
    ///                           num channels entries.  The buffer can be larger than required, in
    ///                           which case the padding capacity can be used to grow the attribute,
    ///                           as determined by the AttributeGrowthPolicy.
    /// @param[in]  num_elements  New number of elements associated with the attribute.
    ///
    void wrap(SharedSpan<ValueType> buffer_ptr, size_t num_elements);

    ///
    /// Wraps a const external buffer into the attribute. The pointer must remain valid for the
    /// lifetime of the attribute. Note that only the number of element is allowed to change when
    /// wrapping an external buffer (the number of channels is fixed during the attribute
    /// construction). Following this, any write operation to the attribute will throw an exception.
    ///
    /// @param[in]  buffer        Pointer to an external buffer to be used as storage. The pointed
    ///                           buffer must have a capacity (determined by span::size()) that is
    ///                           large enough to store num elements x num channels entries. The
    ///                           pointed buffer can be larger than required, in which case the
    ///                           padding capacity can be used to grow the attribute, as determined
    ///                           by the AttributeGrowthPolicy.
    /// @param[in]  num_elements  New number of elements associated with the attribute.
    ///
    void wrap_const(span<const ValueType> buffer, size_t num_elements);

    ///
    /// Wraps a const external buffer into the attribute. The buffer ownership is shared with the
    /// attribute. Note that only the number of element is allowed to change when wrapping an
    /// external buffer (the number of channels is fixed during the attribute construction).
    ///
    /// @param[in]  shared_buffer Pointer to an external buffer managed by a SharedSpan to be used
    ///                           as storage.  This pointer exposes a view of the buffer managed by
    ///                           the owner.  The buffer must have a capacity (determined by
    ///                           shared_buffer.size()) that is large enough to store num elements x
    ///                           num channels entries.  The buffer can be larger than required, in
    ///                           which case the padding capacity can be used to grow the attribute,
    ///                           as determined by the AttributeGrowthPolicy.
    /// @param[in]  num_elements  New number of elements associated with the attribute.
    ///
    void wrap_const(SharedSpan<const ValueType> shared_buffer, size_t num_elements);

    /// @}
    /// @name Attribute growth
    /// @{

    ///
    /// Sets the default value to use when growing the attribute.
    ///
    /// @param[in]  value  New default value.
    ///
    void set_default_value(ValueType value) { m_default_value = value; }

    ///
    /// Gets the default value to use when growing the attribute.
    ///
    ValueType get_default_value() const { return m_default_value; }

    ///
    /// Sets the growth policy for external buffers.
    ///
    /// @param[in]  policy  New policy.
    ///
    void set_growth_policy(AttributeGrowthPolicy policy) { m_growth_policy = policy; }

    ///
    /// Gets the growth policy for external buffers.
    ///
    /// @return     The growth policy.
    ///
    AttributeGrowthPolicy get_growth_policy() const { return m_growth_policy; }

    ///
    /// Sets the shrink policy for external buffers.
    ///
    /// @param[in]  policy  New policy.
    ///
    void set_shrink_policy(AttributeShrinkPolicy policy) { m_shrink_policy = policy; }

    ///
    /// Gets the shrink policy for external buffers.
    ///
    /// @return     The shrink policy.
    ///
    AttributeShrinkPolicy get_shrink_policy() const { return m_shrink_policy; }

    ///
    /// Sets the write policy for read-only external buffers.
    ///
    /// @param[in]  policy  New policy.
    ///
    void set_write_policy(AttributeWritePolicy policy) { m_write_policy = policy; }

    ///
    /// Gets the write policy for read-only external buffers.
    ///
    /// @return     The attribute write policy.
    ///
    AttributeWritePolicy get_write_policy() const { return m_write_policy; }

    ///
    /// Sets the copy policy for external buffers.
    ///
    /// @param[in] policy  New policy
    ///
    void set_copy_policy(AttributeCopyPolicy policy) { m_copy_policy = policy; }

    ///
    /// Gets the copy policy for external buffers.
    ///
    /// @return     The attribute copy policy.
    ///
    AttributeCopyPolicy get_copy_policy() const { return m_copy_policy; }

    ///
    /// Sets the cast policy.
    ///
    /// @param[in]  policy  New policy.
    ///
    void set_cast_policy(AttributeCastPolicy policy) { m_cast_policy = policy; }

    ///
    /// Gets the cast policy.
    ///
    /// @return     The attribute cast policy.
    ///
    AttributeCastPolicy get_cast_policy() const { return m_cast_policy; }

    ///
    /// Creates an internal copy of the attribute data. The attribute buffer must be external before
    /// calling this function. An internal copy of the buffer is created (including padding size).
    /// This can be used prior to attribute export to ensure lifetime of the underlying object.
    ///
    void create_internal_copy();

    ///
    /// Clears the attribute buffer (new number of elements is 0).
    ///
    void clear();

    ///
    /// Shrink attribute buffer to fit the current number of entries. If the attribute points to an
    /// external buffer, an internal copy will be created if the external buffer capacity exceeds
    /// the number of entries in the attribute.
    ///
    void shrink_to_fit();

    ///
    /// Reserve enough memory for @p new_cap entries. The new capacity does not need to be a
    /// multiple of the number of channels (e.g. one may want to pad the last vertex coordinate with
    /// a single value).
    ///
    /// @param[in]  new_cap  The new buffer capacity.
    ///
    void reserve_entries(size_t new_cap);

    ///
    /// Resize the buffer to contain @p num_elements elements. New attribute entries will be
    /// zero-initialized.
    ///
    /// @param[in]  num_elements  New number of elements.
    ///
    void resize_elements(size_t num_elements);

    ///
    /// Inserts values for new elements.
    ///
    /// @param[in]  values  Values to be inserted. The span size must be a multiple of number of
    ///                     channels for the attribute.
    ///
    void insert_elements(span<const ValueType> values);

    ///
    /// Inserts values for new elements.
    ///
    /// @param[in]  values  Values to be inserted. The span size must be a multiple of number of
    ///                     channels for the attribute.
    ///
    void insert_elements(std::initializer_list<const ValueType> values);

    ///
    /// Inserts new elements. Use set_default_value to use a non-zero default value to initialize
    /// the new elements.
    ///
    /// @see        set_default_value
    ///
    /// @param[in]  count  The number elements to insert.
    ///
    void insert_elements(size_t count);

    /// @}
    /// @name Attribute access
    /// @{

    ///
    ///
    /// Test whether the attribute is empty (its size is 0).
    ///
    /// @return     Whether the attribute is empty.
    ///
    [[nodiscard]] bool empty() const { return m_num_elements == 0; }

    ///
    /// Gets the number of elements.
    ///
    /// @return     The number of elements.
    ///
    [[nodiscard]] size_t get_num_elements() const { return m_num_elements; }

    ///
    /// Checks whether an attribute buffer is external or internally managed. External buffers must
    /// remain valid for the lifetime of the attribute object.
    ///
    /// @return     True if the buffer is external, False otherwise.
    ///
    [[nodiscard]] bool is_external() const { return m_is_external; }

    ///
    /// Checks whether the attribute is managing the lifetime of the underlying buffer. This is true
    /// for internal attributes (the buffer is allocated by the attribute itself), or for external
    /// attributes created by wrapping a SharedSpan object.
    ///
    /// @return     True if the buffer is managed, False otherwise.
    ///
    [[nodiscard]] bool is_managed() const { return !is_external() || m_owner; }

    ///
    /// Checks whether the attribute is external and pointing to a const buffer. If the attribute
    /// data is internally managed, this always returns false.
    ///
    /// @return     True if the data is read only, False otherwise.
    ///
    [[nodiscard]] bool is_read_only() const { return m_is_read_only; }

    ///
    /// Gets an entry for element @p i, at channel @p p. Note that due to internal checks, it is
    /// more efficient to first retrieve a span() of the whole buffer, then do access operations on
    /// this span inside a for loop.
    ///
    /// @param[in]  i     Element to access.
    /// @param[in]  c     Channel to access.
    ///
    /// @return     Value of the element attribute for the given channel.
    ///
    ValueType get(size_t i, size_t c) const;

    ///
    /// Gets a writable reference to the entry for element @p i, at channel @p p. Note that due to
    /// internal checks, it is more efficient to first retrieve a span() of the whole buffer, then
    /// do access operations on this span inside a for loop.
    ///
    /// @param[in]  i     Element to access.
    /// @param[in]  c     Channel to access.
    ///
    /// @return     Reference to the element attribute for the given channel.
    ///
    ValueType& ref(size_t i, size_t c);

    ///
    /// Gets an entry for a scalar element @p i. Note that due to internal checks, it is more
    /// efficient to first retrieve a span() of the whole buffer, then do access operations on this
    /// span inside a for loop.
    ///
    /// @param[in]  i     Element to access.
    ///
    /// @return     Value of the element attribute.
    ///
    ValueType get(size_t i) const;

    ///
    /// Gets a writable reference to a scalar element @p i, at channel @p p. Note that due to
    /// internal checks, it is more efficient to first retrieve a span() of the whole buffer, then
    /// do access operations on this span inside a for loop.
    ///
    /// @param[in]  i     Element to access.
    ///
    /// @return     Reference to the element attribute.
    ///
    ValueType& ref(size_t i);

    ///
    /// Returns a read-only view of the buffer spanning num elements x num channels. The actual
    /// buffer may have a larger capacity (e.g. used for padding).
    ///
    /// @return     A read-only view of the attribute buffer.
    ///
    lagrange::span<const ValueType> get_all() const;

    ///
    /// Returns a writable view of the buffer spanning num elements x num channels. The actual
    /// buffer may have a larger capacity (e.g. used for padding).
    ///
    /// @return     A writable view of the attribute buffer.
    ///
    lagrange::span<ValueType> ref_all();

    ///
    /// Returns a read-only view of the attribute values for the first elements in the buffer.
    ///
    /// @param[in]  num_elements  Number of elements to retrieve.
    ///
    /// @return     A read-only view of the attribute values.
    ///
    lagrange::span<const ValueType> get_first(size_t num_elements) const;

    ///
    /// Returns a writable view of the attribute values for the first elements in the buffer.
    ///
    /// @param[in]  num_elements  Number of elements to retrieve.
    ///
    /// @return     A writable view of the attribute values.
    ///
    lagrange::span<ValueType> ref_first(size_t num_elements);

    ///
    /// Returns a read-only view of the attribute values for the last elements in the buffer.
    ///
    /// @param[in]  num_elements  Number of elements to retrieve.
    ///
    /// @return     A read-only view of the attribute values.
    ///
    lagrange::span<const ValueType> get_last(size_t num_elements) const;

    ///
    /// Returns a writable view of the attribute values for the last elements in the buffer.
    ///
    /// @param[in]  num_elements  Number of elements to retrieve.
    ///
    /// @return     A writable view of the attribute values.
    ///
    lagrange::span<ValueType> ref_last(size_t num_elements);

    ///
    /// Returns a read-only view of the attribute values for the middle elements in the buffer.
    ///
    /// @param[in]  first_element  Index of the first element to retrieve.
    /// @param[in]  num_elements   Number of elements to retrieve.
    ///
    /// @return     A read-only view of the attribute values.
    ///
    lagrange::span<const ValueType> get_middle(size_t first_element, size_t num_elements) const;

    ///
    /// Returns a writable view of the attribute values for the middle elements in the buffer.
    ///
    /// @param[in]  first_element  Index of the first element to retrieve.
    /// @param[in]  num_elements   Number of elements to retrieve.
    ///
    /// @return     A writable view of the attribute values.
    ///
    lagrange::span<ValueType> ref_middle(size_t first_element, size_t num_elements);

    ///
    /// Returns a read-only view of the attribute values for the element in the buffer.
    ///
    /// @param[in]  element  Index of the element to retrieve.
    ///
    /// @return     A read-only view of the attribute values.
    ///
    lagrange::span<const ValueType> get_row(size_t element) const;

    ///
    /// Returns a writable view of the attribute values for the element in the buffer.
    ///
    /// @param[in]  element  Index of the first element to retrieve.
    ///
    /// @return     A writable view of the attribute values.
    ///
    lagrange::span<ValueType> ref_row(size_t element);

    /// @}
protected:
    /// @cond LA_INTERNAL_DOCS

    /// It's ok to be friend with attributes of different types.
    template <typename>
    friend class Attribute;

    ///
    /// Check whether the buffer can be grown to the new capacity. This check only applies to
    /// external buffers. Depending on the attribute growth policy, an internal copy of the data may
    /// be created.
    ///
    /// @param[in]  new_cap  The new buffer capacity.
    ///
    void growth_check(size_t new_cap);

    ///
    /// Checks whether the buffer can be written to. This check only applies to external buffers.
    /// Depending on the attribute growth policy, an internal copy of the data may be created.
    ///
    void write_check();

    ///
    /// Update the buffer pointers after a resize/growth operation.
    ///
    void update_views();

    ///
    /// Clear buffer pointers (intended to be used after a move).
    ///
    void clear_views();

    /// @endcond

protected:
    /// Internal buffer storing the data (when the attribute is not external).
    std::vector<ValueType> m_data;

    /// Optional aliased ptr to extend the lifetime of memory owner object of
    /// external buffer.
    std::shared_ptr<const ValueType> m_owner;

    /// Default values used to populate buffer when the attribute grows.
    ValueType m_default_value = ValueType(0);

    /// Writable pointer to the buffer storing the attribute data. For internal buffers, this will
    /// point to m_data.data(). For external buffers, it will be empty() if we are wrapping a
    /// read-only buffer, and point to the external data otherwise.
    lagrange::span<ValueType> m_view;

    /// Read-only pointer to the buffer storing the attribute data. For internal buffers, this will
    /// point to m_data.data(). For external buffers, it will point to the external data.
    lagrange::span<const ValueType> m_const_view;

    /// Growth policy for external buffers.
    AttributeGrowthPolicy m_growth_policy = AttributeGrowthPolicy::ErrorIfExternal;

    /// Shrink policy for external buffers.
    AttributeShrinkPolicy m_shrink_policy = AttributeShrinkPolicy::ErrorIfExternal;

    /// Policy for write access to read-only external buffers.
    AttributeWritePolicy m_write_policy = AttributeWritePolicy::ErrorIfReadOnly;

    /// Copy policy for external buffers.
    AttributeCopyPolicy m_copy_policy = AttributeCopyPolicy::CopyIfExternal;

    /// Cast policy when converting from this attribute.
    AttributeCastPolicy m_cast_policy = AttributeCastPolicy::RemapInvalidIndices;

    /// Flag to determine whether an attribute is using an external or internal buffer. We need this
    /// flag to distinguish if an empty buffer is internally managed or external.
    bool m_is_external = false;

    /// Flag to determine whether an external attribute is read-only or writable. We need this to
    /// distinguish whether a pointer to an empty buffer is read-only or "writable" (in the sense
    /// that a call to ref_all() should not produce an error).
    bool m_is_read_only = false;

    /// Number of elements associated with the attribute.
    size_t m_num_elements = 0;
};

/// @}

} // namespace lagrange
