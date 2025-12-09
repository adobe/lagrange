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

#include <lagrange/Logger.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/bvh/weld_vertices.h>
#include <lagrange/combine_meshes.h>
#include <lagrange/compute_facet_normal.h>
#include <lagrange/map_attribute.h>
#include <lagrange/primitive/api.h>
#include <lagrange/primitive/generate_rounded_cube.h>
#include <lagrange/transform_mesh.h>
#include <lagrange/triangulate_polygonal_facets.h>
#include <lagrange/utils/SmallVector.h>
#include <lagrange/utils/assert.h>
#include <lagrange/views.h>

#include "primitive_utils.h"

namespace lagrange::primitive {

namespace {

bool bevel_is_sharp(const RoundedCubeOptions& setting)
{
    using Scalar = RoundedCubeOptions::Scalar;
    return static_cast<Scalar>(lagrange::internal::pi / (2 * setting.bevel_segments)) >=
           static_cast<Scalar>(setting.angle_threshold);
}

template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> generate_rounded_corner(
    const RoundedCubeOptions& setting,
    const Eigen::Transform<Scalar, 3, Eigen::Affine>& transformation,
    const Eigen::Transform<Scalar, 2, Eigen::Affine>& uv_transformation)
{
    Scalar radius = setting.bevel_radius;
    Scalar num_segments = setting.bevel_segments;

    SurfaceMesh<Scalar, Index> mesh;
    Index num_vertices = (num_segments + 2) * (num_segments + 1) / 2;
    Index num_facets = num_segments * num_segments;

    mesh.add_vertices(num_vertices);
    mesh.add_triangles(num_facets);

    auto vertices = vertex_ref(mesh);
    auto facets = facet_ref(mesh);
    Eigen::Matrix<Scalar, Eigen::Dynamic, 2, Eigen::RowMajor> uvs(num_vertices, 2);
    Eigen::Matrix<Scalar, Eigen::Dynamic, 3, Eigen::RowMajor> normals(num_vertices, 3);

    Index count = 0;
    for (Index i = 0; i <= num_segments; i++) {
        for (Index j = 0; j <= num_segments - i; j++) {
            Scalar theta = static_cast<Scalar>(
                (i == num_segments) ? lagrange::internal::pi / 4
                                    : (j * lagrange::internal::pi) / (2 * (num_segments - i)));
            Scalar phi = static_cast<Scalar>(
                lagrange::internal::pi / 2 -
                (num_segments - i) * lagrange::internal::pi / (2 * num_segments));

            normals.row(count) << sin(theta) * cos(phi), sin(phi), cos(theta) * cos(phi);
            uvs.row(count) << radius * theta, radius * phi;
            count++;
        }
    }
    vertices = normals * radius;
    la_debug_assert(count == num_vertices);

    uvs.template leftCols<2>().transpose() =
        uv_transformation * uvs.template leftCols<2>().transpose();

    count = 0;
    Index prev_base = 0;
    for (Index i = 0; i < num_segments; i++) {
        Index next_base = prev_base + num_segments - i + 1;
        for (Index j = 0; j < num_segments - i; j++) {
            facets.row(count++) << prev_base + j, prev_base + j + 1, next_base + j;
            if (j + 1 < num_segments - i) {
                facets.row(count++) << next_base + j, prev_base + j + 1, next_base + j + 1;
            }
        }
        prev_base = next_base;
    }
    la_debug_assert(count == num_facets);

    mesh.template create_attribute<Scalar>(
        setting.uv_attribute_name,
        AttributeElement::Indexed,
        2,
        AttributeUsage::UV,
        {uvs.data(), static_cast<size_t>(uvs.size())},
        {facets.data(), static_cast<size_t>(facets.size())});

    bool sharp_normal = bevel_is_sharp(setting);
    if (sharp_normal) {
        FacetNormalOptions options;
        options.output_attribute_name = setting.normal_attribute_name;
        compute_facet_normal(mesh, options);
        map_attribute_in_place(mesh, setting.normal_attribute_name, AttributeElement::Indexed);
    } else {
        mesh.template create_attribute<Scalar>(
            setting.normal_attribute_name,
            AttributeElement::Indexed,
            3,
            AttributeUsage::Normal,
            {normals.data(), static_cast<size_t>(normals.size())},
            {facets.data(), static_cast<size_t>(facets.size())});
    }

    transform_mesh(mesh, transformation);

    add_semantic_label(mesh, setting.semantic_label_attribute_name, SemanticLabel::Bevel);
    return mesh;
}

template <typename Scalar, typename Index, typename Vector>
void generate_corners(const RoundedCubeOptions& setting, Vector& parts)
{
    using Vector3S = Eigen::Matrix<Scalar, 3, 1>;
    using Vector2S = Eigen::Matrix<Scalar, 2, 1>;
    using Transform3 = Eigen::Transform<Scalar, 3, Eigen::Affine>;
    using Transform2 = Eigen::Transform<Scalar, 2, Eigen::Affine>;

    const Scalar w = setting.width - 2 * setting.bevel_radius;
    const Scalar h = setting.height - 2 * setting.bevel_radius;
    const Scalar d = setting.depth - 2 * setting.bevel_radius;
    const Scalar t = static_cast<Scalar>(setting.bevel_radius * lagrange::internal::pi / 2);
    const Scalar s = std::max(2 * d + 2 * w + 4 * t, 2 * d + 2 * t + h);

    Transform3 transformation;
    Transform2 uv_transformation;

    // +X, +Y, +Z corner
    transformation.setIdentity();
    transformation.translate(Vector3S(w / 2, h / 2, d / 2));
    uv_transformation.setIdentity();
    uv_transformation.scale(1.0f / s);
    uv_transformation.translate(Vector2S(d + t + w, d + t + h));
    parts.push_back(
        generate_rounded_corner<Scalar, Index>(setting, transformation, uv_transformation));

    // +X +Y -Z corner
    transformation.setIdentity();
    transformation.translate(Vector3S(w / 2, h / 2, -d / 2));
    transformation.rotate(
        Eigen::AngleAxis<Scalar>((Scalar)(0.5 * lagrange::internal::pi), Vector3S::UnitY()));
    uv_transformation.setIdentity();
    uv_transformation.scale(1.0f / s);
    uv_transformation.translate(Vector2S(2 * d + 2 * t + w, d + t + h));
    parts.push_back(
        generate_rounded_corner<Scalar, Index>(setting, transformation, uv_transformation));

    // -X +Y -Z corner
    transformation.setIdentity();
    transformation.translate(Vector3S(-w / 2, h / 2, -d / 2));
    transformation.rotate(
        Eigen::AngleAxis<Scalar>((Scalar)lagrange::internal::pi, Vector3S::UnitY()));
    uv_transformation.setIdentity();
    uv_transformation.scale(1.0f / s);
    uv_transformation.translate(Vector2S(2 * d + 3 * t + 2 * w, d + t + h));
    parts.push_back(
        generate_rounded_corner<Scalar, Index>(setting, transformation, uv_transformation));

    // -X +Y +Z corner
    transformation.setIdentity();
    transformation.translate(Vector3S(-w / 2, h / 2, d / 2));
    transformation.rotate(
        Eigen::AngleAxis<Scalar>((Scalar)(1.5 * lagrange::internal::pi), Vector3S::UnitY()));
    uv_transformation.setIdentity();
    uv_transformation.scale(1.0f / s);
    uv_transformation.translate(Vector2S(d, d + t + h));
    parts.push_back(
        generate_rounded_corner<Scalar, Index>(setting, transformation, uv_transformation));

    // -X -Y +Z corner
    transformation.setIdentity();
    transformation.translate(Vector3S(-w / 2, -h / 2, d / 2));
    transformation.rotate(
        Eigen::AngleAxis<Scalar>((Scalar)lagrange::internal::pi, Vector3S::UnitZ()));
    uv_transformation.setIdentity();
    uv_transformation.scale(1.0f / s);
    uv_transformation.translate(Vector2S(d + t, d + t));
    uv_transformation.rotate(lagrange::internal::pi);
    parts.push_back(
        generate_rounded_corner<Scalar, Index>(setting, transformation, uv_transformation));

    // -X -Y -Z corner
    transformation.setIdentity();
    transformation.translate(Vector3S(-w / 2, -h / 2, -d / 2));
    transformation.rotate(
        Eigen::AngleAxis<Scalar>((Scalar)lagrange::internal::pi, Vector3S::UnitZ()));
    transformation.rotate(
        Eigen::AngleAxis<Scalar>((Scalar)(0.5 * lagrange::internal::pi), Vector3S::UnitY()));
    uv_transformation.setIdentity();
    uv_transformation.scale(1.0f / s);
    uv_transformation.translate(Vector2S(2 * d + 4 * t + 2 * w, d + t));
    uv_transformation.rotate(lagrange::internal::pi);
    parts.push_back(
        generate_rounded_corner<Scalar, Index>(setting, transformation, uv_transformation));

    // X -Y -Z corner
    transformation.setIdentity();
    transformation.translate(Vector3S(w / 2, -h / 2, -d / 2));
    transformation.rotate(
        Eigen::AngleAxis<Scalar>((Scalar)lagrange::internal::pi, Vector3S::UnitZ()));
    transformation.rotate(
        Eigen::AngleAxis<Scalar>((Scalar)lagrange::internal::pi, Vector3S::UnitY()));
    uv_transformation.setIdentity();
    uv_transformation.scale(1.0f / s);
    uv_transformation.translate(Vector2S(2 * d + 3 * t + w, d + t));
    uv_transformation.rotate(lagrange::internal::pi);
    parts.push_back(
        generate_rounded_corner<Scalar, Index>(setting, transformation, uv_transformation));

    // X -Y Z corner
    transformation.setIdentity();
    transformation.translate(Vector3S(w / 2, -h / 2, d / 2));
    transformation.rotate(
        Eigen::AngleAxis<Scalar>((Scalar)lagrange::internal::pi, Vector3S::UnitZ()));
    transformation.rotate(
        Eigen::AngleAxis<Scalar>((Scalar)(1.5 * lagrange::internal::pi), Vector3S::UnitY()));
    uv_transformation.setIdentity();
    uv_transformation.scale(1.0f / s);
    uv_transformation.translate(Vector2S(d + 2 * t + w, d + t));
    uv_transformation.rotate(lagrange::internal::pi);
    parts.push_back(
        generate_rounded_corner<Scalar, Index>(setting, transformation, uv_transformation));
}

template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> generate_rounded_edge(
    const RoundedCubeOptions& setting,
    Scalar edge_length,
    Index edge_segments,
    const Eigen::Transform<Scalar, 3, Eigen::Affine>& transformation,
    const Eigen::Transform<Scalar, 2, Eigen::Affine>& uv_transformation)
{
    SurfaceMesh<Scalar, Index> mesh;
    Index num_vertices = (setting.bevel_segments + 1) * (edge_segments + 1);
    Index num_facets = setting.bevel_segments * edge_segments;

    mesh.add_vertices(num_vertices);
    mesh.add_quads(num_facets);

    auto vertices = vertex_ref(mesh);
    auto facets = facet_ref(mesh);
    Eigen::Matrix<Scalar, Eigen::Dynamic, 2, Eigen::RowMajor> uvs(num_vertices, 2);
    Eigen::Matrix<Scalar, Eigen::Dynamic, 3, Eigen::RowMajor> normals(num_vertices, 3);

    const Scalar r = setting.bevel_radius;

    for (Index i = 0; i <= setting.bevel_segments; i++) {
        Scalar t = static_cast<Scalar>(i) / setting.bevel_segments;
        Scalar theta = static_cast<Scalar>(lagrange::internal::pi * 0.5 * t);

        for (Index j = 0; j <= edge_segments; j++) {
            Index idx = i * (edge_segments + 1) + j;
            auto lj = edge_length * j / static_cast<Scalar>(edge_segments);
            vertices.row(idx) << r * std::sin(theta), lj, r * std::cos(theta);
            normals.row(idx) << std::sin(theta), 0, std::cos(theta);
            uvs.row(idx) << r * theta, lj;
        }
    }

    uvs.template leftCols<2>().transpose() =
        uv_transformation * uvs.template leftCols<2>().transpose();

    for (Index i = 0; i < setting.bevel_segments; i++) {
        for (Index j = 0; j < edge_segments; j++) {
            Index v0 = i * (edge_segments + 1) + j;
            Index v1 = (i + 1) * (edge_segments + 1) + j;
            Index v2 = (i + 1) * (edge_segments + 1) + j + 1;
            Index v3 = i * (edge_segments + 1) + j + 1;

            Index idx = i * edge_segments + j;
            facets.row(idx) << v0, v1, v2, v3;
        }
    }

    mesh.template create_attribute<Scalar>(
        setting.uv_attribute_name,
        AttributeElement::Indexed,
        2,
        AttributeUsage::UV,
        {uvs.data(), static_cast<size_t>(uvs.size())},
        {facets.data(), static_cast<size_t>(facets.size())});

    bool sharp_normal = bevel_is_sharp(setting);
    if (sharp_normal) {
        FacetNormalOptions options;
        options.output_attribute_name = setting.normal_attribute_name;
        compute_facet_normal(mesh, options);
        map_attribute_in_place(mesh, setting.normal_attribute_name, AttributeElement::Indexed);
    } else {
        mesh.template create_attribute<Scalar>(
            setting.normal_attribute_name,
            AttributeElement::Indexed,
            3,
            AttributeUsage::Normal,
            {normals.data(), static_cast<size_t>(normals.size())},
            {facets.data(), static_cast<size_t>(facets.size())});
    }

    transform_mesh(mesh, transformation);

    add_semantic_label(mesh, setting.semantic_label_attribute_name, SemanticLabel::Bevel);
    return mesh;
}

template <typename Scalar, typename Index, typename Vector>
void generate_edges(const RoundedCubeOptions& setting, Vector& parts)
{
    using Vector3S = Eigen::Matrix<Scalar, 3, 1>;
    using Vector2S = Eigen::Matrix<Scalar, 2, 1>;

    const Scalar w = setting.width - 2 * setting.bevel_radius;
    const Scalar h = setting.height - 2 * setting.bevel_radius;
    const Scalar d = setting.depth - 2 * setting.bevel_radius;
    const Scalar t = static_cast<Scalar>(setting.bevel_radius * lagrange::internal::pi / 2);
    const Scalar s = std::max(2 * d + 2 * w + 4 * t, 2 * d + 2 * t + h);

    Eigen::Transform<Scalar, 3, Eigen::Affine> transformation;
    Eigen::Transform<Scalar, 2, Eigen::Affine> uv_transformation;

    if (h > setting.epsilon) {
        // +X +Z edge
        transformation.setIdentity();
        transformation.translate(Vector3S(w / 2, -h / 2, d / 2));
        uv_transformation.setIdentity();
        uv_transformation.scale(1.0f / s);
        uv_transformation.translate(Vector2S(d + t + w, d + t));
        parts.push_back(
            generate_rounded_edge<Scalar, Index>(
                setting,
                h,
                setting.height_segments,
                transformation,
                uv_transformation));

        // +X -Z edge
        transformation.setIdentity();
        transformation.translate(Vector3S(w / 2, -h / 2, -d / 2));
        transformation.rotate(
            Eigen::AngleAxis<Scalar>((Scalar)(0.5 * lagrange::internal::pi), Vector3S::UnitY()));
        uv_transformation.setIdentity();
        uv_transformation.scale(1.0f / s);
        uv_transformation.translate(Vector2S(2 * d + 2 * t + w, d + t));
        parts.push_back(
            generate_rounded_edge<Scalar, Index>(
                setting,
                h,
                setting.height_segments,
                transformation,
                uv_transformation));

        // -X -Z edge
        transformation.setIdentity();
        transformation.translate(Vector3S(-w / 2, -h / 2, -d / 2));
        transformation.rotate(
            Eigen::AngleAxis<Scalar>((Scalar)lagrange::internal::pi, Vector3S::UnitY()));
        uv_transformation.setIdentity();
        uv_transformation.scale(1.0f / s);
        uv_transformation.translate(Vector2S(2 * d + 3 * t + 2 * w, d + t));
        parts.push_back(
            generate_rounded_edge<Scalar, Index>(
                setting,
                h,
                setting.height_segments,
                transformation,
                uv_transformation));

        // -X +Z edge
        transformation.setIdentity();
        transformation.translate(Vector3S(-w / 2, -h / 2, d / 2));
        transformation.rotate(
            Eigen::AngleAxis<Scalar>((Scalar)(1.5 * lagrange::internal::pi), Vector3S::UnitY()));
        uv_transformation.setIdentity();
        uv_transformation.scale(1.0f / s);
        uv_transformation.translate(Vector2S(d, d + t));
        parts.push_back(
            generate_rounded_edge<Scalar, Index>(
                setting,
                h,
                setting.height_segments,
                transformation,
                uv_transformation));
    }

    if (w > setting.epsilon) {
        // +Y +Z edge
        transformation.setIdentity();
        transformation.translate(Vector3S(w / 2, h / 2, d / 2));
        transformation.rotate(
            Eigen::AngleAxis<Scalar>((Scalar)(0.5 * lagrange::internal::pi), Vector3S::UnitZ()));
        uv_transformation.setIdentity();
        uv_transformation.scale(1.0f / s);
        uv_transformation.translate(Vector2S(d + t + w, d + t + h));
        uv_transformation.rotate(0.5f * lagrange::internal::pi);
        parts.push_back(
            generate_rounded_edge<Scalar, Index>(
                setting,
                w,
                setting.width_segments,
                transformation,
                uv_transformation));

        // +Y -Z edge
        transformation.setIdentity();
        transformation.translate(Vector3S(w / 2, h / 2, -d / 2));
        transformation.rotate(
            Eigen::AngleAxis<Scalar>((Scalar)(-0.5 * lagrange::internal::pi), Vector3S::UnitX()));
        transformation.rotate(
            Eigen::AngleAxis<Scalar>((Scalar)(0.5 * lagrange::internal::pi), Vector3S::UnitZ()));
        uv_transformation.setIdentity();
        uv_transformation.scale(1.0f / s);
        uv_transformation.translate(Vector2S(2 * d + 3 * t + w, d + 2 * t + h));
        uv_transformation.rotate((Scalar)(-0.5 * lagrange::internal::pi));
        parts.push_back(
            generate_rounded_edge<Scalar, Index>(
                setting,
                w,
                setting.width_segments,
                transformation,
                uv_transformation));

        // -Y -Z edge
        transformation.setIdentity();
        transformation.translate(Vector3S(w / 2, -h / 2, -d / 2));
        transformation.rotate(
            Eigen::AngleAxis<Scalar>((Scalar)lagrange::internal::pi, Vector3S::UnitX()));
        transformation.rotate(
            Eigen::AngleAxis<Scalar>((Scalar)(0.5 * lagrange::internal::pi), Vector3S::UnitZ()));
        uv_transformation.setIdentity();
        uv_transformation.scale(1.0f / s);
        uv_transformation.translate(Vector2S(2 * d + 3 * t + w, d + t));
        uv_transformation.scale(-1);
        uv_transformation.rotate((Scalar)(0.5f * lagrange::internal::pi));
        parts.push_back(
            generate_rounded_edge<Scalar, Index>(
                setting,
                w,
                setting.width_segments,
                transformation,
                uv_transformation));

        // -Y +Z edge
        transformation.setIdentity();
        transformation.translate(Vector3S(w / 2, -h / 2, d / 2));
        transformation.rotate(
            Eigen::AngleAxis<Scalar>((Scalar)(0.5 * lagrange::internal::pi), Vector3S::UnitX()));
        transformation.rotate(
            Eigen::AngleAxis<Scalar>((Scalar)(0.5 * lagrange::internal::pi), Vector3S::UnitZ()));
        uv_transformation.setIdentity();
        uv_transformation.scale(1.0f / s);
        uv_transformation.translate(Vector2S(d + t + w, d));
        uv_transformation.rotate((Scalar)(0.5f * lagrange::internal::pi));
        parts.push_back(
            generate_rounded_edge<Scalar, Index>(
                setting,
                w,
                setting.width_segments,
                transformation,
                uv_transformation));
    }

    if (d > setting.epsilon) {
        // +X +Y edge
        transformation.setIdentity();
        transformation.translate(Vector3S(w / 2, h / 2, d / 2));
        transformation.rotate(
            Eigen::AngleAxis<Scalar>((Scalar)(-0.5 * lagrange::internal::pi), Vector3S::UnitX()));
        uv_transformation.setIdentity();
        uv_transformation.scale(1.0f / s);
        uv_transformation.translate(Vector2S(d + 2 * t + w, d + 2 * t + h));
        uv_transformation.rotate((Scalar)(-0.5f * lagrange::internal::pi));
        parts.push_back(
            generate_rounded_edge<Scalar, Index>(
                setting,
                d,
                setting.depth_segments,
                transformation,
                uv_transformation));

        // -X +Y edge
        transformation.setIdentity();
        transformation.translate(Vector3S(-w / 2, h / 2, d / 2));
        transformation.rotate(
            Eigen::AngleAxis<Scalar>((Scalar)(0.5 * lagrange::internal::pi), Vector3S::UnitZ()));
        transformation.rotate(
            Eigen::AngleAxis<Scalar>((Scalar)(-0.5 * lagrange::internal::pi), Vector3S::UnitX()));
        uv_transformation.setIdentity();
        uv_transformation.scale(1.0f / s);
        uv_transformation.translate(Vector2S(d, d + t + h));
        uv_transformation.rotate((Scalar)(0.5 * lagrange::internal::pi));
        parts.push_back(
            generate_rounded_edge<Scalar, Index>(
                setting,
                d,
                setting.depth_segments,
                transformation,
                uv_transformation));

        // -X -Y edge
        transformation.setIdentity();
        transformation.translate(Vector3S(-w / 2, -h / 2, d / 2));
        transformation.rotate(
            Eigen::AngleAxis<Scalar>((Scalar)lagrange::internal::pi, Vector3S::UnitZ()));
        transformation.rotate(
            Eigen::AngleAxis<Scalar>((Scalar)(-0.5 * lagrange::internal::pi), Vector3S::UnitX()));
        uv_transformation.setIdentity();
        uv_transformation.scale(1.0f / s);
        uv_transformation.translate(Vector2S(d, d));
        uv_transformation.rotate((Scalar)(0.5 * lagrange::internal::pi));
        parts.push_back(
            generate_rounded_edge<Scalar, Index>(
                setting,
                d,
                setting.depth_segments,
                transformation,
                uv_transformation));

        // +X -Y edge
        transformation.setIdentity();
        transformation.translate(Vector3S(w / 2, -h / 2, d / 2));
        transformation.rotate(
            Eigen::AngleAxis<Scalar>((Scalar)(-0.5 * lagrange::internal::pi), Vector3S::UnitZ()));
        transformation.rotate(
            Eigen::AngleAxis<Scalar>((Scalar)(-0.5 * lagrange::internal::pi), Vector3S::UnitX()));
        uv_transformation.setIdentity();
        uv_transformation.scale(1.0f / s);
        uv_transformation.translate(Vector2S(d + 2 * t + w, d + t));
        uv_transformation.rotate((Scalar)(-0.5 * lagrange::internal::pi));
        parts.push_back(
            generate_rounded_edge<Scalar, Index>(
                setting,
                d,
                setting.depth_segments,
                transformation,
                uv_transformation));
    }
}

template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> generate_flat_quad(
    const RoundedCubeOptions& setting,
    Scalar l0,
    Scalar l1,
    Index num_segments_0,
    Index num_segments_1,
    Eigen::Transform<Scalar, 3, Eigen::Affine> transformation,
    Eigen::Transform<Scalar, 2, Eigen::Affine> uv_transformation)
{
    SurfaceMesh<Scalar, Index> mesh;
    Index num_vertices = (num_segments_0 + 1) * (num_segments_1 + 1);
    Index num_facets = num_segments_0 * num_segments_1;

    mesh.add_vertices(num_vertices);
    mesh.add_quads(num_facets);

    auto vertices = vertex_ref(mesh);
    auto facets = facet_ref(mesh);
    Eigen::Matrix<Scalar, Eigen::Dynamic, 2, Eigen::RowMajor> uvs(num_vertices, 2);

    for (Index i = 0; i <= num_segments_0; i++) {
        for (Index j = 0; j <= num_segments_1; j++) {
            vertices.row(i * (num_segments_1 + 1) + j) << l0 * i / Scalar(num_segments_0),
                l1 * j / Scalar(num_segments_1), 0;
        }
    }
    uvs.transpose() = uv_transformation * vertices.template leftCols<2>().transpose();

    for (Index i = 0; i < num_segments_0; i++) {
        for (Index j = 0; j < num_segments_1; j++) {
            Index v0 = i * (num_segments_1 + 1) + j;
            Index v1 = (i + 1) * (num_segments_1 + 1) + j;
            Index v2 = (i + 1) * (num_segments_1 + 1) + j + 1;
            Index v3 = i * (num_segments_1 + 1) + j + 1;

            facets.row(i * num_segments_1 + j) << v0, v1, v2, v3;
        }
    }

    mesh.template create_attribute<Scalar>(
        setting.uv_attribute_name,
        AttributeElement::Indexed,
        2,
        AttributeUsage::UV,
        {uvs.data(), static_cast<size_t>(uvs.size())},
        {facets.data(), static_cast<size_t>(facets.size())});

    std::array<Scalar, 3> normals({0, 0, 1});
    std::vector<Index> normal_indices(0, mesh.get_num_corners());
    mesh.template create_attribute<Scalar>(
        setting.normal_attribute_name,
        AttributeElement::Indexed,
        3,
        AttributeUsage::Normal,
        {normals.data(), static_cast<size_t>(normals.size())},
        {normal_indices.data(), static_cast<size_t>(normal_indices.size())});

    transform_mesh(mesh, transformation);

    return mesh;
}

template <typename Scalar, typename Index, typename Vector>
void generate_quads(const RoundedCubeOptions& setting, Vector& parts)
{
    using Vector3S = Eigen::Matrix<Scalar, 3, 1>;
    using Vector2S = Eigen::Matrix<Scalar, 2, 1>;

    const Scalar w = setting.width - 2 * setting.bevel_radius;
    const Scalar h = setting.height - 2 * setting.bevel_radius;
    const Scalar d = setting.depth - 2 * setting.bevel_radius;
    const Scalar r = setting.bevel_radius;
    const Scalar t = static_cast<Scalar>(setting.bevel_radius * lagrange::internal::pi / 2);
    const Scalar s = std::max(2 * d + 2 * w + 4 * t, 2 * d + 2 * t + h);

    Eigen::Transform<Scalar, 3, Eigen::Affine> transformation;
    Eigen::Transform<Scalar, 2, Eigen::Affine> uv_transformation;

    Eigen::Matrix<Scalar, 3, 3> rot_y_180, rot_y_90, rot_x_90;
    rot_y_180 << -1, 0, 0, 0, 1, 0, 0, 0, -1;
    rot_y_90 << 0, 0, 1, 0, 1, 0, -1, 0, 0;
    rot_x_90 << 1, 0, 0, 0, 0, -1, 0, 1, 0;

    if (w > setting.epsilon && h > setting.epsilon) {
        // +Z quad
        transformation.setIdentity();
        transformation.translate(Vector3S(-w / 2, -h / 2, d / 2 + r));
        uv_transformation.setIdentity();
        uv_transformation.scale(1.0f / s);
        uv_transformation.translate(Vector2S(d + t, d + t));
        parts.push_back(
            generate_flat_quad<Scalar, Index>(
                setting,
                w,
                h,
                setting.width_segments,
                setting.height_segments,
                transformation,
                uv_transformation));
        add_semantic_label(
            parts.back(),
            setting.semantic_label_attribute_name,
            SemanticLabel::Side);

        // -Z quad
        transformation.setIdentity();
        transformation.translate(Vector3S(w / 2, -h / 2, -d / 2 - r));
        transformation.rotate(rot_y_180);
        uv_transformation.setIdentity();
        uv_transformation.scale(1.0f / s);
        uv_transformation.translate(Vector2S(2 * d + 3 * t + w, d + t));
        parts.push_back(
            generate_flat_quad<Scalar, Index>(
                setting,
                w,
                h,
                setting.width_segments,
                setting.height_segments,
                transformation,
                uv_transformation));
        add_semantic_label(
            parts.back(),
            setting.semantic_label_attribute_name,
            SemanticLabel::Side);
    }

    if (d > setting.epsilon && h > setting.epsilon) {
        // +X quad
        transformation.setIdentity();
        transformation.translate(Vector3S(w / 2 + r, -h / 2, d / 2));
        transformation.rotate(rot_y_90);
        uv_transformation.setIdentity();
        uv_transformation.scale(1.0f / s);
        uv_transformation.translate(Vector2S(d + 2 * t + w, d + t));
        parts.push_back(
            generate_flat_quad<Scalar, Index>(
                setting,
                d,
                h,
                setting.depth_segments,
                setting.height_segments,
                transformation,
                uv_transformation));
        add_semantic_label(
            parts.back(),
            setting.semantic_label_attribute_name,
            SemanticLabel::Side);

        // -X quad
        transformation.setIdentity();
        transformation.translate(Vector3S(-w / 2 - r, -h / 2, -d / 2));
        transformation.rotate(rot_y_90.transpose());
        uv_transformation.setIdentity();
        uv_transformation.scale(1.0f / s);
        uv_transformation.translate(Vector2S(0, d + t));
        parts.push_back(
            generate_flat_quad<Scalar, Index>(
                setting,
                d,
                h,
                setting.depth_segments,
                setting.height_segments,
                transformation,
                uv_transformation));
        add_semantic_label(
            parts.back(),
            setting.semantic_label_attribute_name,
            SemanticLabel::Side);
    }

    if (w > setting.epsilon && d > setting.epsilon) {
        // +Y quad
        transformation.setIdentity();
        transformation.translate(Vector3S(-w / 2, h / 2 + r, d / 2));
        transformation.rotate(rot_x_90.transpose());
        uv_transformation.setIdentity();
        uv_transformation.scale(1.0f / s);
        uv_transformation.translate(Vector2S(d + t, d + 2 * t + h));
        parts.push_back(
            generate_flat_quad<Scalar, Index>(
                setting,
                w,
                d,
                setting.width_segments,
                setting.depth_segments,
                transformation,
                uv_transformation));
        add_semantic_label(parts.back(), setting.semantic_label_attribute_name, SemanticLabel::Top);

        // -Y quad
        transformation.setIdentity();
        transformation.translate(Vector3S(-w / 2, -h / 2 - r, -d / 2));
        transformation.rotate(rot_x_90);
        uv_transformation.setIdentity();
        uv_transformation.scale(1.0f / s);
        uv_transformation.translate(Vector2S(d + t, 0));
        parts.push_back(
            generate_flat_quad<Scalar, Index>(
                setting,
                w,
                d,
                setting.width_segments,
                setting.depth_segments,
                transformation,
                uv_transformation));
        add_semantic_label(
            parts.back(),
            setting.semantic_label_attribute_name,
            SemanticLabel::Bottom);
    }
}

template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> generate_rounded_cube_v0(RoundedCubeOptions setting)
{
    setting.project_to_valid_range();
    SmallVector<SurfaceMesh<Scalar, Index>, 26> parts;

    if (setting.bevel_radius > setting.epsilon) {
        generate_corners<Scalar, Index>(setting, parts);
        generate_edges<Scalar, Index>(setting, parts);
    }
    generate_quads<Scalar, Index>(setting, parts);

    auto mesh = combine_meshes<Scalar, Index>({parts.data(), parts.size()}, true);

    // TODO: Add option to let user control whether to weld.
    bvh::WeldOptions weld_options;
    weld_options.boundary_only = true;
    weld_options.radius = setting.dist_threshold;
    bvh::weld_vertices(mesh, weld_options);

    if (setting.triangulate) {
        triangulate_polygonal_facets(mesh);
    }

    center_mesh(mesh, setting.center);
    return mesh;
}

template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> generate_side(
    const RoundedCubeOptions& setting,
    Scalar u_length,
    Scalar v_length,
    Index u_segments,
    Index v_segments,
    SemanticLabel semantic_label)
{
    const Index half_bevel_segments = setting.bevel_segments / 2;
    Index num_vertices =
        (u_segments + setting.bevel_segments + 1) * (v_segments + setting.bevel_segments + 1);
    Index num_facets =
        (u_segments + setting.bevel_segments) * (v_segments + setting.bevel_segments);

    SurfaceMesh<Scalar, Index> mesh;
    mesh.add_vertices(num_vertices);
    mesh.add_quads(num_facets);
    mesh.template create_attribute<uint8_t>(
        setting.semantic_label_attribute_name,
        AttributeElement::Facet,
        1,
        AttributeUsage::Scalar);

    auto vertices = vertex_ref(mesh);
    auto facets = facet_ref(mesh);
    auto semantic_labels =
        attribute_vector_ref<uint8_t>(mesh, setting.semantic_label_attribute_name);
    Eigen::Matrix<Scalar, Eigen::Dynamic, 2, Eigen::RowMajor> uvs(num_vertices, 2);
    Eigen::Matrix<Scalar, Eigen::Dynamic, 3, Eigen::RowMajor> normals(num_vertices, 3);

    auto interpolate_corner = [&](Scalar s, Scalar t) {
        const Scalar sqrt_3 = static_cast<Scalar>(std::sqrt(3.0));
        const Scalar sin_pi_4 = static_cast<Scalar>(std::sqrt(2) / 2);
        const Scalar cos_pi_4 = sin_pi_4; // cos(pi/4) == sin(pi/4)
        int8_t sign_s = (s < 0) ? -1 : 1;
        int8_t sign_t = (t < 0) ? -1 : 1;
        s = std::abs(s);
        t = std::abs(t);

        Eigen::Matrix<Scalar, 3, 1> c0, c1, c2, c3;
        c0 << 0, 0, 1;
        c1 << sin_pi_4, 0, cos_pi_4;
        c2 << 0, sin_pi_4, cos_pi_4;
        c3 << 1 / sqrt_3, 1 / sqrt_3, 1 / sqrt_3;

        Eigen::Matrix<Scalar, 3, 1> p =
            c0 * (1 - s) * (1 - t) + c1 * s * (1 - t) + c2 * (1 - s) * t + c3 * s * t;
        p(0) *= sign_s;
        p(1) *= sign_t;
        p.stableNormalize();
        return p;
    };

    for (Index i = 0; i <= v_segments + setting.bevel_segments; i++) {
        Scalar t = 0;
        Scalar y_offset = 0;
        if (i < half_bevel_segments) {
            t = -1 + static_cast<Scalar>(i) / half_bevel_segments;
            y_offset = -v_length / 2;
        } else if (i > v_segments + half_bevel_segments) {
            t = static_cast<Scalar>(i - v_segments - half_bevel_segments) / half_bevel_segments;
            y_offset = v_length / 2;
        } else {
            y_offset = v_segments == 0
                           ? 0
                           : -v_length / 2 + static_cast<Scalar>(i - half_bevel_segments) /
                                                 v_segments * v_length;
        }

        for (Index j = 0; j <= u_segments + setting.bevel_segments; j++) {
            Scalar s = 0;
            Scalar x_offset = 0;
            if (j < half_bevel_segments) {
                s = -1 + static_cast<Scalar>(j) / half_bevel_segments;
                x_offset = -u_length / 2;
            } else if (j > u_segments + half_bevel_segments) {
                s = static_cast<Scalar>(j - u_segments - half_bevel_segments) / half_bevel_segments;
                x_offset = u_length / 2;
            } else {
                x_offset = u_segments == 0
                               ? 0
                               : -u_length / 2 + static_cast<Scalar>(j - half_bevel_segments) /
                                                     u_segments * u_length;
            }

            Index idx = i * (u_segments + setting.bevel_segments + 1) + j;
            auto p = interpolate_corner(s, t);
            vertices.row(idx) << x_offset + setting.bevel_radius * p[0],
                y_offset + setting.bevel_radius * p[1], setting.bevel_radius * (p[2] - 1);

            Scalar u = x_offset + setting.bevel_radius * (lagrange::internal::pi / 4) * s;
            Scalar v = y_offset + setting.bevel_radius * (lagrange::internal::pi / 4) * t;
            uvs.row(idx) << u, v;

            normals.row(idx) = p;
        }
    }

    for (Index i = 0; i < v_segments + setting.bevel_segments; i++) {
        for (Index j = 0; j < u_segments + setting.bevel_segments; j++) {
            Index idx = i * (u_segments + setting.bevel_segments) + j;
            Index v0 = i * (u_segments + setting.bevel_segments + 1) + j;
            Index v1 = i * (u_segments + setting.bevel_segments + 1) + j + 1;
            Index v2 = (i + 1) * (u_segments + setting.bevel_segments + 1) + j + 1;
            Index v3 = (i + 1) * (u_segments + setting.bevel_segments + 1) + j;

            facets.row(idx) << v0, v1, v2, v3;
            semantic_labels[idx] =
                (i < half_bevel_segments || i >= v_segments + half_bevel_segments ||
                 j < half_bevel_segments || j >= u_segments + half_bevel_segments)
                    ? static_cast<uint8_t>(SemanticLabel::Bevel)
                    : static_cast<uint8_t>(semantic_label);
        }
    }

    mesh.template create_attribute<Scalar>(
        setting.uv_attribute_name,
        AttributeElement::Indexed,
        2,
        AttributeUsage::UV,
        {uvs.data(), static_cast<size_t>(uvs.size())},
        {facets.data(), static_cast<size_t>(facets.size())});

    bool sharp_normal = bevel_is_sharp(setting);
    if (sharp_normal) {
        FacetNormalOptions options;
        options.output_attribute_name = setting.normal_attribute_name;
        compute_facet_normal(mesh, options);
        map_attribute_in_place(mesh, setting.normal_attribute_name, AttributeElement::Indexed);
    } else {
        mesh.template create_attribute<Scalar>(
            setting.normal_attribute_name,
            AttributeElement::Indexed,
            3,
            AttributeUsage::Normal,
            {normals.data(), static_cast<size_t>(normals.size())},
            {facets.data(), static_cast<size_t>(facets.size())});
    }

    return mesh;
}

template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> generate_rounded_cube_v1(RoundedCubeOptions setting)
{
    setting.project_to_valid_range();
    if (setting.bevel_segments % 2 == 1) {
        logger().warn("Bevel segments must be even for fixed UV mode. Rounding up by +1.");
        setting.bevel_segments += 1;
    }
    SmallVector<SurfaceMesh<Scalar, Index>, 6> parts;

    Scalar w = setting.width - 2 * setting.bevel_radius;
    Scalar h = setting.height - 2 * setting.bevel_radius;
    Scalar d = setting.depth - 2 * setting.bevel_radius;

    Index b_segments = setting.bevel_segments;
    Index w_segments = w < setting.epsilon ? 0 : setting.width_segments;
    Index h_segments = h < setting.epsilon ? 0 : setting.height_segments;
    Index d_segments = d < setting.epsilon ? 0 : setting.depth_segments;

    if ((w_segments + b_segments) != 0 && (h_segments + b_segments) != 0) {
        auto side = generate_side<Scalar, Index>(
            setting,
            w,
            h,
            w_segments,
            h_segments,
            SemanticLabel::Side);

        // Front (+Z) side
        Eigen::Transform<Scalar, 3, Eigen::Affine> transform;
        transform.setIdentity();
        transform.translate(Eigen::Matrix<Scalar, 3, 1>(0, 0, setting.depth / 2));
        auto front_side = transformed_mesh(side, transform);
        normalize_uv(front_side, {0.25, 0.25}, {0.5, 0.5});
        parts.push_back(std::move(front_side));

        // Back (-Z) side
        transform.setIdentity();
        transform.translate(Eigen::Matrix<Scalar, 3, 1>(0, 0, -setting.depth / 2));
        transform.rotate(
            Eigen::AngleAxis<Scalar>(lagrange::internal::pi, Eigen::Matrix<Scalar, 3, 1>::UnitY()));
        auto back_side = transformed_mesh(side, transform);
        normalize_uv(back_side, {0.75, 0.25}, {1, 0.5});
        parts.push_back(std::move(back_side));
    }

    if ((d_segments + b_segments) != 0 && (h_segments + b_segments) != 0) {
        auto side = generate_side<Scalar, Index>(
            setting,
            d,
            h,
            d_segments,
            h_segments,
            SemanticLabel::Side);

        // Left (-X) side
        Eigen::Transform<Scalar, 3, Eigen::Affine> transform;
        transform.setIdentity();
        transform.translate(Eigen::Matrix<Scalar, 3, 1>(-setting.width / 2, 0, 0));
        transform.rotate(
            Eigen::AngleAxis<Scalar>(
                -lagrange::internal::pi / 2,
                Eigen::Matrix<Scalar, 3, 1>::UnitY()));
        auto left_side = transformed_mesh(side, transform);
        normalize_uv(left_side, {0.0, 0.25}, {0.25, 0.5});
        parts.push_back(std::move(left_side));

        // Right (+X) side
        transform.setIdentity();
        transform.translate(Eigen::Matrix<Scalar, 3, 1>(setting.width / 2, 0, 0));
        transform.rotate(
            Eigen::AngleAxis<Scalar>(
                lagrange::internal::pi / 2,
                Eigen::Matrix<Scalar, 3, 1>::UnitY()));
        auto right_side = transformed_mesh(side, transform);
        normalize_uv(right_side, {0.5, 0.25}, {0.75, 0.5});
        parts.push_back(std::move(right_side));
    }

    if ((w_segments + b_segments) != 0 && (d_segments + b_segments) != 0) {
        // Top (+Y) side
        auto top_side =
            generate_side<Scalar, Index>(setting, w, d, w_segments, d_segments, SemanticLabel::Top);
        Eigen::Transform<Scalar, 3, Eigen::Affine> transform;
        transform.setIdentity();
        transform.translate(Eigen::Matrix<Scalar, 3, 1>(0, setting.height / 2, 0));
        transform.rotate(
            Eigen::AngleAxis<Scalar>(
                -lagrange::internal::pi / 2,
                Eigen::Matrix<Scalar, 3, 1>::UnitX()));
        transform_mesh(top_side, transform);
        normalize_uv(top_side, {0.25, 0.5}, {0.5, 0.75});
        parts.push_back(std::move(top_side));

        // Bottom (-Y) side
        auto bottom_side = generate_side<Scalar, Index>(
            setting,
            w,
            d,
            w_segments,
            d_segments,
            SemanticLabel::Bottom);
        transform.setIdentity();
        transform.translate(Eigen::Matrix<Scalar, 3, 1>(0, -setting.height / 2, 0));
        transform.rotate(
            Eigen::AngleAxis<Scalar>(
                lagrange::internal::pi / 2,
                Eigen::Matrix<Scalar, 3, 1>::UnitX()));
        transform_mesh(bottom_side, transform);
        normalize_uv(bottom_side, {0.25, 0.0}, {0.5, 0.25});
        parts.push_back(std::move(bottom_side));
    }

    auto mesh = combine_meshes<Scalar, Index>({parts.data(), parts.size()}, true);

    // TODO: Add option to let user control whether to weld.
    bvh::WeldOptions weld_options;
    weld_options.boundary_only = true;
    weld_options.radius = setting.dist_threshold;
    bvh::weld_vertices(mesh, weld_options);

    if (setting.triangulate) {
        mesh.clear_edges();
        triangulate_polygonal_facets(mesh);
    }

    center_mesh(mesh, setting.center);
    return mesh;
}

} // namespace

template <typename Scalar, typename Index>
SurfaceMesh<Scalar, Index> generate_rounded_cube(RoundedCubeOptions setting)
{
    if (!setting.fixed_uv) {
        return generate_rounded_cube_v0<Scalar, Index>(std::move(setting));
    } else {
        return generate_rounded_cube_v1<Scalar, Index>(std::move(setting));
    }
}

#define LA_X_generate_rounded_cube(_, Scalar, Index)                                           \
    template LA_PRIMITIVE_API SurfaceMesh<Scalar, Index> generate_rounded_cube<Scalar, Index>( \
        RoundedCubeOptions);

LA_SURFACE_MESH_X(generate_rounded_cube, 0)

} // namespace lagrange::primitive
