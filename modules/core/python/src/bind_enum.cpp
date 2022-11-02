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
    nb::enum_<AttributeElement>(m, "AttributeElement")
        .value("Vertex", Vertex)
        .value("Facet", Facet)
        .value("Corner", Corner)
        .value("Value", Value)
        .value("Indexed", Indexed);

    nb::enum_<AttributeUsage>(m, "AttributeUsage")
        .value("Vector", AttributeUsage::Vector)
        .value("Scalar", AttributeUsage::Scalar)
        .value("Normal", AttributeUsage::Normal)
        .value("Tangent", AttributeUsage::Tangent)
        .value("Bitangent", AttributeUsage::Bitangent)
        .value("Color", AttributeUsage::Color)
        .value("UV", AttributeUsage::UV)
        .value("VertexIndex", AttributeUsage::VertexIndex)
        .value("FacetIndex", AttributeUsage::FacetIndex)
        .value("CornerIndex", AttributeUsage::CornerIndex)
        .value("EdgeIndex", AttributeUsage::EdgeIndex);

    nb::enum_<AttributeCreatePolicy>(m, "AttributeCreatePolicy")
        .value("ErrorIfReserved", AttributeCreatePolicy::ErrorIfReserved)
        .value("Force", AttributeCreatePolicy::Force);

    nb::enum_<AttributeGrowthPolicy>(m, "AttributeGrowthPolicy")
        .value("ErrorIfExtenal", AttributeGrowthPolicy::ErrorIfExternal)
        .value("AllowWithinCapacity", AttributeGrowthPolicy::AllowWithinCapacity)
        .value("WarnAndCopy", AttributeGrowthPolicy::WarnAndCopy)
        .value("SilentCopy", AttributeGrowthPolicy::SilentCopy);

    nb::enum_<AttributeWritePolicy>(m, "AttributeWritePolicy")
        .value("ErrorIfReadOnly", AttributeWritePolicy::ErrorIfReadOnly)
        .value("WarnAndCopy", AttributeWritePolicy::WarnAndCopy)
        .value("SilentCopy", AttributeWritePolicy::SilentCopy);

    nb::enum_<AttributeExportPolicy>(m, "AttributeExportPolicy")
        .value("CopyIfExternal", AttributeExportPolicy::CopyIfExternal)
        .value("KeepExternalPtr", AttributeExportPolicy::KeepExternalPtr)
        .value("ErrorIfExternal", AttributeExportPolicy::ErrorIfExternal);

    nb::enum_<AttributeCopyPolicy>(m, "AttributeCopyPolicy")
        .value("CopyIfExternal", AttributeCopyPolicy::CopyIfExternal)
        .value("KeepExternalPtr", AttributeCopyPolicy::KeepExternalPtr)
        .value("ErrorIfExternal", AttributeCopyPolicy::ErrorIfExternal);

    nb::enum_<AttributeDeletePolicy>(m, "AttributeDeletePolicy")
        .value("ErrorIfReserved", AttributeDeletePolicy::ErrorIfReserved)
        .value("Force", AttributeDeletePolicy::Force);
}

} // namespace lagrange::python
