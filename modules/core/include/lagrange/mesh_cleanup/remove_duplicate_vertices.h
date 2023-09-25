/*
 * Copyright 2016 Adobe. All rights reserved.
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

#ifdef LAGRANGE_ENABLE_LEGACY_FUNCTIONS
    #include <lagrange/mesh_cleanup/legacy/remove_duplicate_vertices.h>
#endif

#include <lagrange/SurfaceMesh.h>

#include <vector>

namespace lagrange {

///
/// Option struct for remove_duplicate_vertices.
///
struct RemoveDuplicateVerticesOptions
{
    /// Additional attributes to include for duplicate vertex detection.
    /// Two vertices are duplicates if their positions and all the extra attributes are the same.
    std::vector<AttributeId> extra_attributes = {};
};

///
/// Removes duplicate vertices from a mesh.
///
/// @tparam Scalar          Mesh scalar type
/// @tparam Index           Mesh index type
///
/// @param[in,out] mesh     The mesh to remove duplicate vertices from.
/// @param         options  The options for duplicate vertex detection.
///
template <typename Scalar, typename Index>
void remove_duplicate_vertices(
    SurfaceMesh<Scalar, Index>& mesh,
    const RemoveDuplicateVerticesOptions& options = {});

} // namespace lagrange
