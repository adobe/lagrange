/*
 * Copyright 2025 Adobe. All rights reserved.
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

#include <nanobind/nanobind.h>

namespace lagrange::python {

// Wraps a C++ type `T` but renders as `Hint::value` in generated stubs. Use this
// when nanobind's auto-generated stub for `T` is narrower than what the binding
// actually accepts/returns (e.g. an Eigen vector that also takes a numpy array).
// Conversion is delegated to `T`'s own caster, so runtime behavior is unchanged.
//
// `Hint` is a tag type with a `static constexpr char value[]`; declare reusable
// hints with the LA_STUB_HINT macro below.
//
// Related nanobind issues:
// https://github.com/wjakob/nanobind/issues/1155
// https://github.com/wjakob/nanobind/issues/494
// https://github.com/wjakob/nanobind/discussions/1243
template <typename T, typename Hint>
struct StubType
{
    T value;
};

// Declares a stub-hint tag named `name` rendering as the type string `str`.
#define LA_STUB_HINT(name, str)              \
    struct name                              \
    {                                        \
        static constexpr char value[] = str; \
    }

// Common hint: accepts any array-like (list, tuple, numpy array, ...).
LA_STUB_HINT(ArrayLikeHint, "numpy.typing.ArrayLike");

} // namespace lagrange::python

namespace nanobind::detail {

template <typename T, typename Hint>
struct type_caster<lagrange::python::StubType<T, Hint>>
{
    using Wrapper = lagrange::python::StubType<T, Hint>;
    using TCaster = make_caster<T>;

    NB_TYPE_CASTER(Wrapper, const_name(Hint::value))

    bool from_python(handle src, uint8_t flags, cleanup_list* cleanup) noexcept
    {
        TCaster caster;
        if (!caster.from_python(src, flags, cleanup)) return false;
        value.value = caster.operator cast_t<T>();
        return true;
    }

    static handle from_cpp(const Wrapper& w, rv_policy policy, cleanup_list* cleanup) noexcept
    {
        return TCaster::from_cpp(w.value, policy, cleanup);
    }
};

} // namespace nanobind::detail
