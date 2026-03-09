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

#include <lagrange/raycasting/project_closest_vertex.h>

#include "prepare_attribute_ids.h"
#include "prepare_ray_caster.h"

#include <lagrange/Attribute.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/internal/find_attribute_utils.h>
#include <lagrange/raycasting/RayCaster.h>
#include <lagrange/utils/assert.h>
#include <lagrange/views.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <tbb/parallel_for.h>
#include <lagrange/utils/warnon.h>
// clang-format on

namespace lagrange::raycasting {

template <typename Scalar, typename Index>
void project_closest_vertex(
    const SurfaceMesh<Scalar, Index>& source,
    SurfaceMesh<Scalar, Index>& target,
    const ProjectCommonOptions& options,
    const RayCaster* ray_caster)
{
    la_runtime_assert(source.is_triangle_mesh());

    auto attribute_ids = prepare_attribute_ids(source, options);

    const auto& skip_vertex = options.skip_vertex;

    // Build a temporary ray caster if one is not provided.
    auto engine = prepare_ray_caster(source, ray_caster);
    if (engine) {
        ray_caster = engine.get();
    }

    // Gather source attribute metadata.
    struct AttrInfo
    {
        AttributeId source_id;
        AttributeId target_id;
        size_t num_channels;
    };
    std::vector<AttrInfo> attrs;
    attrs.reserve(attribute_ids.size());
    for (auto src_id : attribute_ids) {
        const auto& src_attr = source.get_attribute_base(src_id);
        la_runtime_assert(
            src_attr.get_element_type() == AttributeElement::Vertex,
            "Only vertex attributes are supported");

        auto name = source.get_attribute_name(src_id);
        size_t num_channels = src_attr.get_num_channels();

        AttributeId dst_id = lagrange::internal::find_or_create_attribute<Scalar>(
            target,
            name,
            AttributeElement::Vertex,
            src_attr.get_usage(),
            num_channels,
            lagrange::internal::ResetToDefault::No);

        attrs.push_back({src_id, dst_id, num_channels});
    }

    // Get the source facet indices for vertex lookup.
    auto source_facets = facet_view(source);

    // Get target vertex positions.
    auto target_vertices = vertex_view(target);
    const Index num_target_vertices = target.get_num_vertices();

    // Process target vertices in packets of 16 for SIMD efficiency.
    const Index num_packets = (num_target_vertices + 15) / 16;

    tbb::parallel_for(Index(0), num_packets, [&](Index packet_index) {
        const Index base = packet_index * 16;
        const size_t batchsize =
            static_cast<size_t>(std::min(num_target_vertices - base, Index(16)));

        RayCaster::Mask16 mask = RayCaster::Mask16::Constant(true);
        RayCaster::Point16f queries = RayCaster::Point16f::Zero();

        size_t num_skipped = 0;
        for (size_t b = 0; b < batchsize; ++b) {
            const Index vi = base + static_cast<Index>(b);
            if (skip_vertex && skip_vertex(vi)) {
                mask(static_cast<Eigen::Index>(b)) = false;
                ++num_skipped;
                continue;
            }
            queries.row(static_cast<Eigen::Index>(b)) =
                target_vertices.row(vi).template cast<float>();
        }

        if (num_skipped == batchsize) return;

        for (size_t b = batchsize; b < 16; ++b) {
            mask(static_cast<Eigen::Index>(b)) = false;
        }

        auto result = ray_caster->closest_vertex16(queries, mask);

        for (size_t b = 0; b < batchsize; ++b) {
            if (!mask(static_cast<Eigen::Index>(b))) continue;
            const Index vi = base + static_cast<Index>(b);

            la_runtime_assert(result.is_valid(b), "closest_point query returned no hit");
            la_runtime_assert(
                result.facet_indices(static_cast<Eigen::Index>(b)) <
                static_cast<uint32_t>(source.get_num_facets()));

            // Determine which vertex of the hit triangle is closest by picking the one with the
            // largest barycentric weight.
            float u = result.barycentric_coords(0, static_cast<Eigen::Index>(b));
            float v = result.barycentric_coords(1, static_cast<Eigen::Index>(b));
            float w = 1.0f - u - v;

            auto face = source_facets.row(result.facet_indices(static_cast<Eigen::Index>(b)));

            Index closest_vi;
            if (w >= u && w >= v) {
                closest_vi = face[0];
            } else if (u >= v) {
                closest_vi = face[1];
            } else {
                closest_vi = face[2];
            }

            // Copy attribute values from the closest source vertex.
            for (const auto& info : attrs) {
                const auto& src_attr = source.template get_attribute<Scalar>(info.source_id);
                auto& dst_attr = target.template ref_attribute<Scalar>(info.target_id);

                auto src_span = src_attr.get_all();
                auto dst_span = dst_attr.ref_all();

                size_t nc = info.num_channels;
                size_t dst_offset = static_cast<size_t>(vi) * nc;
                size_t src_offset = static_cast<size_t>(closest_vi) * nc;

                for (size_t c = 0; c < nc; ++c) {
                    dst_span[dst_offset + c] = src_span[src_offset + c];
                }
            }
        }
    });
}

#define LA_X_project_closest_vertex(_, Scalar, Index)       \
    template LA_RAYCASTING_API void project_closest_vertex( \
        const SurfaceMesh<Scalar, Index>&,                  \
        SurfaceMesh<Scalar, Index>&,                        \
        const ProjectCommonOptions&,                        \
        const RayCaster*);
LA_SURFACE_MESH_X(project_closest_vertex, 0)

} // namespace lagrange::raycasting
