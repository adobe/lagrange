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

#include <lagrange/IndexedAttribute.h>
#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/bvh/weld_vertices.h>
#include <lagrange/combine_meshes.h>
#include <lagrange/internal/constants.h>
#include <lagrange/mesh_cleanup/remove_duplicate_vertices.h>
#include <lagrange/packing/repack_uv_charts.h>
#include <lagrange/primitive/SemanticLabel.h>
#include <lagrange/primitive/SweepOptions.h>
#include <lagrange/primitive/api.h>
#include <lagrange/primitive/generate_disc.h>
#include <lagrange/primitive/generate_swept_surface.h>
#include <lagrange/primitive/generate_torus.h>
#include <lagrange/transform_mesh.h>
#include <lagrange/utils/assert.h>
#include <lagrange/uv_mesh.h>
#include <lagrange/views.h>

#include "primitive_utils.h"

#include <vector>

#include <Eigen/Core>

namespace lagrange::primitive {


template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> generate_torus(TorusOptions setting)
{
    setting.project_to_valid_range();
    auto profile = generate_ring<Scalar>(
        static_cast<Scalar>(setting.minor_radius),
        static_cast<size_t>(setting.pipe_segments),
        static_cast<Scalar>(lagrange::internal::pi));
    auto sweep_setting = SweepOptions<Scalar>::circular_sweep(
        {
            setting.major_radius,
            0,
            0,
        }, // A point on the center line of the torus
        {0, -1, 0} // Axis of rotation
    );
    sweep_setting.set_num_samples(setting.ring_segments + 1);

    // Set domain
    Scalar t_begin = static_cast<Scalar>(setting.start_sweep_angle / (2 * lagrange::internal::pi));
    Scalar t_end = static_cast<Scalar>(setting.end_sweep_angle / (2 * lagrange::internal::pi));
    sweep_setting.set_domain({t_begin, t_end});

    std::vector<SurfaceMesh<Scalar, Index>> parts;
    parts.reserve(3);
    if (!sweep_setting.is_closed() && (setting.with_top_cap || setting.with_bottom_cap)) {
        DiscOptions disc_setting;
        disc_setting.radius = setting.minor_radius;
        disc_setting.radial_sections = setting.pipe_segments;
        auto disc = generate_disc<Scalar, Index>(disc_setting);

        using AffineTransform = Eigen::Transform<Scalar, 3, Eigen::Affine>;
        AffineTransform rot_y, rot_z;
        rot_y.setIdentity();
        rot_y.rotate(
            Eigen::AngleAxis<Scalar>(
                static_cast<Scalar>(lagrange::internal::pi),
                Eigen::Matrix<Scalar, 3, 1>::UnitY()));
        rot_z.setIdentity();
        rot_z.rotate(
            Eigen::AngleAxis<Scalar>(
                static_cast<Scalar>(lagrange::internal::pi),
                Eigen::Matrix<Scalar, 3, 1>::UnitZ()));

        AffineTransform transform_begin = sweep_setting.sample_transform(t_begin);
        AffineTransform transform_end = sweep_setting.sample_transform(t_end);

        auto top_cap = transformed_mesh(disc, transform_begin * rot_y);
        auto bottom_cap = transformed_mesh(disc, transform_end * rot_z);

        add_semantic_label(top_cap, setting.semantic_label_attribute_name, SemanticLabel::Top);
        add_semantic_label(
            bottom_cap,
            setting.semantic_label_attribute_name,
            SemanticLabel::Bottom);

        if (setting.fixed_uv) {
            normalize_uv(
                top_cap,
                {setting.uv_padding, 0.5 + setting.uv_padding},
                {0.5 - setting.uv_padding, 1 - setting.uv_padding});
            normalize_uv(
                bottom_cap,
                {0.5 + setting.uv_padding, 0.5 + setting.uv_padding},
                {1 - setting.uv_padding, 1 - setting.uv_padding});
        }

        if (setting.with_top_cap) parts.push_back(std::move(top_cap));
        if (setting.with_bottom_cap) parts.push_back(std::move(bottom_cap));
    }

    SweptSurfaceOptions sweep_options;
    sweep_options.uv_attribute_name = setting.uv_attribute_name;
    sweep_options.normal_attribute_name = setting.normal_attribute_name;
    sweep_options.triangulate = setting.triangulate;
    sweep_options.angle_threshold = setting.angle_threshold;
    sweep_options.profile_angle_threshold = setting.angle_threshold;
    sweep_options.fixed_uv = setting.fixed_uv;
    sweep_options.use_u_as_profile_length = false;
    sweep_options.longitude_attribute_name = "";
    sweep_options.latitude_attribute_name = "";
    auto side = generate_swept_surface<Scalar, Index>(
        {profile.data(), profile.size()},
        sweep_setting,
        sweep_options);
    if (setting.fixed_uv) {
        // Note that no padding is used due to periodic nature of the patch.
        normalize_uv(side, {0, 0}, {1, 0.5});
    }
    add_semantic_label(side, setting.semantic_label_attribute_name, SemanticLabel::Side);
    parts.push_back(std::move(side));

    auto mesh = combine_meshes<Scalar, Index>({parts.data(), parts.size()}, true);

    bvh::WeldOptions weld_options;
    weld_options.boundary_only = true;
    weld_options.radius = setting.dist_threshold;
    bvh::weld_vertices(mesh, weld_options);

    if (!setting.fixed_uv) {
        packing::RepackOptions repack_options;
        repack_options.margin = setting.uv_padding;
        packing::repack_uv_charts(mesh);
    }
    center_mesh(mesh, setting.center);

    return mesh;
}

#define LA_X_generate_torus(_, Scalar, Index)                                           \
    template LA_PRIMITIVE_API SurfaceMesh<Scalar, Index> generate_torus<Scalar, Index>( \
        TorusOptions);

LA_SURFACE_MESH_X(generate_torus, 0)
} // namespace lagrange::primitive
