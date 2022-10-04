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

#include <lagrange/SurfaceMesh.h>
#include <lagrange/utils/function_ref.h>
#include <lagrange/utils/span.h>

#include <initializer_list>

namespace lagrange {

///
/// @defgroup   group-surfacemesh-utils Mesh utilities
/// @ingroup    group-surfacemesh
///
/// Various mesh processing utilities.
///
/// @{

/**
 * Combine multiple meshes into a single mesh.
 *
 * @tparam     Scalar  Mesh scalar type.
 * @tparam     Index   Mesh index type.
 *
 * @param[in]  meshes               The set of input mesh pointers.
 * @param[in]  preserve_attributes  Preserve shared attributes and map them to
 *                                  the output mesh.
 *
 * @returns The combined mesh.
 */
template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> combine_meshes(
    std::initializer_list<const SurfaceMesh<Scalar, Index>*> meshes,
    bool preserve_attributes = true);

/**
 * Combine multiple meshes into a single mesh, where `meshes` is a span of mesh
 * objects.
 *
 * @overload SurfaceMesh<Scalar, Index> combine_meshes(span<const SurfaceMesh<Scalar, Index>*>,
 * bool)
 */
template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> combine_meshes(
    span<const SurfaceMesh<Scalar, Index>> meshes,
    bool preserve_attributes = true);

/**
 * Combine multiple meshes into a single mesh.  This is the most generic
 * version, where `get_mesh(i)` provides the `i`th mesh.
 *
 * @overload SurfaceMesh<Scalar, Index> combine_meshes(span<const SurfaceMesh<Scalar, Index>*>,
 * bool)
 */
template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> combine_meshes(
    size_t num_meshes,
    function_ref<const SurfaceMesh<Scalar, Index>&(size_t)> get_mesh,
    bool preserve_attributes = true);

/// @}

} // namespace lagrange
