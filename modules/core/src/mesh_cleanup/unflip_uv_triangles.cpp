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
#include <lagrange/views.h>
#include <lagrange/weld_indexed_attribute.h>

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

    // Function to compute the u-scaling factor
    auto compute_u_scaling_factor = [&](Index fid) {
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

        // Compute 3D edge lengths
        Scalar L01 = (p1 - p0).squaredNorm();
        Scalar L02 = (p2 - p0).squaredNorm();
        Scalar L12 = (p2 - p1).squaredNorm();
        logger().trace("L01: {}, L02: {}, L12: {}", L01, L02, L12);

        // Compute similarity ratio R
        Scalar R0 = L01 / L02;
        Scalar R1 = L01 / L12;
        Scalar R2 = L02 / L12;
        logger().trace("R0: {}, R1: {}, R2: {}", R0, R1, R2);

        // Compute scaling factor s
        Scalar n0 = (R0 * d * d) - (b * b);
        Scalar d0 = (a * a) - (R0 * c * c);
        Scalar n1 = (R1 * f * f) - (b * b);
        Scalar d1 = (a * a) - (R1 * e * e);
        Scalar n2 = (R2 * f * f) - (d * d);
        Scalar d2 = (c * c) - (R2 * e * e);

        logger().trace("s^2: ({} + {} + {}) / 3", n0 / d0, n1 / d1, n2 / d2);

        Scalar s = 0;
        Index n = 0;
        if (n0 / d0 > 0) {
            s += n0 / d0;
            n++;
        }
        if (n1 / d1 > 0) {
            s += n1 / d1;
            n++;
        }
        if (n2 / d2 > 0) {
            s += n2 / d2;
            n++;
        }
        s /= n;

        // Ensure the denominator is not zero or negative to avoid invalid sqrt
        if (s <= 0) {
            logger().warn(
                "Invalid geometry or input data. Squared scale is non-positive: {}",
                s);
            return static_cast<Scalar>(-1.0); // Indicate an error
        }

        return -std::sqrt(s);
    };

    // Function to compute the v-scaling factor
    auto compute_v_scaling_factor = [&](Index fid) {
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

        // Compute 3D edge lengths
        Scalar L01 = (p1 - p0).squaredNorm();
        Scalar L02 = (p2 - p0).squaredNorm();
        Scalar L12 = (p2 - p1).squaredNorm();
        logger().trace("L01: {}, L02: {}, L12: {}", L01, L02, L12);

        // Compute similarity ratio R
        Scalar R0 = L01 / L02;
        Scalar R1 = L01 / L12;
        Scalar R2 = L02 / L12;
        logger().trace("R0: {}, R1: {}, R2: {}", R0, R1, R2);

        // Compute scaling factor s
        Scalar d0 = (R0 * d * d) - (b * b);
        Scalar n0 = (a * a) - (R0 * c * c);
        Scalar d1 = (R1 * f * f) - (b * b);
        Scalar n1 = (a * a) - (R1 * e * e);
        Scalar d2 = (R2 * f * f) - (d * d);
        Scalar n2 = (c * c) - (R2 * e * e);

        logger().trace("s^2: ({} + {} + {}) / 3", n0 / d0, n1 / d1, n2 / d2);

        Scalar s = 0;
        Index n = 0;
        if (n0 / d0 > 0) {
            s += n0 / d0;
            n++;
        }
        if (n1 / d1 > 0) {
            s += n1 / d1;
            n++;
        }
        if (n2 / d2 > 0) {
            s += n2 / d2;
            n++;
        }
        s /= n;

        // Ensure the denominator is not zero or negative to avoid invalid sqrt
        if (s <= 0) {
            logger().warn(
                "Invalid geometry or input data. Squared scale is non-positive: {}",
                s);
            return static_cast<Scalar>(-1.0); // Indicate an error
        }

        return -std::sqrt(s);
    };


    auto unflip = [&](Index fid) {
        Scalar su = compute_u_scaling_factor(fid);
        Scalar sv = compute_v_scaling_factor(fid);
        logger().trace("su: {}, sv: {}", su, sv);

        auto uv0 = get_uv(fid, 0);
        auto uv1 = get_uv(fid, 1);
        auto uv2 = get_uv(fid, 2);

        auto [min_u, max_u] = std::minmax({uv0[0], uv1[0], uv2[0]});
        auto [min_v, max_v] = std::minmax({uv0[1], uv1[1], uv2[1]});

        if (std::abs(su) < std::abs(sv)) {
            uv0[0] = max_u + (uv0[0] - max_u) * su;
            uv1[0] = max_u + (uv1[0] - max_u) * su;
            uv2[0] = max_u + (uv2[0] - max_u) * su;
        } else {
            uv0[1] = max_v + (uv0[1] - max_v) * sv;
            uv1[1] = max_v + (uv1[1] - max_v) * sv;
            uv2[1] = max_v + (uv2[1] - max_v) * sv;
        }

        update_uv(fid, 0, uv0);
        update_uv(fid, 1, uv1);
        update_uv(fid, 2, uv2);
    };

    auto is_flipped = [&](const auto& uv0, const auto& uv1, const auto& uv2) {
        Scalar area = uv0[0] * uv1[1] - uv0[1] * uv1[0] + uv1[0] * uv2[1] - uv1[1] * uv2[0] +
                      uv2[0] * uv0[1] - uv2[1] * uv0[0];
        return area < 0;
    };

    const Index num_facets = mesh.get_num_facets();
    for (Index fid = 0; fid < num_facets; fid++) {
        auto uv0 = get_uv(fid, 0);
        auto uv1 = get_uv(fid, 1);
        auto uv2 = get_uv(fid, 2);

        if (is_flipped(uv0, uv1, uv2)) {
            logger().trace("Flipping facet {}", fid);
            unflip(fid);
        }
    }

    uv_values_attr.insert_elements({additional_uv_values.data(), additional_uv_values.size()});

    weld_indexed_attribute(mesh, uv_attr_id);
}

#define LA_X_unflip_uv_triangles(_, Scalar, Index)    \
    template void unflip_uv_triangles<Scalar, Index>( \
        SurfaceMesh<Scalar, Index>&,                  \
        const UnflipUVOptions&);
LA_SURFACE_MESH_X(unflip_uv_triangles, 0)

} // namespace lagrange
