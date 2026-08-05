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

#include <lagrange/SurfaceMesh.h>
#include <lagrange/bvh/api.h>
#include <lagrange/bvh/resolve_tjunctions.h>

namespace lagrange::bvh::internal {

///
/// Resolve T-junctions formed by collinear, overlapping edges (BVH-accelerated variant).
///
/// This behaves identically to @ref lagrange::bvh::resolve_tjunctions, but locates the candidate
/// vertices near each edge using an AABB tree built over the mesh vertices, rather than an
/// axis-aligned sort-and-sweep. It shares @ref lagrange::bvh::ResolveTJunctionsOptions and produces
/// the same output. Kept as an internal reference/benchmark alternative to the public variant.
///
/// @param[in,out] mesh     Input mesh (triangle or polygonal). Modified in place.
/// @param[in]     options  Optional settings.
///
/// @tparam        Scalar   Mesh scalar type.
/// @tparam        Index    Mesh index type.
///
/// @see lagrange::bvh::resolve_tjunctions
///
template <typename Scalar, typename Index>
void resolve_tjunctions(SurfaceMesh<Scalar, Index>& mesh, ResolveTJunctionsOptions options = {});

} // namespace lagrange::bvh::internal
