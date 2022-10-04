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

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <nanobind/nanobind.h>
#include <lagrange/utils/warnon.h>
// clang-format on

namespace lagrange::python {

void bind_indexed_attribute(nanobind::module_& m)
{
    namespace nb = nanobind;
    using namespace nb::literals;

    auto indexed_attr_class = nb::class_<PyIndexedAttribute>(m, "IndexedAttribute");

    indexed_attr_class.def_property_readonly("element_type", [](PyIndexedAttribute& self) {
        return self->get_element_type();
    });
    indexed_attr_class.def_property_readonly("usage", [](PyIndexedAttribute& self) {
        return self->get_usage();
    });
    indexed_attr_class.def_property_readonly("num_channels", [](PyIndexedAttribute& self) {
        return self->get_num_channels();
    });

    indexed_attr_class.def_property_readonly("values", &PyIndexedAttribute::values);
    indexed_attr_class.def_property_readonly("indices", &PyIndexedAttribute::indices);
}

} // namespace lagrange::python
