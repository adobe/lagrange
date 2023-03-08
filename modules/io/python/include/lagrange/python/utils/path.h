/*
 * Copyright 2023 Adobe. All rights reserved.
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

#include <lagrange/fs/filesystem.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <nanobind/nanobind.h>
#include <lagrange/utils/warnon.h>
// clang-format on

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

template <>
struct type_caster<lagrange::fs::path>
{
    static handle from_cpp(const lagrange::fs::path& path, rv_policy, cleanup_list*) noexcept
    {
        str py_str = to_py_str(path.native());
        if (py_str.is_valid()) {
            try {
                return module_::import_("pathlib").attr("Path")(py_str).release();
            } catch (python_error& e) {
                e.restore();
            }
        }
        return handle();
    }

    template <typename Char = typename lagrange::fs::path::value_type>
    bool from_python(handle src, uint8_t, cleanup_list*) noexcept
    {
        bool success = false;

        /* PyUnicode_FSConverter and PyUnicode_FSDecoder normally take care of
           calling PyOS_FSPath themselves, but that's broken on PyPy (see PyPy
           issue #3168) so we do it ourselves instead. */
        PyObject* buf = PyOS_FSPath(src.ptr());
        if (buf) {
            PyObject* native = nullptr;
            if constexpr (std::is_same_v<Char, char>) {
                if (PyUnicode_FSConverter(buf, &native)) {
                    if (char* s = PyBytes_AsString(native)) {
                        value = s; // Points to internal buffer, no need to free
                        success = true;
                    }
                }
            } else {
                if (PyUnicode_FSDecoder(buf, &native)) {
                    if (wchar_t* s = PyUnicode_AsWideCharString(native, nullptr)) {
                        value = s;
                        PyMem_Free(s); // New string, must free
                        success = true;
                    }
                }
            }

            Py_DECREF(buf);
            Py_XDECREF(native);
        }

        if (!success) PyErr_Clear();

        return success;
    }

    NB_TYPE_CASTER(lagrange::fs::path, const_name("os.PathLike"));

private:
    static str to_py_str(const std::string& s)
    {
        return steal<str>(PyUnicode_DecodeFSDefaultAndSize(s.c_str(), (Py_ssize_t)s.size()));
    }

    static str to_py_str(const std::wstring& s)
    {
        return steal<str>(PyUnicode_FromWideChar(s.c_str(), (Py_ssize_t)s.size()));
    }
};

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
