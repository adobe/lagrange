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
/// @todo       Add Tangent, Bitangent + others?
///
enum class AttributeUsage {
    Vector, ///< Mesh attribute can have any number of channels (including 1 channel).
    Scalar, ///< Mesh attribute must have exactly 1 channel.
    Normal, ///< Mesh attribute can have 2, 3 or 4 channels.
    Color, ///< Mesh attribute can have 1, 2, 3 or 4 channels.
    UV, ///< Mesh attribute must have exactly 2 channels.
    VertexIndex, ///< Single channel integer attribute indexing a mesh vertex.
    FacetIndex, ///< Single channel integer attribute indexing a mesh facet.
    CornerIndex, ///< Single channel integer attribute indexing a mesh corner.
    EdgeIndex, ///< Single channel integer attribute indexing a mesh edge.
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
enum class AttributeCreatePolicy {
    ErrorIfReserved, ///< Default deletion policy, throw an exception if attribute name is reserved.
    Force, ///< Force creation of reserved attribute names.
};

///
/// Policy for growing external attribute buffers. If we need to add elements to an external
/// attribute buffer, and we have reached the capacity of the provided span, we can either throw an
/// exception (default behavior), or warn and create an internal copy of the buffer data.
///
enum class AttributeGrowthPolicy {
    ErrorIfExternal, ///< Throws an exception when trying to grow an external buffer (even if the new size is
    ///< still within the buffer capacity).
    AllowWithinCapacity, ///< Allow attribute growth as long as it remains within the capacity of the external
    ///< buffer. Will throw an error if a reallocation is needed.
    WarnAndCopy, ///< Logs a warning and copy the buffer data if it grows beyond the buffer capacity.
    SilentCopy, ///< Silently copy the buffer data if it grows beyond the buffer capacity.
};

///
/// Policy for attempting to write to read-only external buffers. By default any write operation on
/// a read-only external buffer will throw an exception. Alternatively, we can issue a warning a
/// perform a copy of the buffer data.
///
enum class AttributeWritePolicy {
    ErrorIfReadOnly, ///< Throws an exception when trying to write to a read-only buffer.
    WarnAndCopy, ///< Logs a warning and copy the buffer data.
    SilentCopy, ///< Silently copy the buffer data.
};

///
/// Policy for exporting attributes that are views onto external buffers. By default, a copy is
/// created, meaning we can use the resulting pointer to manage the lifetime of the allocated
/// data.
///
enum class AttributeExportPolicy {
    CopyIfExternal, ///< Copy the buffer during export if the attribute points to an external buffer.
    KeepExternalPtr, ///< Keep the raw pointer to the external buffer data. Use with caution.
    ErrorIfExternal, ///< Throw an exception if the attribute points to an external buffer.
};

///
/// Policy for copying attribute that are views onto external buffers.  By
/// default, a copy is created, meaning we can use the copied attribute to
/// manage the lifetime of the allocated data.
///
enum class AttributeCopyPolicy {
    CopyIfExternal, ///< Copy the buffer during copy if the attribute points to an external buffer.
    KeepExternalPtr, ///< Keep the raw pointer to the external buffer data. Use with caution.
    ErrorIfExternal, ///< Throw an exception if the attribute points to an external buffer.
};

///
/// Policy for attribute deletion of reserved attribute names. By default, attribute names starting
/// with "$" are reserved for internal use. Deleting a reserved attribute name requires an explicit
/// policy flag to be passed to the appropriate function.
///
enum class AttributeDeletePolicy {
    ErrorIfReserved, ///< Default deletion policy, throw an exception if attribute name is reserved.
    Force, ///< Force deletion of reserved attribute names.
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
