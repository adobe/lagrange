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

#include "PyAttribute.h"

#include <lagrange/Attribute.h>
#include <lagrange/Logger.h>
#include <lagrange/internal/string_from_scalar.h>
#include <lagrange/python/tensor_utils.h>
#include <lagrange/utils/Error.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/invalid.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <nanobind/nanobind.h>
#include <lagrange/utils/warnon.h>
// clang-format on

namespace lagrange::python {

void bind_attribute(nanobind::module_& m)
{
    namespace nb = nanobind;
    using namespace nb::literals;

    auto attr_class = nb::class_<PyAttribute>(m, "Attribute");
    attr_class.def_prop_ro("element_type", [](PyAttribute& self) {
        return self->get_element_type();
    });
    attr_class.def_prop_ro("usage", [](PyAttribute& self) { return self->get_usage(); });
    attr_class.def_prop_ro("num_channels", [](PyAttribute& self) {
        return self->get_num_channels();
    });

    attr_class.def_prop_rw(
        "default_value",
        [](PyAttribute& self) {
            return self.process([](auto& attr) { return nb::cast(attr.get_default_value()); });
        },
        [](PyAttribute& self, double val) {
            self.process([&](auto& attr) {
                using ValueType = typename std::decay_t<decltype(attr)>::ValueType;
                attr.set_default_value(static_cast<ValueType>(val));
            });
        });
    attr_class.def_prop_rw(
        "growth_policy",
        [](PyAttribute& self) {
            return self.process([](auto& attr) { return attr.get_growth_policy(); });
        },
        [](PyAttribute& self, AttributeGrowthPolicy policy) {
            self.process([&](auto& attr) { attr.set_growth_policy(policy); });
        });
    attr_class.def_prop_rw(
        "write_policy",
        [](PyAttribute& self) {
            return self.process([](auto& attr) { return attr.get_write_policy(); });
        },
        [](PyAttribute& self, AttributeWritePolicy policy) {
            self.process([&](auto& attr) { attr.set_write_policy(policy); });
        });
    attr_class.def_prop_rw(
        "copy_policy",
        [](PyAttribute& self) {
            return self.process([](auto& attr) { return attr.get_copy_policy(); });
        },
        [](PyAttribute& self, AttributeCopyPolicy policy) {
            self.process([&](auto& attr) { attr.set_copy_policy(policy); });
        });
    attr_class.def("create_internal_copy", [](PyAttribute& self) {
        self.process([](auto& attr) { attr.create_internal_copy(); });
    });
    attr_class.def("clear", [](PyAttribute& self) {
        self.process([](auto& attr) { attr.clear(); });
    });
    attr_class.def("reserve_entries", [](PyAttribute& self, size_t s) {
        self.process([=](auto& attr) { attr.reserve_entries(s); });
    });
    attr_class.def("insert_elements", [](PyAttribute& self, size_t num_elements) {
        self.process([=](auto& attr) { attr.insert_elements(num_elements); });
    });
    attr_class.def(
        "insert_elements",
        [](PyAttribute& self, GenericTensor tensor) {
            auto insert_elements = [&](auto& attr) {
                using ValueType = typename std::decay_t<decltype(attr)>::ValueType;
                if (tensor.dtype() != nb::dtype<ValueType>()) {
                    throw nb::type_error(fmt::format(
                        "Tensor has a unexpected dtype.  Expecting {}.",
                        internal::string_from_scalar<ValueType>()).c_str());
                }
                Tensor<ValueType> local_tensor(tensor.handle());
                auto [data, shape, stride] = tensor_to_span(local_tensor);
                la_runtime_assert(is_dense(shape, stride));
                attr.insert_elements(data);
            };
            self.process(insert_elements);
        },
        "tensor"_a);
    attr_class.def("empty", [](PyAttribute& self) {
        return self.process([](auto& attr) { return attr.empty(); });
    });
    attr_class.def_prop_ro("num_elements", [](PyAttribute& self) {
        return self.process([](auto& attr) { return attr.get_num_elements(); });
    });
    attr_class.def_prop_ro("external", [](PyAttribute& self) {
        return self.process([](auto& attr) { return attr.is_external(); });
    });
    attr_class.def_prop_ro("readonly", [](PyAttribute& self) {
        return self.process([](auto& attr) { return attr.is_read_only(); });
    });
    attr_class.def_prop_rw(
        "data",
        [](PyAttribute& self) {
            return self.process([&](auto& attr) {
                auto tensor = attribute_to_tensor(attr, nb::find(&self));
                return nb::cast(tensor, nb::rv_policy::reference_internal);
            });
        },
        [](PyAttribute& self, GenericTensor tensor) {
            auto wrap_tensor = [&](auto& attr) {
                using ValueType = typename std::decay_t<decltype(attr)>::ValueType;
                if (tensor.dtype() != nb::dtype<ValueType>()) {
                    throw nb::type_error(fmt::format(
                        "Tensor has a unexpected dtype.  Expecting {}.",
                        internal::string_from_scalar<ValueType>()).c_str());
                }
                Tensor<ValueType> local_tensor(tensor.handle());
                auto [data, shape, stride] = tensor_to_span(local_tensor);
                la_runtime_assert(is_dense(shape, stride));
                la_runtime_assert(shape.size() == 1 ? 1 == attr.get_num_channels() : true);
                la_runtime_assert(shape.size() == 2 ? shape[1] == attr.get_num_channels() : true);
                const size_t num_elements = shape[0];

                auto owner = std::make_shared<nb::object>(nb::find(tensor));
                attr.wrap(make_shared_span(owner, data.data(), data.size()), num_elements);
            };
            self.process(wrap_tensor);
        });
}

} // namespace lagrange::python
