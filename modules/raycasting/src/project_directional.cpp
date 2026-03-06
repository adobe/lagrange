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

#include <lagrange/raycasting/project_directional.h>

#include "prepare_attribute_ids.h"
#include "prepare_ray_caster.h"

#include <lagrange/Attribute.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/compute_vertex_normal.h>
#include <lagrange/find_matching_attributes.h>
#include <lagrange/internal/find_attribute_utils.h>
#include <lagrange/raycasting/RayCaster.h>
#include <lagrange/raycasting/project_closest_point.h>
#include <lagrange/raycasting/project_closest_vertex.h>
#include <lagrange/utils/Error.h>
#include <lagrange/utils/assert.h>
#include <lagrange/views.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <tbb/parallel_for.h>
#include <spdlog/fmt/fmt.h>
#include <lagrange/utils/warnon.h>
// clang-format on

#include <algorithm>
#include <cmath>

namespace lagrange::raycasting {

template <typename Scalar, typename Index>
void project_directional(
    const SurfaceMesh<Scalar, Index>& source,
    SurfaceMesh<Scalar, Index>& target,
    const ProjectDirectionalOptions& options,
    const RayCaster* ray_caster)
{
    la_runtime_assert(source.is_triangle_mesh());

    auto attribute_ids = prepare_attribute_ids(source, options);

    const auto cast_mode = options.cast_mode;
    const auto fallback_mode = options.fallback_mode;
    const Scalar default_value = static_cast<Scalar>(options.default_value);
    const auto& user_callback = options.user_callback;
    const auto& skip_vertex = options.skip_vertex;

    // Resolve the direction variant.
    // - monostate: find or compute vertex normals on the target mesh.
    // - AttributeId: per-vertex direction attribute on the target mesh.
    // - Vector3f: uniform direction for all rays.
    bool uniform_direction = false;
    Eigen::Vector3f uniform_dir = Eigen::Vector3f::UnitZ();
    AttributeId direction_attr_id = invalid<AttributeId>();

    struct DirectionCleanup
    {
        SurfaceMesh<Scalar, Index>* mesh = nullptr;
        AttributeId attr_id = invalid<AttributeId>();
        ~DirectionCleanup()
        {
            if (mesh && attr_id != invalid<AttributeId>()) {
                mesh->delete_attribute(mesh->get_attribute_name(attr_id));
            }
        }
    };
    DirectionCleanup direction_cleanup;

    std::visit(
        [&](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, Eigen::Vector3f>) {
                uniform_direction = true;
                uniform_dir = arg.normalized();
            } else if constexpr (std::is_same_v<T, AttributeId>) {
                direction_attr_id = arg;
                auto res = internal::check_attribute<Scalar>(
                    target,
                    direction_attr_id,
                    AttributeElement::Vertex,
                    AttributeUsage::Normal,
                    3,
                    internal::ShouldBeWritable::No);
                if (!res.success) {
                    throw Error(fmt::format("Invalid direction attribute: {}", res.msg));
                }
            } else {
                // std::monostate: find existing vertex normal or compute one.
                // Search for an existing vertex normal attribute on the target mesh.
                AttributeMatcher matcher;
                matcher.usages = AttributeUsage::Normal;
                matcher.element_types = AttributeElement::Vertex;
                matcher.num_channels = 3;
                auto found_id = find_matching_attribute(target, matcher);
                if (found_id.has_value()) {
                    direction_attr_id = found_id.value();
                } else {
                    // Compute vertex normals.
                    direction_attr_id = compute_vertex_normal(target);
                    direction_cleanup.mesh = &target;
                    direction_cleanup.attr_id = direction_attr_id;
                }
            }
        },
        options.direction);

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
        auto name = source.get_attribute_name(src_id);
        la_runtime_assert(
            source.has_attribute(name),
            fmt::format("Source mesh missing attribute: {}", name));
        const auto& src_base = source.get_attribute_base(src_id);
        la_runtime_assert(
            src_base.get_element_type() == AttributeElement::Vertex,
            fmt::format("Only vertex attributes are supported: {}", name));

        size_t num_channels = src_base.get_num_channels();

        AttributeId dst_id = lagrange::internal::find_or_create_attribute<Scalar>(
            target,
            name,
            AttributeElement::Vertex,
            src_base.get_usage(),
            num_channels,
            lagrange::internal::ResetToDefault::No);

        attrs.push_back({src_id, dst_id, num_channels});
    }

    // Compute bounding box diagonal for the offset in BOTH_WAYS mode.
    auto source_verts = vertex_view(source);
    Eigen::Vector3f bbox_min = source_verts.template cast<float>().colwise().minCoeff().transpose();
    Eigen::Vector3f bbox_max = source_verts.template cast<float>().colwise().maxCoeff().transpose();
    float diag = (bbox_max - bbox_min).norm();

    // Get source facet connectivity.
    auto source_facets = facet_view(source);

    // Get target vertex positions.
    auto target_vertices = vertex_view(target);
    const Index num_target_vertices = target.get_num_vertices();

    // Get per-vertex direction data (if applicable).
    lagrange::span<const Scalar> dir_data;
    if (!uniform_direction) {
        const auto& dir_attr = target.template get_attribute<Scalar>(direction_attr_id);
        dir_data = dir_attr.get_all();
    }

    // Track which vertices were hit (needed for FallbackMode fallback).
    // Using char instead of bool for thread-safe concurrent writes.
    std::vector<char> is_hit;
    if (fallback_mode != FallbackMode::Constant) {
        is_hit.assign(num_target_vertices, false);
    }

    // Process target vertices in packets of 16 for SIMD efficiency.
    const Index num_packets = (num_target_vertices + 15) / 16;

    tbb::parallel_for(Index(0), num_packets, [&](Index packet_index) {
        Index base = packet_index * 16;
        size_t batchsize = static_cast<size_t>(std::min(num_target_vertices - base, Index(16)));

        RayCaster::Mask16 mask = RayCaster::Mask16::Constant(true);
        RayCaster::Point16f origins;
        RayCaster::Direction16f dirs;

        size_t num_skipped = 0;
        for (size_t b = 0; b < batchsize; ++b) {
            Index i = base + static_cast<Index>(b);
            if (skip_vertex && skip_vertex(i)) {
                if (!is_hit.empty()) {
                    is_hit[i] = true;
                }
                mask(static_cast<Eigen::Index>(b)) = false;
                ++num_skipped;
                continue;
            }
            origins.row(static_cast<Eigen::Index>(b)) =
                target_vertices.row(i).template cast<float>();
            if (uniform_direction) {
                dirs.row(static_cast<Eigen::Index>(b)) = uniform_dir.transpose();
            } else {
                size_t offset = static_cast<size_t>(i) * 3;
                Eigen::Vector3f d(
                    static_cast<float>(dir_data[offset + 0]),
                    static_cast<float>(dir_data[offset + 1]),
                    static_cast<float>(dir_data[offset + 2]));
                dirs.row(static_cast<Eigen::Index>(b)) = d.normalized().transpose();
            }
        }

        if (num_skipped == batchsize) return;

        for (size_t b = batchsize; b < 16; ++b) {
            mask(static_cast<Eigen::Index>(b)) = false;
        }

        auto result = ray_caster->cast16(origins, dirs, mask);

        if (cast_mode == CastMode::BothWays) {
            // Try again in the opposite direction with a small offset.
            RayCaster::Point16f origins2 = origins;
            RayCaster::Direction16f dirs2 = -dirs;
            for (size_t b = 0; b < batchsize; ++b) {
                origins2.row(static_cast<Eigen::Index>(b)) +=
                    1e-6f * diag * dirs.row(static_cast<Eigen::Index>(b));
            }

            auto result2 = ray_caster->cast16(origins2, dirs2, mask);

            // Keep the closer hit.
            for (size_t b = 0; b < batchsize; ++b) {
                if (!mask(static_cast<Eigen::Index>(b))) continue;
                bool hit1 = result.is_valid(b);
                bool hit2 = result2.is_valid(b);
                if (hit2) {
                    float len2 =
                        std::abs(1e-6f * diag - result2.ray_depths(static_cast<Eigen::Index>(b)));
                    if (!hit1 || len2 < result.ray_depths(static_cast<Eigen::Index>(b))) {
                        result.valid_mask |= (1u << b);
                        result.mesh_indices(static_cast<Eigen::Index>(b)) =
                            result2.mesh_indices(static_cast<Eigen::Index>(b));
                        result.facet_indices(static_cast<Eigen::Index>(b)) =
                            result2.facet_indices(static_cast<Eigen::Index>(b));
                        result.barycentric_coords.col(static_cast<Eigen::Index>(b)) =
                            result2.barycentric_coords.col(static_cast<Eigen::Index>(b));
                        result.ray_depths(static_cast<Eigen::Index>(b)) = len2;
                    }
                }
            }
        }

        // Write results for each ray in the packet.
        for (size_t b = 0; b < batchsize; ++b) {
            if (!mask(static_cast<Eigen::Index>(b))) continue;
            Index i = base + static_cast<Index>(b);
            bool hit = result.is_valid(b);

            if (hit) {
                la_runtime_assert(
                    result.facet_indices(static_cast<Eigen::Index>(b)) <
                    static_cast<uint32_t>(source.get_num_facets()));

                float u = result.barycentric_coords(0, static_cast<Eigen::Index>(b));
                float v = result.barycentric_coords(1, static_cast<Eigen::Index>(b));
                Scalar b0 = static_cast<Scalar>(1.0f - u - v);
                Scalar b1 = static_cast<Scalar>(u);
                Scalar b2 = static_cast<Scalar>(v);

                auto face = source_facets.row(result.facet_indices(static_cast<Eigen::Index>(b)));

                for (const auto& info : attrs) {
                    const auto& src_attr = source.template get_attribute<Scalar>(info.source_id);
                    auto& dst_attr = target.template ref_attribute<Scalar>(info.target_id);

                    auto src_span = src_attr.get_all();
                    auto dst_span = dst_attr.ref_all();

                    size_t nc = info.num_channels;
                    size_t dst_offset = static_cast<size_t>(i) * nc;
                    size_t s0 = static_cast<size_t>(face[0]) * nc;
                    size_t s1 = static_cast<size_t>(face[1]) * nc;
                    size_t s2 = static_cast<size_t>(face[2]) * nc;

                    for (size_t c = 0; c < nc; ++c) {
                        dst_span[dst_offset + c] =
                            src_span[s0 + c] * b0 + src_span[s1 + c] * b1 + src_span[s2 + c] * b2;
                    }
                }
            } else {
                // No hit: fill with default for Constant mode.
                for (const auto& info : attrs) {
                    auto& dst_attr = target.template ref_attribute<Scalar>(info.target_id);
                    auto dst_span = dst_attr.ref_all();
                    size_t nc = info.num_channels;
                    size_t dst_offset = static_cast<size_t>(i) * nc;
                    for (size_t c = 0; c < nc; ++c) {
                        dst_span[dst_offset + c] = default_value;
                    }
                }
            }

            if (!is_hit.empty()) {
                is_hit[i] = hit;
            }
            if (user_callback) {
                user_callback(i, hit);
            }
        }
    });

    // If there is any vertex without a hit, defer to the relevant fallback function.
    if (fallback_mode != FallbackMode::Constant) {
        bool all_hit = std::all_of(is_hit.begin(), is_hit.end(), [](char x) { return bool(x); });
        if (!all_hit) {
            auto fallback_skip = [&](Index i) { return bool(is_hit[i]); };
            ProjectCommonOptions fb_options(options);
            fb_options.skip_vertex = fallback_skip;
            if (fallback_mode == FallbackMode::ClosestPoint) {
                project_closest_point(source, target, fb_options, ray_caster);
            } else if (fallback_mode == FallbackMode::ClosestVertex) {
                project_closest_vertex(source, target, fb_options, ray_caster);
            } else {
                la_runtime_assert(false, "Unknown FallbackMode");
            }
        }
    }
}

#define LA_X_project_directional(_, Scalar, Index)       \
    template LA_RAYCASTING_API void project_directional( \
        const SurfaceMesh<Scalar, Index>&,               \
        SurfaceMesh<Scalar, Index>&,                     \
        const ProjectDirectionalOptions&,                \
        const RayCaster*);
LA_SURFACE_MESH_X(project_directional, 0)

} // namespace lagrange::raycasting
