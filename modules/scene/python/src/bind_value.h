/*
 * Copyright 2024 Adobe. All rights reserved.
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
#include <lagrange/scene/SceneExtension.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/variant.h>
#include <lagrange/utils/warnon.h>
// clang-format on

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

template <>
struct type_caster<lagrange::scene::Value>
{
    NB_TYPE_CASTER(lagrange::scene::Value, const_name("Value"))

    template <typename T>
    bool try_cast(const handle& src, uint8_t flags, cleanup_list* cleanup)
    {
        using CasterT = make_caster<T>;

        flags |= (uint8_t)cast_flags::none_disallowed;
        CasterT caster;
        if (!caster.from_python(src, flags, cleanup)) return false;
        value.set(caster.operator cast_t<T>());
        return true;
    }

    bool from_python(handle src, uint8_t flags, cleanup_list* cleanup) noexcept
    {
        if (PyNumber_Check(src.ptr())) {
            lagrange::logger().debug("Number!");
            return try_cast<int>(src, flags, cleanup) || try_cast<double>(src, flags, cleanup);
        } else if (try_cast<std::string>(src, flags, cleanup)) {
            lagrange::logger().debug("String!");
            return true;
        } else if (PySequence_Check(src.ptr())) {
            lagrange::logger().debug("Array of size {}!", PySequence_Size(src.ptr()));

            size_t n;
            PyObject* temp;
            /* Will initialize 'temp' (NULL in the case of a failure.) */
            PyObject** o = seq_get(src.ptr(), &n, &temp);

            bool success = o != nullptr;

            if (success) {
                lagrange::scene::Value::Array arr;
                arr.reserve(n);
                make_caster<lagrange::scene::Value> caster;

                for (size_t i = 0; i < n; ++i) {
                    if (!caster.from_python(o[i], flags, cleanup)) {
                        success = false;
                        break;
                    }

                    arr.push_back(caster.operator cast_t<lagrange::scene::Value>());
                }
                value.set(arr);
            }

            Py_XDECREF(temp);
            return success;
        } else if (PyMapping_Check(src.ptr())) {
            lagrange::logger().debug("Dict!");

            PyObject* items = PyMapping_Items(src.ptr());
            if (items == nullptr) {
                PyErr_Clear();
                return false;
            }

            Py_ssize_t size = NB_LIST_GET_SIZE(items);
            bool success = size >= 0;

            make_caster<std::string> key_caster;
            make_caster<lagrange::scene::Value> val_caster;
            lagrange::scene::Value::Object obj;

            for (Py_ssize_t i = 0; i < size; ++i) {
                PyObject* item = NB_LIST_GET_ITEM(items, i);
                PyObject* key = NB_TUPLE_GET_ITEM(item, 0);
                PyObject* val = NB_TUPLE_GET_ITEM(item, 1);

                if (!key_caster.from_python(key, flags, cleanup)) {
                    success = false;
                    break;
                }

                if (!val_caster.from_python(val, flags, cleanup)) {
                    success = false;
                    break;
                }

                obj.emplace(
                    key_caster.operator cast_t<std::string>(),
                    val_caster.operator cast_t<lagrange::scene::Value>());
            }
            value.set(obj);

            Py_DECREF(items);

            return success;
        }

        return false;
    }

    static handle
    from_cpp(const lagrange::scene::Value& value, rv_policy policy, cleanup_list* cleanup) noexcept
    {
        if (value.is_bool()) {
            return make_caster<bool>::from_cpp(value.get_bool(), policy, cleanup);
        } else if (value.is_int()) {
            return make_caster<int>::from_cpp(value.get_int(), policy, cleanup);
        } else if (value.is_real()) {
            return make_caster<double>::from_cpp(value.get_real(), policy, cleanup);
        } else if (value.is_string()) {
            return make_caster<std::string>::from_cpp(value.get_string(), policy, cleanup);
        } else if (value.is_buffer()) {
            return make_caster<lagrange::scene::Value::Buffer>::from_cpp(
                value.get_buffer(),
                policy,
                cleanup);
        } else if (value.is_array()) {
            return make_caster<lagrange::scene::Value::Array>::from_cpp(
                value.get_array(),
                policy,
                cleanup);
        } else if (value.is_object()) {
            return make_caster<lagrange::scene::Value::Object>::from_cpp(
                value.get_object(),
                policy,
                cleanup);
        }
        return none().release();
    }
};

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)

