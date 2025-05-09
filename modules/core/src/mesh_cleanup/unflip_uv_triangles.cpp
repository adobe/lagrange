/*
 * Copyright 2025 Adobe. All rights reserved.
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
#include <lagrange/IndexedAttribute.h>
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/internal/get_uv_attribute.h>
#include <lagrange/mesh_cleanup/unflip_uv_triangles.h>
#include <lagrange/utils/assert.h>
#include <lagrange/utils/timing.h>
#include <lagrange/utils/triangle_area.h>
#include <lagrange/utils/triangle_orientation_2d.h>
#include <lagrange/utils/triangle_uv_distortion.h>
#include <lagrange/views.h>
#include <lagrange/weld_indexed_attribute.h>

#include <numeric>
#include <vector>

namespace lagrange {

template <typename Scalar, typename Index>
void unflip_uv_triangles(SurfaceMesh<Scalar, Index>& mesh, const UnflipUVOptions& options)
{
    la_runtime_assert(mesh.is_triangle_mesh(), "Mesh must be a triangle mesh.");

    AttributeId uv_attr_id;
    if (options.uv_attribute_name.empty()) {
        uv_attr_id = internal::get_uv_id(mesh, options.uv_attribute_name);
    } else {
        la_runtime_assert(mesh.has_attribute(options.uv_attribute_name));
        uv_attr_id = mesh.get_attribute_id(options.uv_attribute_name);
    }
    la_runtime_assert(
        mesh.is_attribute_indexed(options.uv_attribute_name),
        "UV attribute must be indexed.");

    auto vertices = vertex_view(mesh);
    auto facets = facet_view(mesh);

    auto& uv_attr = mesh.template ref_indexed_attribute<Scalar>(uv_attr_id);
    auto& uv_values_attr = uv_attr.values();
    auto uv_indices = reshaped_ref(uv_attr.indices(), 3);
    la_debug_assert(uv_indices.maxCoeff() < uv_values_attr.get_num_elements());

    auto uv_values = matrix_view(uv_values_attr);
    auto get_uv = [&](Index fid, Index lv) -> Eigen::Matrix<Scalar, 1, 2> {
        Index i = uv_indices(fid, lv);
        return uv_values.row(i).eval();
    };

    std::vector<Scalar> additional_uv_values;
    auto update_uv = [&](Index fid, Index lv, Eigen::Matrix<Scalar, 1, 2> new_uv) {
        additional_uv_values.insert(additional_uv_values.end(), new_uv.data(), new_uv.data() + 2);
        Index old_id = uv_indices(fid, lv);
        uv_indices(fid, lv) = static_cast<Index>(uv_values_attr.get_num_elements()) +
                              static_cast<Index>(additional_uv_values.size()) / 2 - 1;
        auto old_uv = uv_values.row(old_id);
        logger().trace(
            "fid: {}, lv: {}, new_uv: [{}, {}], old_uv: [{}, {}]",
            fid,
            lv,
            new_uv[0],
            new_uv[1],
            old_uv[0],
            old_uv[1]);
    };

    auto compute_offset = [](auto uv0,
                             auto uv1,
                             auto uv2,
                             Scalar L01,
                             Scalar L02,
                             Scalar L12,
                             Scalar l12,
                             span<Scalar, 4> offsets_u,
                             span<Scalar, 4> offsets_v) {
        if (L12 == 0) {
            offsets_u[0] = invalid<Scalar>();
            offsets_u[1] = invalid<Scalar>();
            offsets_u[2] = invalid<Scalar>();
            offsets_u[3] = invalid<Scalar>();
            offsets_v[0] = invalid<Scalar>();
            offsets_v[1] = invalid<Scalar>();
            offsets_v[2] = invalid<Scalar>();
            offsets_v[3] = invalid<Scalar>();
            return;
        }

        Scalar ref_l01 = l12 / L12 * L01;
        Scalar ref_l02 = l12 / L12 * L02;

        Scalar du01 = ref_l01 * ref_l01 - (uv0[1] - uv1[1]) * (uv0[1] - uv1[1]);
        Scalar du02 = ref_l02 * ref_l02 - (uv0[1] - uv2[1]) * (uv0[1] - uv2[1]);
        Scalar dv01 = ref_l01 * ref_l01 - (uv0[0] - uv1[0]) * (uv0[0] - uv1[0]);
        Scalar dv02 = ref_l02 * ref_l02 - (uv0[0] - uv2[0]) * (uv0[0] - uv2[0]);

        if (du01 > 0) {
            offsets_u[0] = uv1[0] - uv0[0] + std::sqrt(du01);
            offsets_u[1] = uv1[0] - uv0[0] - std::sqrt(du01);
        } else {
            offsets_u[0] = uv1[0] - uv0[0];
            offsets_u[1] = uv1[0] - uv0[0];
        }

        if (du02 > 0) {
            offsets_u[2] = uv2[0] - uv0[0] + std::sqrt(du02);
            offsets_u[3] = uv2[0] - uv0[0] - std::sqrt(du02);
        } else {
            offsets_u[2] = uv2[0] - uv0[0];
            offsets_u[3] = uv2[0] - uv0[0];
        }

        if (dv01 > 0) {
            offsets_v[0] = uv1[1] - uv0[1] + std::sqrt(dv01);
            offsets_v[1] = uv1[1] - uv0[1] - std::sqrt(dv01);
        } else {
            offsets_v[0] = uv1[1] - uv0[1];
            offsets_v[1] = uv1[1] - uv0[1];
        }

        if (dv02 > 0) {
            offsets_v[2] = uv2[1] - uv0[1] + std::sqrt(dv02);
            offsets_v[3] = uv2[1] - uv0[1] - std::sqrt(dv02);
        } else {
            offsets_v[2] = uv2[1] - uv0[1];
            offsets_v[3] = uv2[1] - uv0[1];
        }
    };

    auto compute_offset_amount = [&](Index fid) {
        auto p0 = vertices.row(facets(fid, 0)).eval();
        auto p1 = vertices.row(facets(fid, 1)).eval();
        auto p2 = vertices.row(facets(fid, 2)).eval();
        auto uv0 = get_uv(fid, 0);
        auto uv1 = get_uv(fid, 1);
        auto uv2 = get_uv(fid, 2);

        Scalar L01 = (p1 - p0).norm();
        Scalar L02 = (p2 - p0).norm();
        Scalar L12 = (p2 - p1).norm();
        Scalar l01 = (uv1 - uv0).norm();
        Scalar l02 = (uv2 - uv0).norm();
        Scalar l12 = (uv2 - uv1).norm();

        std::array<Scalar, 12> u_offsets;
        std::array<Scalar, 12> v_offsets;

        compute_offset(
            uv0,
            uv1,
            uv2,
            L01,
            L02,
            L12,
            l12,
            span<Scalar, 4>(u_offsets.data(), 4),
            span<Scalar, 4>(v_offsets.data(), 4));
        compute_offset(
            uv1,
            uv2,
            uv0,
            L12,
            L01,
            L02,
            l02,
            span<Scalar, 4>(u_offsets.data() + 4, 4),
            span<Scalar, 4>(v_offsets.data() + 4, 4));
        compute_offset(
            uv2,
            uv0,
            uv1,
            L02,
            L12,
            L01,
            l01,
            span<Scalar, 4>(u_offsets.data() + 8, 4),
            span<Scalar, 4>(v_offsets.data() + 8, 4));

        return std::make_pair(u_offsets, v_offsets);
    };

    // Function to compute the u-scaling factor
    auto compute_scaling_factor = [&](Index fid) {
        auto p0 = vertices.row(facets(fid, 0)).eval();
        auto p1 = vertices.row(facets(fid, 1)).eval();
        auto p2 = vertices.row(facets(fid, 2)).eval();
        auto uv0 = get_uv(fid, 0);
        auto uv1 = get_uv(fid, 1);
        auto uv2 = get_uv(fid, 2);

        logger().trace("p0: [{}, {}, {}]", p0[0], p0[1], p0[2]);
        logger().trace("p1: [{}, {}, {}]", p1[0], p1[1], p1[2]);
        logger().trace("p2: [{}, {}, {}]", p2[0], p2[1], p2[2]);
        logger().trace("uv0: [{}, {}]", uv0[0], uv0[1]);
        logger().trace("uv1: [{}, {}]", uv1[0], uv1[1]);
        logger().trace("uv2: [{}, {}]", uv2[0], uv2[1]);

        Scalar a = uv1[0] - uv0[0];
        Scalar b = uv1[1] - uv0[1];
        Scalar c = uv2[0] - uv0[0];
        Scalar d = uv2[1] - uv0[1];
        Scalar e = uv2[0] - uv1[0];
        Scalar f = uv2[1] - uv1[1];
        logger().trace("a: {}, b: {}, c: {}, d: {}, e: {}, f: {}", a, b, c, d, e, f);
        la_debug_assert(a * d != b * c, "UV triangle is degenerate");

        // Compute 3D edge lengths
        Scalar L01 = (p1 - p0).squaredNorm();
        Scalar L02 = (p2 - p0).squaredNorm();
        Scalar L12 = (p2 - p1).squaredNorm();
        logger().trace("L01: {}, L02: {}, L12: {}", L01, L02, L12);

        // Compute scaling factor s
        Scalar n0 = (L01 * d * d) - (L02 * b * b);
        Scalar d0 = (L02 * a * a) - (L01 * c * c);
        Scalar n1 = (L01 * f * f) - (L12 * b * b);
        Scalar d1 = (L12 * a * a) - (L01 * e * e);
        Scalar n2 = (L02 * f * f) - (L12 * d * d);
        Scalar d2 = (L12 * c * c) - (L02 * e * e);

        Index n = 0;
        Scalar s = 0;

        if (std::isfinite(n0 / d0) && n0 / d0 > 0) {
            s += n0 / d0;
            n++;
        }
        if (std::isfinite(n1 / d1) && n1 / d1 > 0) {
            s += n1 / d1;
            n++;
        }
        if (std::isfinite(n2 / d2) && n2 / d2 > 0) {
            s += n2 / d2;
            n++;
        }

        // Ensure the denominator is not zero or negative to avoid invalid sqrt
        if (n == 0 || s <= 0) {
            logger().warn("Invalid geometry or input data. Squared scale is non-positive: {}", s);
            return invalid<Scalar>(); // Indicate an error
        }

        la_debug_assert(std::isfinite(s / n));
        la_debug_assert(std::isfinite(std::sqrt(s / n)));
        return std::sqrt(s / n);
    };

    auto offset_uv = [&](Index fid) {
        auto p0 = vertices.row(facets(fid, 0)).eval();
        auto p1 = vertices.row(facets(fid, 1)).eval();
        auto p2 = vertices.row(facets(fid, 2)).eval();

        auto uv0 = get_uv(fid, 0);
        auto uv1 = get_uv(fid, 1);
        auto uv2 = get_uv(fid, 2);

        auto [u_offsets, v_offsets] = compute_offset_amount(fid);

        Eigen::Matrix<Scalar, 3, 2, Eigen::RowMajor> new_uv;

        Scalar opt_u_distortion = invalid<Scalar>();
        Scalar opt_v_distortion = invalid<Scalar>();
        Index opt_u_index = 0;
        Index opt_v_index = 0;
        Scalar opt_u_offset = invalid<Scalar>();
        Scalar opt_v_offset = invalid<Scalar>();

        for (Index i = 0; i < 12; i++) {
            Scalar u_offset = u_offsets[i];
            Scalar v_offset = v_offsets[i];
            Index idx = i / 4;

            new_uv.row(0) = uv0;
            new_uv.row(1) = uv1;
            new_uv.row(2) = uv2;

            Scalar distortion_u, distortion_v;

            new_uv(idx, 0) += u_offset;
            distortion_u = triangle_uv_distortion<DistortionMetric::AreaRatio, Scalar>(
                span<Scalar, 3>(p0.data(), 3),
                span<Scalar, 3>(p1.data(), 3),
                span<Scalar, 3>(p2.data(), 3),
                span<Scalar, 2>(new_uv.data(), 2),
                span<Scalar, 2>(new_uv.data() + 2, 2),
                span<Scalar, 2>(new_uv.data() + 4, 2));
            new_uv(idx, 0) -= u_offset;
            la_debug_assert(std::isfinite(distortion_u));
            distortion_u = std::abs(distortion_u - 1);

            new_uv(idx, 1) += v_offset;
            distortion_v = triangle_uv_distortion<DistortionMetric::AreaRatio, Scalar>(
                span<Scalar, 3>(p0.data(), 3),
                span<Scalar, 3>(p1.data(), 3),
                span<Scalar, 3>(p2.data(), 3),
                span<Scalar, 2>(new_uv.data(), 2),
                span<Scalar, 2>(new_uv.data() + 2, 2),
                span<Scalar, 2>(new_uv.data() + 4, 2));
            la_debug_assert(std::isfinite(distortion_v));
            distortion_v = std::abs(distortion_v - 1);

            if (std::isfinite(distortion_u) && distortion_u > 0 &&
                distortion_u < opt_u_distortion) {
                opt_u_distortion = distortion_u;
                opt_u_index = idx;
                opt_u_offset = u_offset;
            }
            if (std::isfinite(distortion_v) && distortion_v > 0 &&
                distortion_v < opt_v_distortion) {
                opt_v_distortion = distortion_v;
                opt_v_index = idx;
                opt_v_offset = v_offset;
            }
        }
        logger().debug(
            "fid: {}, opt_u_distortion: {}, opt_v_distortion: {}",
            fid,
            opt_u_distortion,
            opt_v_distortion);

        la_debug_assert(opt_u_distortion > 0);
        if (std::min(opt_u_distortion, opt_v_distortion) > 0.5) {
            return false;
        }
        if (opt_u_distortion < opt_v_distortion) {
            la_debug_assert(opt_u_distortion != invalid<Scalar>());
            la_debug_assert(opt_u_distortion > 0);
            switch (opt_u_index) {
            case 0:
                uv0[0] += opt_u_offset;
                update_uv(fid, 0, uv0);
                break;
            case 1:
                uv1[0] += opt_u_offset;
                update_uv(fid, 1, uv1);
                break;
            case 2:
                uv2[0] += opt_u_offset;
                update_uv(fid, 2, uv2);
                break;
            }
        } else {
            la_debug_assert(opt_v_distortion != invalid<Scalar>());
            la_debug_assert(opt_v_distortion > 0);
            switch (opt_v_index) {
            case 0:
                uv0[1] += opt_v_offset;
                update_uv(fid, 0, uv0);
                break;
            case 1:
                uv1[1] += opt_v_offset;
                update_uv(fid, 1, uv1);
                break;
            case 2:
                uv2[1] += opt_v_offset;
                update_uv(fid, 2, uv2);
                break;
            }
        }
        return true;
    };

    auto rescale_uv = [&](Index fid, Scalar sign) {
        auto uv0 = get_uv(fid, 0);
        auto uv1 = get_uv(fid, 1);
        auto uv2 = get_uv(fid, 2);

        Scalar s = compute_scaling_factor(fid);
        logger().trace("s: {}", s);
        if (s == invalid<Scalar>()) {
            logger().warn("Invalid scaling factors. Skipping facet {}", fid);
            return;
        }

        la_debug_assert(std::isfinite(s));

        auto [min_u, max_u] = std::minmax({uv0[0], uv1[0], uv2[0]});
        auto [min_v, max_v] = std::minmax({uv0[1], uv1[1], uv2[1]});

        if (std::abs(s) < 1) {
            uv0[0] = max_u + (uv0[0] - max_u) * s * sign;
            uv1[0] = max_u + (uv1[0] - max_u) * s * sign;
            uv2[0] = max_u + (uv2[0] - max_u) * s * sign;
        } else {
            uv0[1] = max_v + (uv0[1] - max_v) / s * sign;
            uv1[1] = max_v + (uv1[1] - max_v) / s * sign;
            uv2[1] = max_v + (uv2[1] - max_v) / s * sign;
        }

        update_uv(fid, 0, uv0);
        update_uv(fid, 1, uv1);
        update_uv(fid, 2, uv2);
    };

    auto correct_uv_triangle = [&](Index fid, Scalar sign) {
        bool offset_success = offset_uv(fid);
        if (!offset_success) {
            rescale_uv(fid, sign);
        }
    };

    const Index num_facets = mesh.get_num_facets();

    for (Index fid = 0; fid < num_facets; fid++) {
        auto p0 = vertices.row(facets(fid, 0)).eval();
        auto p1 = vertices.row(facets(fid, 1)).eval();
        auto p2 = vertices.row(facets(fid, 2)).eval();

        auto uv0 = get_uv(fid, 0);
        auto uv1 = get_uv(fid, 1);
        auto uv2 = get_uv(fid, 2);

        lagrange::logger().trace("uv0: [{}, {}]", uv0[0], uv0[1]);
        lagrange::logger().trace("uv1: [{}, {}]", uv1[0], uv1[1]);
        lagrange::logger().trace("uv2: [{}, {}]", uv2[0], uv2[1]);

        Scalar ori_distortion = triangle_uv_distortion<DistortionMetric::MIPS, Scalar>(
            span<const Scalar, 3>(p0.data(), 3),
            span<const Scalar, 3>(p1.data(), 3),
            span<const Scalar, 3>(p2.data(), 3),
            span<const Scalar, 2>(uv0.data(), 2),
            span<const Scalar, 2>(uv1.data(), 2),
            span<const Scalar, 2>(uv2.data(), 2));

        if (std::isfinite(ori_distortion) && (ori_distortion < 0 || ori_distortion > 50)) {
            // Either UV triangle is flipped or has a very large distortion.
            logger().trace("Flipping facet {}", fid);
            correct_uv_triangle(fid, -1);
        }
    }

    uv_values_attr.insert_elements({additional_uv_values.data(), additional_uv_values.size()});
    la_runtime_assert(matrix_view(uv_values_attr).array().isFinite().all());

    weld_indexed_attribute(mesh, uv_attr_id);
}

#define LA_X_unflip_uv_triangles(_, Scalar, Index)    \
    template void unflip_uv_triangles<Scalar, Index>( \
        SurfaceMesh<Scalar, Index>&,                  \
        const UnflipUVOptions&);
LA_SURFACE_MESH_X(unflip_uv_triangles, 0)

} // namespace lagrange
