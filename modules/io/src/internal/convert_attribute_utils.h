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

#include <lagrange/SurfaceMesh.h>
#include <lagrange/unify_index_buffer.h>

#include <vector>

namespace lagrange::io {

template <typename Scalar, typename Index>
bool involve_indexed_attribute(
    const SurfaceMesh<Scalar, Index>& mesh,
    span<const AttributeId> attr_ids)
{
    if (attr_ids.empty()) {
        bool has_indexed_attribute = false;
        mesh.seq_foreach_attribute_id([&](AttributeId id) {
            if (has_indexed_attribute) return;
            auto name = mesh.get_attribute_name(id);
            if (mesh.attr_name_is_reserved(name)) return;
            if (mesh.is_attribute_indexed(id)) {
                has_indexed_attribute = true;
            }
        });
        return has_indexed_attribute;
    } else {
        for (auto id : attr_ids) {
            if (mesh.is_attribute_indexed(id)) return true;
        }
        return false;
    }
}

template <typename Scalar, typename Index>
std::tuple<SurfaceMesh<Scalar, Index>, std::vector<AttributeId>> remap_indexed_attributes(
    const SurfaceMesh<Scalar, Index>& in_mesh,
    span<const AttributeId> in_attr_ids)
{
    std::vector<AttributeId> attr_ids(in_attr_ids.begin(), in_attr_ids.end());

    auto mesh = unify_index_buffer(in_mesh);
    std::transform(
        attr_ids.begin(),
        attr_ids.end(),
        attr_ids.begin(),
        [&](AttributeId id) -> AttributeId {
            auto name = in_mesh.get_attribute_name(id);
            la_runtime_assert(mesh.has_attribute(name));
            return mesh.get_attribute_id(name);
        });

    return {mesh, attr_ids};
}

} // namespace lagrange::io
