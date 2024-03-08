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
#include <lagrange/scene/scene_utils.h>

#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/foreach_attribute.h>
#include <lagrange/utils/assert.h>

namespace lagrange::scene::utils {

template <typename Scalar, typename Index>
void convert_texcoord_uv_st(SurfaceMesh<Scalar, Index>& mesh, AttributeId attribute_id)
{
    seq_foreach_named_attribute_write(mesh, [&](std::string_view name, auto&& attr) -> void {
        using AttributeType = std::decay_t<decltype(attr)>;
        AttributeId current_attribute_id = mesh.get_attribute_id(name);

        if (attribute_id == invalid<AttributeId>()) {
            // return if the attribute is not UV
            if (attr.get_usage() != AttributeUsage::UV) return;
        } else {
            // return if the attribute is not the specified one
            if (attribute_id != current_attribute_id) return;
        }

        Attribute<typename AttributeType::ValueType>* values = nullptr;
        if constexpr (AttributeType::IsIndexed) {
            values = &attr.values();
        } else {
            values = &attr;
        }

        la_runtime_assert(attr.get_num_channels() == 2);
        for (size_t i = 0; i < values->get_num_elements(); ++i) {
            auto vt = values->ref_row(i);
            vt[1] = 1 - vt[1];
        }
    });
}
#define LA_X_convert_texcoord_uv_st(_, S, I) \
    template void convert_texcoord_uv_st(SurfaceMesh<S, I>& mesh, AttributeId attribute_id);
LA_SURFACE_MESH_X(convert_texcoord_uv_st, 0)
#undef LA_X_convert_texcoord_uv_st

}
