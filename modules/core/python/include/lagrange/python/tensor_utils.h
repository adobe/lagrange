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

#include <lagrange/Logger.h>
#include <lagrange/utils/span.h>

#include <lagrange/Attribute.h>
#include <lagrange/IndexedAttribute.h>
#include <lagrange/python/binding.h>
#include <lagrange/utils/SmallVector.h>

#include <type_traits>
#include <utility>
#include <vector>

namespace lagrange::python {
namespace nb = nanobind;
using namespace nb::literals;

template <typename ValueType>
using Tensor = nb::ndarray<ValueType, nb::numpy, nb::c_contig, nb::device::cpu>;
using GenericTensor = nb::ndarray<nb::c_contig, nb::numpy, nb::device::cpu>;
using Shape = SmallVector<size_t, 2>;
using Stride = SmallVector<int64_t, 2>;

/**
 * Create a Python capsule that owns a C++ value.
 *
 * This is useful for keeping a C++ buffer owner alive without retaining the Python object that
 * exposed the buffer.
 */
template <typename Owner>
nb::capsule make_owner_capsule(Owner owner)
{
    using OwnerType = std::decay_t<Owner>;
    auto* stored_owner = new OwnerType(std::move(owner));
    return nb::capsule(stored_owner, [](void* ptr) noexcept {
        delete static_cast<OwnerType*>(ptr);
    });
}

/**
 * Apply the Python storage defaults when an attribute is bound to a foreign NumPy buffer.
 *
 * Wrapping makes the attribute external and hands its storage to a buffer Lagrange does not
 * own, which activates the growth and shrink policies. Python callers expect a later resize to
 * detach rather than fail, so wrapping selects `SilentCopy` for both, replacing the native
 * `ErrorIfExternal` defaults.
 *
 * This is a creation-time default for the buffer being bound here. It is deliberately NOT
 * applied on export: `Attribute::ensure_shared_storage()` only changes `ErrorIfExternal` growth and
 * shrink policies to `SilentCopy` when promoting internal storage. Existing external storage
 * and all other policies are preserved.
 */
template <typename ValueType>
void set_python_attribute_policies(Attribute<ValueType>& attr)
{
    attr.set_growth_policy(AttributeGrowthPolicy::SilentCopy);
    attr.set_shrink_policy(AttributeShrinkPolicy::SilentCopy);
}

/**
 * Check if shape represent a valid vector.
 *
 * @param[in]  shape  The tensor shape.
 *
 * @return True if shape represents a vector.
 */
bool is_vector(const Shape& shape);

/**
 * Check if tensor shape can be interpreted as a valid 1D array.
 *
 * @param[in]  shape  The tensor shape.
 * @param[in]  expected_size  The expected array size.
 *
 * @return True if shape is a 1D array of the expected size.
 */
bool check_shape(const Shape& shape, size_t expected_size);

/**
 * Check if tensor shape can be interpreted as a valid 2D array.
 *
 * @param[in]  shape          The tensor shape.
 * @param[in]  expected_rows  The expected number of rows.
 *                            Use `invalid<size_t>()` for dynamic number of rows.
 * @param[in]  expected_cols  The expected number of cols.
 *                            Use `invalid<size_t>()` for dynamic number of
 *                            columns.
 *
 * @return True if shape is a 2D array of the expected rows and cols.
 */
bool check_shape(const Shape& shape, size_t expected_rows, size_t expected_cols);

/**
 * Check if tensor is a densely packed. I.e. all entries in the data buffer are
 * used.
 *
 * @param[in]  shape       Tensor shape.
 * @param[in]  stride      Tensor stride.
 *
 * @return True if the tensor is dense.
 */
bool is_dense(const Shape& shape, const Stride& stride);


/**
 * Create an empty tensor.
 *
 * @return A valid empty tensor object.
 */
template <typename ValueType>
Tensor<ValueType> create_empty_tensor();

template <typename ValueType>
std::tuple<span<ValueType>, Shape, Stride> tensor_to_span(Tensor<ValueType> tensor);

template <typename ValueType>
Tensor<ValueType> span_to_tensor(span<ValueType> values, nb::handle base);

template <typename ValueType>
Tensor<ValueType> span_to_tensor(span<ValueType> values, span<const size_t> shape, nb::handle base);

template <typename ValueType>
Tensor<ValueType> span_to_tensor(
    span<ValueType> values,
    span<const size_t> shape,
    span<const int64_t> stride,
    nb::handle base);

/** Export a shared allocation, preserving its constness and lifetime. */
template <typename ValueType>
Tensor<ValueType> shared_span_to_array(SharedSpan<ValueType> values, span<const size_t> shape = {})
{
    auto owner = make_owner_capsule(values.owner());
    return shape.empty() ? span_to_tensor(values.ref(), owner)
                         : span_to_tensor(values.ref(), shape, owner);
}

/**
 * Promote and export the active attribute allocation, preserving its dtype and write permission.
 * The dtype-erased C++ wrapper retains the array's runtime writability. Read-only attributes
 * produce read-only Python arrays; writable attributes produce writable Python arrays.
 */
template <typename ValueType>
GenericTensor attribute_to_tensor(Attribute<ValueType>& attr, span<const size_t> shape = {});

} // namespace lagrange::python
