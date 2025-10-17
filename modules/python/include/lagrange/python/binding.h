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

// Avoids potential ODR violations by grouping all nanobind includes in a single file.
// https://nanobind.readthedocs.io/en/latest/faq.html#compilation-fails-with-a-static-assertion-mentioning-nb-make-opaque

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <nanobind/nanobind.h>
#include <nanobind/eigen/dense.h>
#include <nanobind/eval.h>
#include <nanobind/ndarray.h>
#include <nanobind/stl/array.h>
#include <nanobind/stl/bind_map.h>
#include <nanobind/stl/bind_vector.h>
#include <nanobind/stl/detail/nb_list.h>
#include <nanobind/stl/filesystem.h>
#include <nanobind/stl/function.h>
#include <nanobind/stl/map.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/pair.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string_view.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/tuple.h>
#include <nanobind/stl/unique_ptr.h>
#include <nanobind/stl/unordered_map.h>
#include <nanobind/stl/unordered_set.h>
#include <nanobind/stl/variant.h>
#include <nanobind/stl/vector.h>
#include <nanobind/trampoline.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <lagrange/python/utils/bind_safe_vector.h>

#include <lagrange/scene/Scene.h>

NB_MAKE_OPAQUE(lagrange::SafeVector<size_t>);
NB_MAKE_OPAQUE(lagrange::SafeVector<lagrange::scene::Node>);
NB_MAKE_OPAQUE(lagrange::SafeVector<lagrange::scene::SceneMeshInstance>);
NB_MAKE_OPAQUE(lagrange::SafeVector<lagrange::SurfaceMesh<double, uint32_t>>);
NB_MAKE_OPAQUE(lagrange::SafeVector<lagrange::scene::ImageExperimental>);
NB_MAKE_OPAQUE(lagrange::SafeVector<lagrange::scene::Texture>);
NB_MAKE_OPAQUE(lagrange::SafeVector<lagrange::scene::MaterialExperimental>);
NB_MAKE_OPAQUE(lagrange::SafeVector<lagrange::scene::Light>);
NB_MAKE_OPAQUE(lagrange::SafeVector<lagrange::scene::Camera>);
NB_MAKE_OPAQUE(lagrange::SafeVector<lagrange::scene::Skeleton>);
NB_MAKE_OPAQUE(lagrange::SafeVector<lagrange::scene::Animation>);
