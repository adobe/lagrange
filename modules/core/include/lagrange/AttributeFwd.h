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

#include <cstdint>
#include <limits>
#include <array>

namespace lagrange {

/// @addtogroup group-surfacemesh-attr
/// @{

///
/// Type of element to which the attribute is attached.
///
enum AttributeElement : int {
    /// Per-vertex mesh attributes.
    Vertex = (1 << 0),

    /// Per-facet mesh attributes.
    Facet = (1 << 1),

    /// Per-edge mesh attributes.
    Edge = (1 << 2),

    /// Per-corner mesh attributes.
    Corner = (1 << 3),

    /// Values that are not attached to a specific element. Used by indexed attributes to store
    /// value buffers. It is the responsibility of the user to resize each value attribute as
    /// needed.
    Value = (1 << 4),

    /// Indexed mesh attributes.
    Indexed = (1 << 5),
};

///
/// Usage tag indicating how the attribute should behave under mesh transformations. This tag mostly
/// serves as a hint, and does not impact how the attribute is stored/loaded.
///
/// @todo       Add skinning weights + others?
///
enum class AttributeUsage : uint16_t {
    Vector = (1 << 0), ///< Mesh attribute can have any number of channels (including 1 channel).
    Scalar = (1 << 1), ///< Mesh attribute must have exactly 1 channel.
    Position = (1 << 2), ///< Mesh attribute must have exactly dim channels.
    Normal = (1 << 3), ///< Mesh attribute can have dim or dim + 1 channels.
    Tangent = (1 << 4), ///< Mesh attribute can have dim or dim + 1 channels.
    Bitangent = (1 << 5), ///< Mesh attribute can have dim or dim + 1 channels.
    Color = (1 << 6), ///< Mesh attribute can have 1, 2, 3 or 4 channels.
    UV = (1 << 7), ///< Mesh attribute must have exactly 2 channels.
    VertexIndex = (1 << 8), ///< Single channel integer attribute indexing a mesh vertex.
    FacetIndex = (1 << 9), ///< Single channel integer attribute indexing a mesh facet.
    CornerIndex = (1 << 10), ///< Single channel integer attribute indexing a mesh corner.
    EdgeIndex = (1 << 11), ///< Single channel integer attribute indexing a mesh edge.
    String = (1 << 12), ///< Mesh attribute is a metadata string (stored as a uint8_t buffer).
};

/// Identified to be used to access an attribute. Attribute names are mapped to a unique identified
/// when created. These unique identifiers can be used to more efficiently access the data (rather
/// than rehashing the string every time).
using AttributeId = uint32_t;

/// Invalid attribute id.
inline constexpr AttributeId invalid_attribute_id()
{
    return std::numeric_limits<AttributeId>::max();
};

///
/// Policy for attribute creation with reserved attribute names. By default, attribute names
/// starting with "$" are reserved for internal use. Creating a reserved attribute name requires an
/// explicit policy flag to be passed to the appropriate function.
///
enum class AttributeCreatePolicy : uint8_t {
    ErrorIfReserved, ///< Default deletion policy, throw an exception if attribute name is reserved.
    Force, ///< Force creation of reserved attribute names.
};

///
/// Policy for growing external attribute buffers. If we need to add elements to an external
/// attribute buffer, and we have reached the capacity of the provided span, we can either throw an
/// exception (default behavior), or warn and create an internal copy of the buffer data.
///
enum class AttributeGrowthPolicy : uint8_t {
    /// Throws an exception when trying to grow an external buffer (even if the new size is still
    /// within the buffer capacity).
    ErrorIfExternal,

    /// Allow attribute growth as long as it remains within the capacity of the external buffer.
    /// Will throw an error if a reallocation is needed.
    AllowWithinCapacity,

    /// Logs a warning and copy the buffer data if it grows beyond the buffer capacity.
    WarnAndCopy,

    /// Silently copy the buffer data if it grows beyond the buffer capacity.
    SilentCopy,
};

///
/// Policy for shrinking external attribute buffers. This policy controls what happens when calling
/// `shrink_to_fit()` to save memory. Shrinkage can happen either when mesh elements have been
/// removed by another operation, or if the attribute has been created with additional capacity.
///
enum class AttributeShrinkPolicy : uint8_t {
    /// Throws an exception when trying to shrink an external buffer (even if the new size is still
    /// within the buffer capacity). This is the default policy.
    ErrorIfExternal,

    /// Ignore external buffers when trying to shrink an attribute.
    IgnoreIfExternal,

    /// Logs a warning and creates an internal copy of the buffer data when shrinking below
    /// capacity.
    WarnAndCopy,

    /// Silently copy the buffer data when shrinking below capacity.
    SilentCopy,
};

///
/// Policy for attempting to write to read-only external buffers. By default any write operation on
/// a read-only external buffer will throw an exception. Alternatively, we can issue a warning a
/// perform a copy of the buffer data.
///
enum class AttributeWritePolicy : uint8_t {
    ErrorIfReadOnly, ///< Throws an exception when trying to write to a read-only buffer.
    WarnAndCopy, ///< Logs a warning and copy the buffer data.
    SilentCopy, ///< Silently copy the buffer data.
};

///
/// Policy for exporting attributes that are views onto external buffers. By default, a copy is
/// created, meaning we can use the resulting pointer to manage the lifetime of the allocated
/// data.
///
enum class AttributeExportPolicy : uint8_t {
    CopyIfExternal, ///< Copy the buffer during export if the attribute points to an external buffer.
    CopyIfUnmanaged, ///< Copy the buffer during export if the attribute points to an unmanaged external buffer.
    KeepExternalPtr, ///< Keep the raw pointer to the external buffer data. Use with caution.
    ErrorIfExternal, ///< Throw an exception if the attribute points to an external buffer.
};

///
/// Policy for copying attribute that are views onto external buffers.  By
/// default, a copy is created, meaning we can use the copied attribute to
/// manage the lifetime of the allocated data.
///
enum class AttributeCopyPolicy : uint8_t {
    CopyIfExternal, ///< Copy the buffer during copy if the attribute points to an external buffer.
    KeepExternalPtr, ///< Keep the raw pointer to the external buffer data. Use with caution.
    ErrorIfExternal, ///< Throw an exception if the attribute points to an external buffer.
};

///
/// Policy for attribute deletion of reserved attribute names. By default, attribute names starting
/// with "$" are reserved for internal use. Deleting a reserved attribute name requires an explicit
/// policy flag to be passed to the appropriate function.
///
enum class AttributeDeletePolicy : uint8_t {
    ErrorIfReserved, ///< Default deletion policy, throw an exception if attribute name is reserved.
    Force, ///< Force deletion of reserved attribute names.
};

///
/// Policy for remapping invalid values when casting to a different value type. By default, invalid
/// values are remapped from source type to target type if the AttributeUsage represents element
/// indices.
///
enum class AttributeCastPolicy : uint8_t {
    RemapInvalidIndices, ///< Map invalue values only if the AttributeUsage represents indices.
    RemapInvalidAlways, ///< Always remap invalid values from source type to target type, regardless of AttributeUsage.
    DoNotRemapInvalid, ///< Do not remap invalid values. They are simply static_cast<> to the target type.
};

///
/// Base handle for attributes. This is a common base class to allow for type erasure.
///
class AttributeBase;

/// Derived attribute class that stores the actual information.
template <typename ValueType>
class Attribute;

/// Derived attribute class that stores the actual information.
template <typename ValueType, typename Index>
class IndexedAttribute;

/// @}

} // namespace lagrange
