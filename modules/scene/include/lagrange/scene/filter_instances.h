/*
 * Copyright 2026 Adobe. All rights reserved.
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

#include <lagrange/scene/SimpleScene.h>
#include <lagrange/scene/api.h>
#include <lagrange/utils/function_ref.h>

namespace lagrange::scene {

namespace detail {
// Stand-in for C++20 std::type_identity_t: makes the templated parameter a non-deduced
// context so `Index` is deduced from `scene` only, not from the callable.
template <typename T>
struct type_identity
{
    using type = T;
};
template <typename T>
using type_identity_t = typename type_identity<T>::type;
} // namespace detail

///
/// Build a new SimpleScene keeping only instances for which `keep(mesh_index, instance_index)`
/// returns true. Meshes with no remaining instances are dropped; mesh indices are compacted.
///
/// @param[in]  scene  Input scene.
/// @param[in]  keep   Predicate.
///
/// @return     The filtered scene.
///
/// @tparam     Scalar     Mesh scalar type.
/// @tparam     Index      Mesh index type.
/// @tparam     Dimension  Spatial dimension of the scene.
///
template <typename Scalar, typename Index, size_t Dimension>
LA_SCENE_API SimpleScene<Scalar, Index, Dimension> filter_instances(
    const SimpleScene<Scalar, Index, Dimension>& scene,
    function_ref<bool(detail::type_identity_t<Index>, detail::type_identity_t<Index>)> keep);

} // namespace lagrange::scene
