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

#include "PyIndexedAttribute.h"

#include <lagrange/IndexedAttribute.h>
#include <lagrange/python/binding.h>

namespace lagrange::python {

void bind_indexed_attribute(nanobind::module_& m)
{
    namespace nb = nanobind;
    using namespace nb::literals;

    auto indexed_attr_class = nb::class_<PyIndexedAttribute>(
        m,
        "IndexedAttribute",
        R"(Indexed attribute data structure.

An indexed attribute stores values and indices separately, allowing for efficient
storage when multiple elements share the same values. This is commonly used for
UV coordinates, normals, or colors where the same value may be referenced by
multiple vertices, corners, or facets.)");

    indexed_attr_class.def_prop_ro(
        "element_type",
        [](PyIndexedAttribute& self) { return self->get_element_type(); },
        R"(Element type (i.e. Indexed).)");

    indexed_attr_class.def_prop_ro(
        "usage",
        [](PyIndexedAttribute& self) { return self->get_usage(); },
        "Usage type (Position, Normal, UV, Color, etc.).");

    indexed_attr_class.def_prop_ro(
        "num_channels",
        [](PyIndexedAttribute& self) { return self->get_num_channels(); },
        "Number of channels per element.");

    indexed_attr_class.def_prop_ro(
        "values",
        &PyIndexedAttribute::values,
        R"(The values array of the indexed attribute.

:returns: Attribute containing the unique values referenced by the indices)");
    indexed_attr_class.def_prop_ro(
        "indices",
        &PyIndexedAttribute::indices,
        R"(The indices array of the indexed attribute.

:returns: Attribute containing the indices that reference into the values array)");
}

} // namespace lagrange::python
