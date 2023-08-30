/*
 * Copyright 2018 Adobe. All rights reserved.
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

#include <exception>
#include <unordered_set>

#include <lagrange/Edge.h>
#include <lagrange/Mesh.h>
#include <lagrange/MeshTrait.h>
#include <lagrange/common.h>
#include <lagrange/legacy/inline.h>
#include <lagrange/utils/safe_cast.h>

namespace lagrange {
LAGRANGE_LEGACY_INLINE
namespace legacy {

/**
 * Check if the input mesh is vertex manifold for all vertices.
 *
 * @deprecated Use `mesh.is_vertex_manifold()` instead.
 */
template <typename MeshType>
[[deprecated("Use `mesh.is_vertex_manifold()` instead.")]] bool is_vertex_manifold(
    const MeshType& mesh)
{
    return mesh.is_vertex_manifold();
}
} // namespace legacy
} // namespace lagrange
