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
#include <lagrange/AttributeValueType.h>
#include <lagrange/Logger.h>
#include <lagrange/internal/string_from_scalar.h>
#include <lagrange/python/binding.h>
#include <lagrange/python/tensor_utils.h>
#include <lagrange/utils/Error.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/fmt/format.h>
#include <lagrange/utils/invalid.h>

#include <memory>
#include <vector>

namespace lagrange::python {


void bind_attribute(nanobind::module_& m)
{
    namespace nb = nanobind;
    using namespace nb::literals;

    auto attr_class = nb::class_<PyAttribute>(
        m,
        "Attribute",
        "Attribute data associated with mesh elements (vertices, facets, corners, edges).");
    attr_class.def_prop_ro(
        "element_type",
        [](PyAttribute& self) { return self->get_element_type(); },
        "Element type (Vertex, Facet, Corner, Edge, Value).");
    attr_class.def_prop_ro(
        "usage",
        [](PyAttribute& self) { return self->get_usage(); },
        "Usage type (Position, Normal, UV, Color, etc.).");
    attr_class.def_prop_ro(
        "num_channels",
        [](PyAttribute& self) { return self->get_num_channels(); },
        "Number of channels per element.");

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
        },
        "Default value for new elements.");
    attr_class.def_prop_rw(
        "growth_policy",
        [](PyAttribute& self) {
            return self.process([](auto& attr) { return attr.get_growth_policy(); });
        },
        [](PyAttribute& self, AttributeGrowthPolicy policy) {
            self.process([&](auto& attr) { attr.set_growth_policy(policy); });
        },
        "Policy for growing the attribute when elements are added.");
    attr_class.def_prop_rw(
        "shrink_policy",
        [](PyAttribute& self) {
            return self.process([](auto& attr) { return attr.get_shrink_policy(); });
        },
        [](PyAttribute& self, AttributeShrinkPolicy policy) {
            self.process([&](auto& attr) { attr.set_shrink_policy(policy); });
        },
        "Policy for shrinking the attribute when elements are removed.");
    attr_class.def_prop_rw(
        "write_policy",
        [](PyAttribute& self) {
            return self.process([](auto& attr) { return attr.get_write_policy(); });
        },
        [](PyAttribute& self, AttributeWritePolicy policy) {
            self.process([&](auto& attr) { attr.set_write_policy(policy); });
        },
        "Policy for write operations on the attribute.");
    attr_class.def_prop_rw(
        "copy_policy",
        [](PyAttribute& self) {
            return self.process([](auto& attr) { return attr.get_copy_policy(); });
        },
        [](PyAttribute& self, AttributeCopyPolicy policy) {
            self.process([&](auto& attr) { attr.set_copy_policy(policy); });
        },
        "Policy for copying the attribute.");
    attr_class.def_prop_rw(
        "cast_policy",
        [](PyAttribute& self) {
            return self.process([](auto& attr) { return attr.get_cast_policy(); });
        },
        [](PyAttribute& self, AttributeCastPolicy policy) {
            self.process([&](auto& attr) { attr.set_cast_policy(policy); });
        },
        "Policy for casting the attribute to different types.");
    attr_class.def(
        "create_internal_copy",
        [](PyAttribute& self) { self.process([](auto& attr) { attr.create_internal_copy(); }); },
        "Create an internal copy if the attribute wraps external data.");
    attr_class.def(
        "ensure_shared_storage",
        [](PyAttribute& self) { self.process([](auto& attr) { attr.ensure_shared_storage(); }); },
        "Transfer internal storage into shared ownership without copying values. Managed external "
        "storage is preserved; unmanaged external storage is rejected. Only when "
        "promoting internal storage, ErrorIfExternal growth and shrink policies become "
        "SilentCopy; all other policies are preserved. Policies set afterward apply normally.");
    attr_class.def(
        "shrink_to_fit",
        [](PyAttribute& self) { self.process([](auto& attr) { attr.shrink_to_fit(); }); },
        "Shrink allocation using the selected shrink policy; retained arrays keep their previous "
        "allocation.");
    attr_class.def(
        "clear",
        [](PyAttribute& self) { self.process([](auto& attr) { attr.clear(); }); },
        "Remove all elements from the attribute.");
    attr_class.def(
        "reserve_entries",
        [](PyAttribute& self, size_t s) {
            self.process([=](auto& attr) { attr.reserve_entries(s); });
        },
        "num_entries"_a,
        R"(Reserve enough memory for `num_entries` entries.

:param num_entries: Number of entries to reserve. It does not need to be a multiple of `num_channels`.)");
    attr_class.def(
        "insert_elements",
        [](PyAttribute& self, size_t num_elements) {
            self.process([=](auto& attr) { attr.insert_elements(num_elements); });
        },
        "num_elements"_a,
        R"(Insert new elements with default value to the attribute.

:param num_elements: Number of elements to insert.)");
    attr_class.def(
        "insert_elements",
        [](PyAttribute& self, nb::object value) {
            auto insert_elements = [&](auto& attr) {
                using ValueType = typename std::decay_t<decltype(attr)>::ValueType;
                GenericTensor tensor;
                std::vector<ValueType> buffer;
                if (nb::try_cast(value, tensor)) {
                    if (tensor.dtype() != nb::dtype<ValueType>()) {
                        throw nb::type_error(
                            lagrange::format(
                                "Tensor has a unexpected dtype.  Expecting {}.",
                                internal::string_from_scalar<ValueType>())
                                .c_str());
                    }
                    Tensor<ValueType> local_tensor(tensor.handle());
                    auto [data, shape, stride] = tensor_to_span(local_tensor);
                    la_runtime_assert(is_dense(shape, stride));
                    attr.insert_elements(data);
                } else if (nb::try_cast(value, buffer)) {
                    attr.insert_elements({buffer.data(), buffer.size()});
                } else {
                    throw nb::type_error("Argument `value` must be either list or np.ndarray.");
                }
            };
            self.process(insert_elements);
        },
        "tensor"_a,
        R"(Insert new elements to the attribute.

:param tensor: A tensor with shape (num_elements, num_channels) or (num_elements,).)");
    attr_class.def(
        "empty",
        [](PyAttribute& self) { return self.process([](auto& attr) { return attr.empty(); }); },
        "Check if the attribute has no elements.");
    attr_class.def_prop_ro(
        "num_elements",
        [](PyAttribute& self) {
            return self.process([](auto& attr) { return attr.get_num_elements(); });
        },
        "Number of elements in the attribute.");
    attr_class.def_prop_ro(
        "external",
        [](PyAttribute& self) {
            return self.process([](auto& attr) { return attr.is_external(); });
        },
        "Check if storage is external to the attribute's embedded vector; NumPy export promotes "
        "internal storage to managed external storage.");
    attr_class.def_prop_ro(
        "readonly",
        [](PyAttribute& self) {
            return self.process([](auto& attr) { return attr.is_read_only(); });
        },
        "Check if the attribute is read-only.");
    attr_class.def_prop_rw(
        "data",
        [](PyAttribute& self) {
            return self.process([](auto& attr) { return attribute_to_tensor(attr); });
        },
        [](PyAttribute& self, nb::object value) {
            auto wrap_tensor = [&](auto& attr) {
                using ValueType = typename std::decay_t<decltype(attr)>::ValueType;
                GenericTensor tensor;
                std::vector<ValueType> buffer;
                if (nb::try_cast(value, tensor)) {
                    if (tensor.dtype() != nb::dtype<ValueType>()) {
                        throw nb::type_error(
                            lagrange::format(
                                "Tensor has a unexpected dtype.  Expecting {}.",
                                internal::string_from_scalar<ValueType>())
                                .c_str());
                    }
                    Tensor<ValueType> local_tensor(tensor.handle());
                    auto [data, shape, stride] = tensor_to_span(local_tensor);
                    la_runtime_assert(is_dense(shape, stride));
                    la_runtime_assert(shape.size() == 1 ? 1 == attr.get_num_channels() : true);
                    la_runtime_assert(
                        shape.size() == 2 ? shape[1] == attr.get_num_channels() : true);
                    const size_t num_elements = shape[0];

                    const auto current_data = attr.get_all();
                    if (data.data() == current_data.data() && data.size() == current_data.size()) {
                        return;
                    }

                    // Incoming arrays own allocations, never the mutable attribute itself.
                    auto owner = std::make_shared<nb::object>(nb::find(tensor));
                    attr.wrap(make_shared_span(owner, data.data(), data.size()), num_elements);
                    set_python_attribute_policies(attr);
                } else if (nb::try_cast(value, buffer)) {
                    attr.clear();
                    attr.insert_elements({buffer.data(), buffer.size()});
                } else {
                    throw nb::type_error("Attribute.data must be either list or np.ndarray.");
                }
            };
            self.process(wrap_tensor);
        },
        nb::for_getter(nb::rv_policy::reference),
        nb::for_getter(nb::sig("def data(self, /) -> numpy.typing.NDArray")),
        R"(Raw data as a NumPy array.

The first export transfers internal storage into shared ownership without copying existing values.
Later exports share that allocation. Value-only writes remain shared; rebinding, growth,
or allocation shrink may detach, while previously returned arrays keep their allocation alive.
Read-only attributes produce read-only arrays. Only internal-storage promotion changes
ErrorIfExternal growth and shrink policies to SilentCopy; existing external storage and all
other policies are preserved.
Assigning a NumPy array rebinds the attribute to that buffer with `SilentCopy` growth and shrink.)");
    attr_class.def_prop_ro(
        "dtype",
        [](PyAttribute& self) -> std::optional<nb::type_object> {
            auto np = nb::module_::import_("numpy");
            switch (self.ptr()->get_value_type()) {
            case AttributeValueType::e_int8_t: return np.attr("int8");
            case AttributeValueType::e_int16_t: return np.attr("int16");
            case AttributeValueType::e_int32_t: return np.attr("int32");
            case AttributeValueType::e_int64_t: return np.attr("int64");
            case AttributeValueType::e_uint8_t: return np.attr("uint8");
            case AttributeValueType::e_uint16_t: return np.attr("uint16");
            case AttributeValueType::e_uint32_t: return np.attr("uint32");
            case AttributeValueType::e_uint64_t: return np.attr("uint64");
            case AttributeValueType::e_float: return np.attr("float32");
            case AttributeValueType::e_double: return np.attr("float64");
            default: logger().warn("Attribute has an unknown dtype."); return std::nullopt;
            }
        },
        "NumPy dtype of the attribute values.");
}

} // namespace lagrange::python
