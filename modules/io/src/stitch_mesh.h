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

#include <lagrange/find_matching_attributes.h>
#include <lagrange/map_attribute.h>
#include <lagrange/mesh_cleanup/remove_duplicate_vertices.h>

namespace lagrange::io {

template <typename Scalar, typename Index>
void stitch_mesh(SurfaceMesh<Scalar, Index>& mesh)
{
    // Convert vertex attributes to indexed before stitching anything
    for (auto& id : find_matching_attributes(mesh, AttributeElement::Vertex)) {
        id = map_attribute_in_place(mesh, id, AttributeElement::Indexed);
    }

    // Now we can stitch vertices
    RemoveDuplicateVerticesOptions rm_opts;
    rm_opts.boundary_only = true;
    remove_duplicate_vertices(mesh, rm_opts);
}

} // namespace lagrange::io
