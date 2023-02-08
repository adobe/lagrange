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

#include <lagrange/utils/StackVector.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <nanobind/nanobind.h>
#include <nanobind/stl/detail/nb_list.h>
#include <lagrange/utils/warnon.h>
// clang-format on

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

///
/// Type caster to map `lagrange::StackVector` to python list data structure.
///
/// @note Data are __copied__ between the two data structures.
///
template <typename T, size_t N> struct type_caster<lagrange::StackVector<T, N>>
 : list_caster<lagrange::StackVector<T, N>, T> { };

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
