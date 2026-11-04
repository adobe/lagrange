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
#include <lagrange/python/binding.h>

#include <lagrange/python/tensor_utils.h>

#include <lagrange/AttributeTypes.h>
#include <lagrange/Logger.h>
#include <lagrange/utils/Error.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/fmt/format.h>
#include <lagrange/utils/invalid.h>
#include <lagrange/utils/span.h>

#include <type_traits>

namespace lagrange::python {

namespace nb = nanobind;

template <typename ValueType>
Tensor<ValueType> create_empty_tensor()
{
    // Tensor object must have a non-null data point.
    static ValueType dummy_data;
    size_t shape[] = {0};
    return {&dummy_data, 1, shape, nb::handle()};
}

bool is_vector(const Shape& shape)
{
    const size_t ndim = shape.size();
    return ndim == 1 || (ndim == 2 && (shape[0] == 1 || shape[1] == 1));
}

bool check_shape(const Shape& shape, size_t expected_size)
{
    const size_t ndim = shape.size();
    switch (ndim) {
    case 1: return shape[0] == expected_size;
    case 2:
        if (shape[0] == 1)
            return shape[1] == expected_size;
        else if (shape[1] == 1)
            return shape[0] == expected_size;
        else
            return false;
    default: throw Error(lagrange::format("{}-dimensional tensor is not supported", ndim));
    }
}

bool check_shape(const Shape& shape, size_t expected_rows, size_t expected_cols)
{
    constexpr size_t dynamic = invalid<size_t>();
    const size_t ndim = shape.size();
    switch (ndim) {
    case 1:
        if (expected_rows != dynamic) {
            return (expected_cols == 1 || expected_cols == dynamic) && expected_rows == shape[0];
        } else {
            return expected_cols == dynamic || expected_cols == shape[0];
        }
    case 2:
        return (expected_rows == dynamic || expected_rows == shape[0]) &&
               (expected_cols == dynamic || expected_cols == shape[1]);
    default: throw Error(lagrange::format("{}-dimensional tensor is not supported", ndim));
    }
}

bool is_dense(const Shape& shape, const Stride& stride)
{
    const size_t ndim = shape.size();
    switch (ndim) {
    case 1: return static_cast<size_t>(shape[0]) == 0 || static_cast<size_t>(stride[0]) == 1;
    case 2:
        return shape[0] == 0 || shape[1] == 0 ||
               (static_cast<size_t>(stride[0]) == shape[1] && static_cast<size_t>(stride[1]) == 1);
    default: throw Error(lagrange::format("{}-dimensional tensor is not supported", ndim));
    }
}

template <typename ValueType>
std::tuple<span<ValueType>, Shape, Stride> tensor_to_span(Tensor<ValueType> tensor)
{
    Shape shape;
    Stride stride;
    size_t size = 1;
    for (size_t i = 0; i < tensor.ndim(); i++) {
        shape.push_back(tensor.shape(i));
        stride.push_back(tensor.stride(i));
        size *= tensor.shape(i);
    }
    span<ValueType> data(static_cast<ValueType*>(tensor.data()), size);
    return {data, shape, stride};
}

template <typename ValueType>
Tensor<ValueType> span_to_tensor(span<ValueType> values, nb::handle base)
{
    const size_t shape = values.size();
    return Tensor<ValueType>(values.data(), 1, &shape, base);
}

template <typename ValueType>
Tensor<ValueType> span_to_tensor(span<ValueType> values, span<const size_t> shape, nb::handle base)
{
    return Tensor<ValueType>(values.data(), shape.size(), shape.data(), base);
}

template <typename ValueType>
Tensor<ValueType> span_to_tensor(
    span<ValueType> values,
    span<const size_t> shape,
    span<const int64_t> stride,
    nb::handle base)
{
    return Tensor<ValueType>(values.data(), shape.size(), shape.data(), base, stride.data());
}

template <typename ValueType>
GenericTensor attribute_to_tensor(Attribute<ValueType>& attr, span<const size_t> shape)
{
    attr.ensure_shared_storage();
    const size_t default_shape[2] = {attr.get_num_elements(), attr.get_num_channels()};
    if (shape.empty() && attr.get_num_channels() != 1) shape = default_shape;
    if (!shape.empty()) {
        la_runtime_assert(shape.size() == 2);
        la_runtime_assert(attr.get_all().size() == shape[0] * shape[1]);
    }
    if (attr.is_read_only()) {
        return GenericTensor(shared_span_to_array(attr.get_owner(), shape));
    }
    return GenericTensor(shared_span_to_array(attr.ref_owner(), shape));
}

#define LA_X_tensor_utils(_, ValueType)                                                        \
    template Tensor<ValueType> create_empty_tensor<ValueType>();                               \
    template std::tuple<span<ValueType>, Shape, Stride> tensor_to_span<ValueType>(             \
        Tensor<ValueType>);                                                                    \
    template std::tuple<span<const ValueType>, Shape, Stride> tensor_to_span<const ValueType>( \
        Tensor<const ValueType>);                                                              \
    template Tensor<ValueType> span_to_tensor<ValueType>(span<ValueType>, nb::handle);         \
    template Tensor<const ValueType> span_to_tensor<const ValueType>(                          \
        span<const ValueType>,                                                                 \
        nb::handle);                                                                           \
    template Tensor<ValueType> span_to_tensor<ValueType>(                                      \
        span<ValueType>,                                                                       \
        span<const size_t>,                                                                    \
        nb::handle);                                                                           \
    template Tensor<const ValueType> span_to_tensor<const ValueType>(                          \
        span<const ValueType>,                                                                 \
        span<const size_t>,                                                                    \
        nb::handle);                                                                           \
    template Tensor<ValueType> span_to_tensor<ValueType>(                                      \
        span<ValueType>,                                                                       \
        span<const size_t>,                                                                    \
        span<const int64_t>,                                                                   \
        nb::handle);                                                                           \
    template Tensor<const ValueType> span_to_tensor<const ValueType>(                          \
        span<const ValueType>,                                                                 \
        span<const size_t>,                                                                    \
        span<const int64_t>,                                                                   \
        nb::handle);                                                                           \
    template GenericTensor attribute_to_tensor<ValueType>(                                     \
        Attribute<ValueType>&,                                                                 \
        span<const size_t>);
LA_ATTRIBUTE_X(tensor_utils, 0)

} // namespace lagrange::python
