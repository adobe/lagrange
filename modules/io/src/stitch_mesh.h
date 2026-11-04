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
#include <lagrange/foreach_attribute.h>
#include <lagrange/map_attribute.h>
#include <lagrange/mesh_cleanup/remove_duplicate_vertices.h>
#include <lagrange/utils/assert.h>
#include <lagrange/weld_indexed_attribute.h>


#include <numeric>

namespace lagrange::io {

template <typename Scalar, typename Index>
void weld_indexed_attributes(SurfaceMesh<Scalar, Index>& mesh)
{
    WeldOptions weld_opts;
    weld_opts.epsilon_abs = 0;
    weld_opts.epsilon_rel = 0;
    for (auto id : find_matching_attributes(mesh, AttributeElement::Indexed)) {
        weld_indexed_attribute(mesh, id, weld_opts);
    }
}

/// Indexed value rows created from vertex attributes stay addressed by the original vertex index.
/// New corners must be pointed at those rows before welding, or they keep the default index 0.
template <typename Scalar, typename Index>
void assign_indexed_corner_indices(
    SurfaceMesh<Scalar, Index>& mesh,
    Index corner_begin,
    span<const Index> value_indices)
{
    seq_foreach_attribute_write<AttributeElement::Indexed>(mesh, [&](auto&& attr) {
        auto indices = attr.indices().ref_all();
        for (size_t i = 0; i < value_indices.size(); ++i) {
            indices[static_cast<size_t>(corner_begin) + i] = value_indices[i];
        }
    });
}

template <typename Scalar, typename Index>
void stitch_mesh(
    SurfaceMesh<Scalar, Index>& mesh,
    span<size_t> old_to_new = {},
    bool weld_indexed = true)
{
    if (!old_to_new.empty()) {
        la_runtime_assert(
            old_to_new.size() == static_cast<size_t>(mesh.get_num_vertices()),
            "old_to_new must have one entry per input vertex");
    }
    // Point clouds have no corners to stitch. Keep their vertex attributes vertex-based.
    if (mesh.get_num_corners() == 0) {
        if (!old_to_new.empty()) {
            std::iota(old_to_new.begin(), old_to_new.end(), size_t(0));
        }
        return;
    }
    // Convert vertex attributes to indexed before stitching anything
    for (auto& id : find_matching_attributes(mesh, AttributeElement::Vertex)) {
        id = map_attribute_in_place(mesh, id, AttributeElement::Indexed);
    }

    // Now we can stitch vertices
    RemoveDuplicateVerticesOptions rm_opts;
    rm_opts.boundary_only = true;
    rm_opts.old_to_new = old_to_new;
    remove_duplicate_vertices(mesh, rm_opts);

    if (weld_indexed) {
        weld_indexed_attributes(mesh);
    }
}

} // namespace lagrange::io
