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
#include <lagrange/Attribute.h>
#include <lagrange/ExactPredicatesShewchuk.h>
#include <lagrange/IndexedAttribute.h>
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/compute_uv_orientation.h>
#include <lagrange/internal/find_attribute_utils.h>
#include <lagrange/internal/get_uv_attribute.h>
#include <lagrange/utils/Error.h>
#include <lagrange/utils/assert.h>
#include <lagrange/uv_mesh.h>
#include <lagrange/views.h>

// clang-format off
#include <lagrange/utils/warnoff.h>
#include <tbb/blocked_range.h>
#include <tbb/parallel_reduce.h>
#include <lagrange/utils/warnon.h>
// clang-format on

namespace lagrange {

namespace {

template <typename UVScalar, typename Scalar, typename Index>
UVOrientationCount compute_uv_orientation_impl(
    SurfaceMesh<Scalar, Index>& mesh,
    AttributeId out_id,
    std::string_view uv_attr_name)
{
    auto uv = internal::get_uv_attribute<Scalar, Index, UVScalar>(mesh, uv_attr_name);
    const auto& uv_values = std::get<0>(uv);
    const auto& uv_indices = std::get<1>(uv);

    auto out = mesh.template ref_attribute<int8_t>(out_id).ref_all();

    ExactPredicatesShewchuk predicates;

    auto counts = tbb::parallel_reduce(
        tbb::blocked_range<Index>(0, mesh.get_num_facets()),
        UVOrientationCount{},
        [&](const tbb::blocked_range<Index>& r, UVOrientationCount init) {
            double p0[2], p1[2], p2[2];
            for (Index f = r.begin(); f != r.end(); ++f) {
                const auto c = mesh.get_facet_corner_begin(f);
                const Index i0 = uv_indices[c + 0];
                const Index i1 = uv_indices[c + 1];
                const Index i2 = uv_indices[c + 2];
                p0[0] = static_cast<double>(uv_values(i0, 0));
                p0[1] = static_cast<double>(uv_values(i0, 1));
                p1[0] = static_cast<double>(uv_values(i1, 0));
                p1[1] = static_cast<double>(uv_values(i1, 1));
                p2[0] = static_cast<double>(uv_values(i2, 0));
                p2[1] = static_cast<double>(uv_values(i2, 1));
                const short orient = predicates.orient2D(p0, p1, p2);
                out[f] = static_cast<int8_t>(orient);
                if (orient > 0) {
                    ++init.positive;
                } else if (orient == 0) {
                    ++init.degenerate;
                } else {
                    ++init.negative;
                }
            }
            return init;
        },
        [](UVOrientationCount a, const UVOrientationCount& b) {
            a.positive += b.positive;
            a.degenerate += b.degenerate;
            a.negative += b.negative;
            return a;
        });

    if (counts.degenerate > 0) {
        logger().debug(
            "compute_uv_orientation: {} degenerate UV triangle(s) detected.",
            counts.degenerate);
    }
    return counts;
}

} // namespace

template <typename Scalar, typename Index>
UVOrientationCount compute_uv_orientation(
    SurfaceMesh<Scalar, Index>& mesh,
    const UVOrientationOptions& options)
{
    la_runtime_assert(
        mesh.is_triangle_mesh(),
        "compute_uv_orientation: mesh must be a triangle mesh.");

    AttributeId out_id = internal::find_or_create_attribute<int8_t>(
        mesh,
        options.output_attribute_name,
        Facet,
        AttributeUsage::Scalar,
        1,
        internal::ResetToDefault::No);

    UVMeshOptions uv_mesh_options;
    uv_mesh_options.uv_attribute_name = options.uv_attribute_name;
    return internal::dispatch_uv_scalar_type(
        mesh,
        uv_mesh_options,
        "compute_uv_orientation",
        [&](auto tag, AttributeId uv_attr_id) {
            using UVScalar = typename decltype(tag)::type;
            return compute_uv_orientation_impl<UVScalar>(
                mesh,
                out_id,
                mesh.get_attribute_name(uv_attr_id));
        });
}

#define LA_X_compute_uv_orientation(_, Scalar, Index)                              \
    template LA_CORE_API UVOrientationCount compute_uv_orientation<Scalar, Index>( \
        SurfaceMesh<Scalar, Index>&,                                               \
        const UVOrientationOptions&);
LA_SURFACE_MESH_X(compute_uv_orientation, 0)

} // namespace lagrange
