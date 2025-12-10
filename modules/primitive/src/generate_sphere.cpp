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
#include <lagrange/mesh_cleanup/remove_degenerate_facets.h>
#include <lagrange/primitive/SweepOptions.h>
#include <lagrange/primitive/api.h>
#include <lagrange/primitive/generate_disc.h>
#include <lagrange/primitive/generate_sphere.h>
#include <lagrange/primitive/generate_swept_surface.h>
#include <lagrange/transform_mesh.h>
#include <lagrange/utils/SmallVector.h>
#include <lagrange/utils/assert.h>
#include <lagrange/weld_indexed_attribute.h>

#include "primitive_utils.h"

namespace lagrange::primitive {

namespace {

template <typename Scalar>
Eigen::Matrix<Scalar, Eigen::Dynamic, 2, Eigen::RowMajor> generate_profile(
    const SphereOptions& setting)
{
    using PointArray = Eigen::Matrix<Scalar, Eigen::Dynamic, 2, Eigen::RowMajor>;

    const size_t num_segments = setting.num_longitude_sections;
    la_debug_assert(num_segments >= 3, "Number of longitude sections must be at least 3.");

    PointArray profile(num_segments + 1, 2);
    for (size_t i = 0; i <= num_segments; ++i) {
        Scalar theta = static_cast<Scalar>(
            (lagrange::internal::pi * i) / static_cast<Scalar>(num_segments) -
            lagrange::internal::pi / 2);
        profile(i, 0) = setting.radius * std::cos(theta); // X coordinate
        profile(i, 1) = setting.radius * std::sin(theta); // Y coordinate
    }
    return profile;
}

} // namespace

template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> generate_sphere(SphereOptions setting)
{
    setting.project_to_valid_range();

    auto profile = generate_profile<Scalar>(setting);

    auto sweep_setting = SweepOptions<Scalar>::circular_sweep(
        {setting.radius, 0, 0}, // A point on the center line of the sphere
        {0, -1, 0} // Axis of rotation
    );
    sweep_setting.set_pivot({setting.radius, 0, 0}); // Set pivot at a point on the unit circle
    sweep_setting.set_num_samples(setting.num_latitude_sections + 1);

    Scalar t_begin = static_cast<Scalar>(setting.start_sweep_angle / (2 * lagrange::internal::pi));
    Scalar t_end = static_cast<Scalar>(setting.end_sweep_angle / (2 * lagrange::internal::pi));
    sweep_setting.set_domain({t_begin, t_end});

    SmallVector<SurfaceMesh<Scalar, Index>, 3> parts;

    // Side
    {
        SweptSurfaceOptions sweep_options;
        sweep_options.uv_attribute_name = setting.uv_attribute_name;
        sweep_options.normal_attribute_name = setting.normal_attribute_name;
        sweep_options.longitude_attribute_name = "";
        sweep_options.latitude_attribute_name = "";

        sweep_options.triangulate = setting.triangulate;
        sweep_options.angle_threshold = setting.angle_threshold;
        sweep_options.profile_angle_threshold = setting.angle_threshold;
        sweep_options.use_u_as_profile_length = false;

        auto side = generate_swept_surface<Scalar, Index>(
            {profile.data(), static_cast<size_t>(profile.size())},
            sweep_setting,
            sweep_options);
        add_semantic_label(side, setting.semantic_label_attribute_name, SemanticLabel::Side);

        if (setting.fixed_uv) {
            normalize_uv(side, {0, 0}, {1, 0.5});
        } else {
            normalize_uv(side, {1 - t_end, 0}, {1 - t_begin, 0.5});
        }

        parts.push_back(std::move(side));
    }

    // Cross sections
    if (!sweep_setting.is_closed() && setting.with_cross_section) {
        using AffineTransform = Eigen::Transform<Scalar, 3, Eigen::Affine>;
        AffineTransform transform_begin = sweep_setting.sample_transform(t_begin);
        AffineTransform transform_end = sweep_setting.sample_transform(t_end);

        DiscOptions disc_setting;
        disc_setting.radius = setting.radius;
        disc_setting.start_angle =
            static_cast<PrimitiveOptions::Scalar>(-lagrange::internal::pi / 2);
        disc_setting.end_angle = static_cast<PrimitiveOptions::Scalar>(lagrange::internal::pi / 2);
        disc_setting.radial_sections = setting.num_longitude_sections;

        auto cross_section_end = generate_disc<Scalar, Index>(disc_setting);
        transform_mesh(cross_section_end, transform_end);

        disc_setting.start_angle =
            static_cast<PrimitiveOptions::Scalar>(lagrange::internal::pi / 2);
        disc_setting.end_angle =
            static_cast<PrimitiveOptions::Scalar>(3 * lagrange::internal::pi / 2);
        auto cross_section_begin = generate_disc<Scalar, Index>(disc_setting);
        transform_begin.rotate(
            Eigen::AngleAxis<Scalar>(
                static_cast<Scalar>(lagrange::internal::pi),
                Eigen::Matrix<Scalar, 3, 1>::UnitY()));
        transform_mesh(cross_section_begin, transform_begin);

        normalize_uv(
            cross_section_end,
            {0.5, 0.5 + setting.uv_padding},
            {0.75 - setting.uv_padding, 1 - setting.uv_padding});
        normalize_uv(
            cross_section_begin,
            {0.25 + setting.uv_padding, 0.5 + setting.uv_padding},
            {0.5, 1 - setting.uv_padding});

        add_semantic_label(
            cross_section_begin,
            setting.semantic_label_attribute_name,
            SemanticLabel::CrossSection);
        add_semantic_label(
            cross_section_end,
            setting.semantic_label_attribute_name,
            SemanticLabel::CrossSection);
        parts.push_back(std::move(cross_section_begin));
        parts.push_back(std::move(cross_section_end));
    }

    auto mesh = combine_meshes<Scalar, Index>({parts.data(), parts.size()}, true);

    // TODO: Add option to let user control whether to weld.
    bvh::WeldOptions weld_options;
    weld_options.boundary_only = true;
    weld_options.radius = setting.dist_threshold;
    bvh::weld_vertices(mesh, weld_options);

    // Weld indexed normals
    WeldOptions attr_weld_options;
    attr_weld_options.epsilon_abs = 1; // Disable distance-based check
    attr_weld_options.angle_abs = setting.angle_threshold;
    weld_indexed_attribute(
        mesh,
        mesh.get_attribute_id(setting.normal_attribute_name),
        attr_weld_options);

    if (setting.triangulate) {
        remove_degenerate_facets(mesh);
    }

    // Move center of sphere to origin
    Eigen::Transform<Scalar, 3, Eigen::Affine> transform;
    transform.setIdentity();
    transform.translate(Eigen::Matrix<Scalar, 3, 1>(-setting.radius, 0, 0));
    transform_mesh(mesh, transform);
    center_mesh(mesh, setting.center);

    return mesh;
}

#define LA_X_generate_sphere(_, Scalar, Index)                                           \
    template LA_PRIMITIVE_API SurfaceMesh<Scalar, Index> generate_sphere<Scalar, Index>( \
        SphereOptions);

LA_SURFACE_MESH_X(generate_sphere, 0)

} // namespace lagrange::primitive
