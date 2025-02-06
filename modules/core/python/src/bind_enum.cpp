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

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <nanobind/nanobind.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <lagrange/AttributeFwd.h>

namespace lagrange::python {

namespace nb = nanobind;
void bind_enum(nb::module_& m)
{
    nb::enum_<AttributeElement>(
        m,
        "AttributeElement",
        nb::is_arithmetic(),
        "Attribute element type")
        .value("Vertex", Vertex, "Per-vertex attribute")
        .value("Facet", Facet, "Per-facet attribute")
        .value("Edge", Edge, "Per-edge attribute")
        .value("Corner", Corner, "Per-corner attribute")
        .value("Value", Value, "Value attribute not attached to any mesh elements")
        .value("Indexed", Indexed, "Indexed attribute");

    nb::enum_<AttributeUsage>(m, "AttributeUsage", "Attribute usage type")
        .value(
            "Vector",
            AttributeUsage::Vector,
            "Vector attribute that may have any number of channels")
        .value("Scalar", AttributeUsage::Scalar, "Scalar attribute that has exactly 1 channel")
        .value(
            "Position",
            AttributeUsage::Position,
            "Position attribute must have exactly dim channels")
        .value("Normal", AttributeUsage::Normal, "Normal attribute must have exactly dim channels")
        .value(
            "Tangent",
            AttributeUsage::Tangent,
            "Tangent attribute must have exactly dim channels")
        .value(
            "Bitangent",
            AttributeUsage::Bitangent,
            "Bitangent attribute must have exactly dim channels")
        .value("Color", AttributeUsage::Color, "Color attribute may have 1, 2, 3 or 4 channels")
        .value("UV", AttributeUsage::UV, "UV attribute has exactly 2 channels")
        .value(
            "VertexIndex",
            AttributeUsage::VertexIndex,
            "Single channel integer attribute indexing mesh vertices")
        .value(
            "FacetIndex",
            AttributeUsage::FacetIndex,
            "Single channel integer attribute indexing mesh facets")
        .value(
            "CornerIndex",
            AttributeUsage::CornerIndex,
            "Single channel integer attribute indexing mesh corners")
        .value(
            "EdgeIndex",
            AttributeUsage::EdgeIndex,
            "Single channel integer attribute indexing mesh edges");

    nb::enum_<AttributeCreatePolicy>(m, "AttributeCreatePolicy", "Attribute creation policy")
        .value(
            "ErrorIfReserved",
            AttributeCreatePolicy::ErrorIfReserved,
            "Default policy, error if attribute name is reserved")
        .value(
            "Force",
            AttributeCreatePolicy::Force,
            "Force create attribute even if name is reserved");

    nb::enum_<AttributeGrowthPolicy>(
        m,
        "AttributeGrowthPolicy",
        "Attribute growth policy (for external buffers)")
        .value(
            "ErrorIfExtenal",
            AttributeGrowthPolicy::ErrorIfExternal,
            "Disallow growth if external buffer is used (default)")
        .value(
            "AllowWithinCapacity",
            AttributeGrowthPolicy::AllowWithinCapacity,
            "Allow growth as long as it is within the capacity of the external buffer")
        .value(
            "WarnAndCopy",
            AttributeGrowthPolicy::WarnAndCopy,
            "Warn and copy attribute to internal buffer when growth exceeds external buffer "
            "capacity")
        .value(
            "SilentCopy",
            AttributeGrowthPolicy::SilentCopy,
            "Silently copy attribute to internal buffer when growth exceeds external buffer "
            "capacity");

    nb::enum_<AttributeShrinkPolicy>(
        m,
        "AttributeShrinkPolicy",
        "Attribute shrink policy (for external buffers)")
        .value(
            "ErrorIfExternal",
            AttributeShrinkPolicy::ErrorIfExternal,
            "Disallow shrink if external buffer is used (default)")
        .value(
            "IgnoreIfExternal",
            AttributeShrinkPolicy::IgnoreIfExternal,
            "Ignore shrink if external buffer is used")
        .value(
            "WarnAndCopy",
            AttributeShrinkPolicy::WarnAndCopy,
            "Warn and copy attribute to internal buffer when shrinking below external buffer "
            "capacity")
        .value(
            "SilentCopy",
            AttributeShrinkPolicy::SilentCopy,
            "Silently copy attribute to internal buffer when shrinking below external buffer "
            "capacity");

    nb::enum_<AttributeWritePolicy>(
        m,
        "AttributeWritePolicy",
        "Policy for attempting to write to read-only external buffer")
        .value(
            "ErrorIfReadOnly",
            AttributeWritePolicy::ErrorIfReadOnly,
            "Disallow writing to read-only external buffer (default)")
        .value(
            "WarnAndCopy",
            AttributeWritePolicy::WarnAndCopy,
            "Warn and copy attribute to internal buffer when writing to read-only external buffer")
        .value(
            "SilentCopy",
            AttributeWritePolicy::SilentCopy,
            "Silently copy attribute to internal buffer when writing to read-only external buffer");

    nb::enum_<AttributeExportPolicy>(
        m,
        "AttributeExportPolicy",
        "Policy for exporting attribute that is a view of an external buffer")
        .value(
            "CopyIfExternal",
            AttributeExportPolicy::CopyIfExternal,
            "Copy attribute to internal buffer")
        .value(
            "CopyIfUnmanaged",
            AttributeExportPolicy::CopyIfUnmanaged,
            "Copy attribute to internal buffer if the external buffer is unmanaged (i.e. not "
            "reference counted)")
        .value(
            "KeepExternalPtr",
            AttributeExportPolicy::KeepExternalPtr,
            "Keep external buffer pointer")
        .value(
            "ErrorIfExternal",
            AttributeExportPolicy::ErrorIfExternal,
            "Error if external buffer is used");

    nb::enum_<AttributeCopyPolicy>(
        m,
        "AttributeCopyPolicy",
        "Policy for copying attribute that is a view of an external buffer")
        .value(
            "CopyIfExternal",
            AttributeCopyPolicy::CopyIfExternal,
            "Copy attribute to internal buffer")
        .value(
            "KeepExternalPtr",
            AttributeCopyPolicy::KeepExternalPtr,
            "Keep external buffer pointer")
        .value(
            "ErrorIfExternal",
            AttributeCopyPolicy::ErrorIfExternal,
            "Error if external buffer is used");

    nb::enum_<AttributeCastPolicy>(
        m,
        "AttributeCastPolicy",
        "Policy for remapping invalid values when casting to a different value type")
        .value(
            "RemapInvalidIndices",
            AttributeCastPolicy::RemapInvalidIndices,
            "Map invalue values only if the AttributeUsage represents indices")
        .value(
            "RemapInvalidAlways",
            AttributeCastPolicy::RemapInvalidAlways,
            "Always remap invalid values from source type to target type, regardless of AttributeUsage")
        .value(
            "DoNotRemapInvalid",
            AttributeCastPolicy::DoNotRemapInvalid,
            "Do not remap invalid values. They are simply static_cast<> to the target type");

    nb::enum_<AttributeDeletePolicy>(
        m,
        "AttributeDeletePolicy",
        "Policy for deleting attributes with reserved names")
        .value(
            "ErrorIfReserved",
            AttributeDeletePolicy::ErrorIfReserved,
            "Disallow deletion (default)")
        .value("Force", AttributeDeletePolicy::Force, "Force delete attribute");
}

} // namespace lagrange::python
