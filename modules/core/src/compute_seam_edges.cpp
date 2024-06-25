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
#include <lagrange/compute_seam_edges.h>

#include <lagrange/Attribute.h>
#include <lagrange/IndexedAttribute.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/foreach_attribute.h>
#include <lagrange/internal/find_attribute_utils.h>
#include <lagrange/internal/visit_attribute.h>
#include <lagrange/utils/Error.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/warning.h>

#include <tbb/parallel_for.h>

#include <optional>

namespace lagrange {

template <typename Scalar, typename Index>
AttributeId compute_seam_edges(
    SurfaceMesh<Scalar, Index>& mesh,
    AttributeId source_id,
    const SeamEdgesOptions& options)
{
    mesh.initialize_edges();

    AttributeId output_id = internal::find_or_create_attribute<uint8_t>(
        mesh,
        options.output_attribute_name,
        AttributeElement::Edge,
        AttributeUsage::Scalar,
        1,
        internal::ResetToDefault::Yes);
    auto output_is_seam = mesh.template ref_attribute<uint8_t>(output_id).ref_all();

    auto process_attribute = [&](auto&& attr) {
        auto indices = attr.indices().get_all();
        tbb::parallel_for(Index(0), mesh.get_num_edges(), [&](Index e) {
            auto v = mesh.get_edge_vertices(e);
            std::optional<std::array<Index, 2>> prev_indices;
            mesh.foreach_corner_around_edge(e, [&](Index c0) {
                const Index f = mesh.get_corner_facet(c0);
                auto fv = mesh.get_facet_vertices(f);
                const Index cs = mesh.get_facet_corner_begin(f);
                const Index lv0 = c0 - cs;
                const Index lv1 = (lv0 + 1) % fv.size();
                Index v0 = fv[lv0];
                Index v1 = fv[lv1];
                Index c1 = cs + lv1;
                if (v[0] != v0) {
                    la_debug_assert(v[0] == v1);
                    LA_IGNORE(v1);
                    std::swap(c0, c1);
                    std::swap(v0, v1);
                }
                la_debug_assert(v[0] == v0);
                la_debug_assert(v[1] == v1);
                if (!prev_indices.has_value()) {
                    prev_indices = {indices[c0], indices[c1]};
                } else {
                    if (((*prev_indices)[0] != indices[c0]) ||
                        ((*prev_indices)[1] != indices[c1])) {
                        output_is_seam[e] = 1;
                    }
                }
            });
        });
    };

    internal::visit_attribute_read(mesh, source_id, [&](auto&& attr) {
        using AttributeType = std::decay_t<decltype(attr)>;
        if constexpr (AttributeType::IsIndexed) {
            process_attribute(attr);
        } else {
            throw Error("Cannot compute seam edges for a non-indexed attribute.");
        }
    });

    return output_id;
}

#define LA_X_compute_facet_normal(_, Scalar, Index)      \
    template LA_CORE_API AttributeId compute_seam_edges( \
        SurfaceMesh<Scalar, Index>& mesh,                \
        AttributeId source_id,                           \
        const SeamEdgesOptions& options);
LA_SURFACE_MESH_X(compute_facet_normal, 0)

} // namespace lagrange
