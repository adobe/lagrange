/*
 * Copyright 2019 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#pragma once

#include <algorithm>
#include <vector>

#include <lagrange/Mesh.h>
#include <lagrange/attributes/attribute_utils.h>
#include <lagrange/bvh/zip_boundary.h>
#include <lagrange/combine_mesh_list.h>
#include <lagrange/compute_edge_lengths.h>
#include <lagrange/compute_normal.h>
#include <lagrange/create_mesh.h>
#include <lagrange/internal/constants.h>
#include <lagrange/legacy/inline.h>
#include <lagrange/primitive/generation_utils.h>
#include <lagrange/utils/safe_cast.h>


namespace lagrange {
namespace primitive {
LAGRANGE_LEGACY_INLINE
namespace legacy {
namespace Cube {

template <typename MeshType>
std::unique_ptr<MeshType> generate_rounded_corner(
    const typename MeshType::Scalar radius,
    const typename MeshType::Index num_segments,
    const typename Eigen::Transform<typename MeshType::Scalar, 3, Eigen::AffineCompact>&
        transformation,
    const typename Eigen::Transform<typename MeshType::Scalar, 2, Eigen::AffineCompact>&
        uv_transformation)
{
    using Scalar = typename MeshType::Scalar;
    using Index = typename MeshType::Index;
    using VertexArray = typename MeshType::VertexArray;
    using FacetArray = typename MeshType::FacetArray;
    using UVArray = typename MeshType::UVArray;
    using UVIndices = typename MeshType::UVIndices;

    const Index num_vertices = (num_segments + 2) * (num_segments + 1) / 2;
    const Index num_facets = num_segments * num_segments;
    VertexArray vertices(num_vertices, 3);
    FacetArray facets(num_facets, 3);
    UVArray uvs(num_vertices, 2);
    UVIndices uv_indices(num_facets, 3);

    Index count = 0;
    for (auto i : range(num_segments + 1)) {
        for (auto j : range(num_segments + 1 - i)) {
            const Scalar theta = (Scalar)((i == num_segments) ? lagrange::internal::pi / 4
                                                              : (j * lagrange::internal::pi) /
                                                                    (2 * (num_segments - i)));
            const Scalar phi =
                (Scalar)(lagrange::internal::pi / 2 -
                         (num_segments - i) * lagrange::internal::pi / (2 * num_segments));

            vertices.row(count) << radius * sin(theta) * cos(phi), radius * sin(phi),
                radius * cos(theta) * cos(phi);
            uvs.row(count) << radius * theta, radius * phi;
            count++;
        }
    }
    assert(count == num_vertices);
    vertices.transpose() = transformation * vertices.transpose();
    uvs.template leftCols<2>().transpose() =
        uv_transformation * uvs.template leftCols<2>().transpose();

    count = 0;
    Index prev_base = 0;
    for (auto i : range(num_segments)) {
        Index next_base = prev_base + num_segments - i + 1;
        for (auto j : range(num_segments - i)) {
            facets.row(count) << prev_base + j, prev_base + j + 1, next_base + j;
            count++;
            if (j + 1 < num_segments - i) {
                facets.row(count) << next_base + j, prev_base + j + 1, next_base + j + 1;
                count++;
            }
        }
        prev_base = next_base;
    }
    assert(count == num_facets);
    uv_indices = facets; // Same index buffer.

    auto mesh = create_mesh(std::move(vertices), std::move(facets));
    mesh->initialize_uv(uvs, uv_indices);
    return mesh;
}

template <typename MeshType>
void generate_rounded_corners(
    const typename MeshType::Scalar width,
    const typename MeshType::Scalar height,
    const typename MeshType::Scalar depth,
    const typename MeshType::Scalar radius,
    const typename MeshType::Index num_segments,
    std::vector<std::unique_ptr<MeshType>>& parts)
{
    using Scalar = typename MeshType::Scalar;
    using Vector3S = Eigen::Matrix<Scalar, 3, 1>;
    using Vector2S = Eigen::Matrix<Scalar, 2, 1>;

    const Scalar w = width - 2 * radius;
    const Scalar h = height - 2 * radius;
    const Scalar d = depth - 2 * radius;
    const Scalar r = radius;
    const Scalar t = (Scalar)(radius * lagrange::internal::pi / 2);
    const Scalar s = std::max(2 * d + 2 * w + 4 * t, 2 * d + 2 * t + h);

    Eigen::Transform<Scalar, 3, Eigen::AffineCompact> transformation;
    Eigen::Transform<Scalar, 2, Eigen::AffineCompact> uv_transformation;

    // +X +Y +Z corner
    transformation.setIdentity();
    transformation.translate(Vector3S(w / 2, h / 2, d / 2));
    uv_transformation.setIdentity();
    uv_transformation.scale(1.0f / s);
    uv_transformation.translate(Vector2S(d + t + w, d + t + h));
    parts.push_back(
        generate_rounded_corner<MeshType>(r, num_segments, transformation, uv_transformation));
    set_uniform_semantic_label(*parts.back(), PrimitiveSemanticLabel::SIDE);

    // +X +Y -Z corner
    transformation.setIdentity();
    transformation.translate(Vector3S(w / 2, h / 2, -d / 2));
    transformation.rotate(
        Eigen::AngleAxis<Scalar>((Scalar)(0.5 * lagrange::internal::pi), Vector3S::UnitY()));
    uv_transformation.setIdentity();
    uv_transformation.scale(1.0f / s);
    uv_transformation.translate(Vector2S(2 * d + 2 * t + w, d + t + h));
    parts.push_back(
        generate_rounded_corner<MeshType>(r, num_segments, transformation, uv_transformation));
    set_uniform_semantic_label(*parts.back(), PrimitiveSemanticLabel::SIDE);

    // -X +Y -Z corner
    transformation.setIdentity();
    transformation.translate(Vector3S(-w / 2, h / 2, -d / 2));
    transformation.rotate(
        Eigen::AngleAxis<Scalar>((Scalar)lagrange::internal::pi, Vector3S::UnitY()));
    uv_transformation.setIdentity();
    uv_transformation.scale(1.0f / s);
    uv_transformation.translate(Vector2S(2 * d + 3 * t + 2 * w, d + t + h));
    parts.push_back(
        generate_rounded_corner<MeshType>(r, num_segments, transformation, uv_transformation));
    set_uniform_semantic_label(*parts.back(), PrimitiveSemanticLabel::SIDE);

    // -X +Y +Z corner
    transformation.setIdentity();
    transformation.translate(Vector3S(-w / 2, h / 2, d / 2));
    transformation.rotate(
        Eigen::AngleAxis<Scalar>((Scalar)(1.5 * lagrange::internal::pi), Vector3S::UnitY()));
    uv_transformation.setIdentity();
    uv_transformation.scale(1.0f / s);
    uv_transformation.translate(Vector2S(d, d + t + h));
    parts.push_back(
        generate_rounded_corner<MeshType>(r, num_segments, transformation, uv_transformation));
    set_uniform_semantic_label(*parts.back(), PrimitiveSemanticLabel::SIDE);

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
        generate_rounded_corner<MeshType>(r, num_segments, transformation, uv_transformation));
    set_uniform_semantic_label(*parts.back(), PrimitiveSemanticLabel::SIDE);

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
        generate_rounded_corner<MeshType>(r, num_segments, transformation, uv_transformation));
    set_uniform_semantic_label(*parts.back(), PrimitiveSemanticLabel::SIDE);

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
        generate_rounded_corner<MeshType>(r, num_segments, transformation, uv_transformation));
    set_uniform_semantic_label(*parts.back(), PrimitiveSemanticLabel::SIDE);

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
        generate_rounded_corner<MeshType>(r, num_segments, transformation, uv_transformation));
    set_uniform_semantic_label(*parts.back(), PrimitiveSemanticLabel::SIDE);
}

template <typename MeshType>
std::unique_ptr<MeshType> generate_rounded_edge(
    const typename MeshType::Scalar radius,
    const typename MeshType::Scalar l,
    const typename MeshType::Index num_round_segments,
    const typename MeshType::Index num_straight_segments,
    const typename Eigen::Transform<typename MeshType::Scalar, 3, Eigen::AffineCompact>&
        transformation,
    const typename Eigen::Transform<typename MeshType::Scalar, 2, Eigen::AffineCompact>&
        uv_transformation)
{
    using Scalar = typename MeshType::Scalar;
    using Index = typename MeshType::Index;
    using VertexArray = typename MeshType::VertexArray;
    using FacetArray = typename MeshType::FacetArray;
    using UVArray = typename MeshType::UVArray;
    using UVIndices = typename MeshType::UVIndices;

    Index num_vertices = (num_round_segments + 1) * (num_straight_segments + 1);
    VertexArray vertices(num_vertices, 3);
    UVArray uvs(num_vertices, 2);
    for (auto i : range(num_round_segments + 1)) {
        const Scalar theta = (Scalar)((i * lagrange::internal::pi) / (2 * num_round_segments));
        for (auto j : range(num_straight_segments + 1)) {
            auto idx = i * (num_straight_segments + 1) + j;
            auto lj = l * j / (Scalar)(num_straight_segments);
            vertices.row(idx) << radius * sin(theta), lj, radius * cos(theta);
            uvs.row(idx) << radius * theta, lj;
        }
    }
    vertices.transpose() = transformation * vertices.transpose();
    uvs.template leftCols<2>().transpose() =
        uv_transformation * uvs.template leftCols<2>().transpose();

    Index num_facets = 2 * num_round_segments * num_straight_segments;
    FacetArray facets(num_facets, 3);
    for (auto i : range(num_round_segments)) {
        for (auto j : range(num_straight_segments)) {
            auto v0 = i * (num_straight_segments + 1) + j;
            auto v1 = (i + 1) * (num_straight_segments + 1) + j;
            auto v2 = (i + 1) * (num_straight_segments + 1) + j + 1;
            auto v3 = i * (num_straight_segments + 1) + j + 1;

            auto idx = i * num_straight_segments + j;
            facets.row(idx * 2) << v0, v1, v2;
            facets.row(idx * 2 + 1) << v0, v2, v3;
        }
    }
    UVIndices uv_indices = facets;

    auto mesh = create_mesh(std::move(vertices), std::move(facets));
    mesh->initialize_uv(uvs, uv_indices);
    return mesh;
}

template <typename MeshType>
void generate_rounded_edges(
    const typename MeshType::Scalar width,
    const typename MeshType::Scalar height,
    const typename MeshType::Scalar depth,
    const typename MeshType::Scalar radius,
    const typename MeshType::Index num_round_segments,
    const typename MeshType::Index num_width_segments,
    const typename MeshType::Index num_height_segments,
    const typename MeshType::Index num_depth_segments,
    const typename MeshType::Scalar tolerance,
    std::vector<std::unique_ptr<MeshType>>& parts)
{
    using Scalar = typename MeshType::Scalar;
    using Vector3S = Eigen::Matrix<Scalar, 3, 1>;
    using Vector2S = Eigen::Matrix<Scalar, 2, 1>;

    const Scalar w = width - 2 * radius;
    const Scalar h = height - 2 * radius;
    const Scalar d = depth - 2 * radius;
    const Scalar r = radius;
    const Scalar t = (Scalar)(radius * lagrange::internal::pi / 2);
    const Scalar s = std::max(2 * d + 2 * w + 4 * t, 2 * d + 2 * t + h);

    Eigen::Transform<Scalar, 3, Eigen::AffineCompact> transformation;
    Eigen::Transform<Scalar, 2, Eigen::AffineCompact> uv_transformation;

    if (h > tolerance) {
        // +X +Z edge
        transformation.setIdentity();
        transformation.translate(Vector3S(w / 2, -h / 2, d / 2));
        uv_transformation.setIdentity();
        uv_transformation.scale(1.0f / s);
        uv_transformation.translate(Vector2S(d + t + w, d + t));
        parts.push_back(
            generate_rounded_edge<MeshType>(
                r,
                h,
                num_round_segments,
                num_height_segments,
                transformation,
                uv_transformation));
        set_uniform_semantic_label(*parts.back(), PrimitiveSemanticLabel::SIDE);

        // +X -Z edge
        transformation.setIdentity();
        transformation.translate(Vector3S(w / 2, -h / 2, -d / 2));
        transformation.rotate(
            Eigen::AngleAxis<Scalar>((Scalar)(0.5 * lagrange::internal::pi), Vector3S::UnitY()));
        uv_transformation.setIdentity();
        uv_transformation.scale(1.0f / s);
        uv_transformation.translate(Vector2S(2 * d + 2 * t + w, d + t));
        parts.push_back(
            generate_rounded_edge<MeshType>(
                r,
                h,
                num_round_segments,
                num_height_segments,
                transformation,
                uv_transformation));
        set_uniform_semantic_label(*parts.back(), PrimitiveSemanticLabel::SIDE);

        // -X -Z edge
        transformation.setIdentity();
        transformation.translate(Vector3S(-w / 2, -h / 2, -d / 2));
        transformation.rotate(
            Eigen::AngleAxis<Scalar>((Scalar)lagrange::internal::pi, Vector3S::UnitY()));
        uv_transformation.setIdentity();
        uv_transformation.scale(1.0f / s);
        uv_transformation.translate(Vector2S(2 * d + 3 * t + 2 * w, d + t));
        parts.push_back(
            generate_rounded_edge<MeshType>(
                r,
                h,
                num_round_segments,
                num_height_segments,
                transformation,
                uv_transformation));
        set_uniform_semantic_label(*parts.back(), PrimitiveSemanticLabel::SIDE);

        // -X +Z edge
        transformation.setIdentity();
        transformation.translate(Vector3S(-w / 2, -h / 2, d / 2));
        transformation.rotate(
            Eigen::AngleAxis<Scalar>((Scalar)(1.5 * lagrange::internal::pi), Vector3S::UnitY()));
        uv_transformation.setIdentity();
        uv_transformation.scale(1.0f / s);
        uv_transformation.translate(Vector2S(d, d + t));
        parts.push_back(
            generate_rounded_edge<MeshType>(
                r,
                h,
                num_round_segments,
                num_height_segments,
                transformation,
                uv_transformation));
        set_uniform_semantic_label(*parts.back(), PrimitiveSemanticLabel::SIDE);
    }

    if (w > tolerance) {
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
            generate_rounded_edge<MeshType>(
                r,
                w,
                num_round_segments,
                num_width_segments,
                transformation,
                uv_transformation));
        set_uniform_semantic_label(*parts.back(), PrimitiveSemanticLabel::SIDE);

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
            generate_rounded_edge<MeshType>(
                r,
                w,
                num_round_segments,
                num_width_segments,
                transformation,
                uv_transformation));
        set_uniform_semantic_label(*parts.back(), PrimitiveSemanticLabel::SIDE);

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
            generate_rounded_edge<MeshType>(
                r,
                w,
                num_round_segments,
                num_width_segments,
                transformation,
                uv_transformation));
        set_uniform_semantic_label(*parts.back(), PrimitiveSemanticLabel::SIDE);

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
            generate_rounded_edge<MeshType>(
                r,
                w,
                num_round_segments,
                num_width_segments,
                transformation,
                uv_transformation));
        set_uniform_semantic_label(*parts.back(), PrimitiveSemanticLabel::SIDE);
    }

    if (d > tolerance) {
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
            generate_rounded_edge<MeshType>(
                r,
                d,
                num_round_segments,
                num_depth_segments,
                transformation,
                uv_transformation));
        set_uniform_semantic_label(*parts.back(), PrimitiveSemanticLabel::SIDE);

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
            generate_rounded_edge<MeshType>(
                r,
                d,
                num_round_segments,
                num_depth_segments,
                transformation,
                uv_transformation));
        set_uniform_semantic_label(*parts.back(), PrimitiveSemanticLabel::SIDE);

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
            generate_rounded_edge<MeshType>(
                r,
                d,
                num_round_segments,
                num_depth_segments,
                transformation,
                uv_transformation));
        set_uniform_semantic_label(*parts.back(), PrimitiveSemanticLabel::SIDE);

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
            generate_rounded_edge<MeshType>(
                r,
                d,
                num_round_segments,
                num_depth_segments,
                transformation,
                uv_transformation));
        set_uniform_semantic_label(*parts.back(), PrimitiveSemanticLabel::SIDE);
    }
}

template <typename MeshType>
std::unique_ptr<MeshType> generate_flat_quad(
    const typename MeshType::Scalar l0,
    const typename MeshType::Scalar l1,
    const typename MeshType::Index num_segments_0,
    const typename MeshType::Index num_segments_1,
    const typename Eigen::Transform<typename MeshType::Scalar, 3, Eigen::AffineCompact>&
        transformation,
    const typename Eigen::Transform<typename MeshType::Scalar, 2, Eigen::AffineCompact>&
        uv_transformation)
{
    using Scalar = typename MeshType::Scalar;
    using Index = typename MeshType::Index;
    using VertexArray = typename MeshType::VertexArray;
    using FacetArray = typename MeshType::FacetArray;
    using UVArray = typename MeshType::UVArray;
    using UVIndices = typename MeshType::UVIndices;

    const Index num_vertices = (num_segments_0 + 1) * (num_segments_1 + 1);
    VertexArray vertices(num_vertices, 3);
    for (Index i = 0; i <= num_segments_0; i++) {
        for (Index j = 0; j <= num_segments_1; j++) {
            vertices.row(i * (num_segments_1 + 1) + j) << l0 * i / Scalar(num_segments_0),
                l1 * j / Scalar(num_segments_1), 0;
        }
    }

    UVArray uvs = vertices.template leftCols<2>();

    vertices.transpose() = transformation * vertices.transpose();
    uvs.template leftCols<2>().transpose() =
        uv_transformation * uvs.template leftCols<2>().transpose();

    const Index num_facets = num_segments_0 * num_segments_1 * 2;
    FacetArray facets(num_facets, 3);
    for (Index i = 0; i < num_segments_0; i++) {
        for (Index j = 0; j < num_segments_1; j++) {
            const Index k = i * num_segments_1 + j;
            const Index v0 = i * (num_segments_1 + 1) + j;
            const Index v1 = (i + 1) * (num_segments_1 + 1) + j;
            const Index v2 = (i + 1) * (num_segments_1 + 1) + j + 1;
            const Index v3 = i * (num_segments_1 + 1) + j + 1;
            facets.row(k * 2) << v0, v1, v2;
            facets.row(k * 2 + 1) << v0, v2, v3;
        }
    }
    UVIndices uv_indices = facets;

    auto mesh = create_mesh(std::move(vertices), std::move(facets));
    mesh->initialize_uv(uvs, uv_indices);
    return mesh;
}

template <typename MeshType>
void generate_flat_quads(
    const typename MeshType::Scalar width,
    const typename MeshType::Scalar height,
    const typename MeshType::Scalar depth,
    const typename MeshType::Scalar radius,
    const typename MeshType::Index num_width_segments,
    const typename MeshType::Index num_height_segments,
    const typename MeshType::Index num_depth_segments,
    const typename MeshType::Scalar tolerance,
    std::vector<std::unique_ptr<MeshType>>& parts)
{
    using Scalar = typename MeshType::Scalar;
    using Vector3S = Eigen::Matrix<Scalar, 3, 1>;
    using Vector2S = Eigen::Matrix<Scalar, 2, 1>;

    const Scalar w = width - 2 * radius;
    const Scalar h = height - 2 * radius;
    const Scalar d = depth - 2 * radius;
    const Scalar r = radius;
    const Scalar t = (Scalar)(radius * lagrange::internal::pi / 2);
    const Scalar s = std::max(2 * d + 2 * w + 4 * t, 2 * d + 2 * t + h);

    Eigen::Transform<Scalar, 3, Eigen::AffineCompact> transformation;
    Eigen::Transform<Scalar, 2, Eigen::AffineCompact> uv_transformation;

    Eigen::Matrix<Scalar, 3, 3> rot_y_180, rot_y_90, rot_x_90;
    rot_y_180 << -1, 0, 0, 0, 1, 0, 0, 0, -1;
    rot_y_90 << 0, 0, 1, 0, 1, 0, -1, 0, 0;
    rot_x_90 << 1, 0, 0, 0, 0, -1, 0, 1, 0;

    if (w > tolerance && h > tolerance) {
        // +Z quad
        transformation.setIdentity();
        transformation.translate(Vector3S(-w / 2, -h / 2, d / 2 + r));
        uv_transformation.setIdentity();
        uv_transformation.scale(1.0f / s);
        uv_transformation.translate(Vector2S(d + t, d + t));
        parts.push_back(
            generate_flat_quad<MeshType>(
                w,
                h,
                num_width_segments,
                num_height_segments,
                transformation,
                uv_transformation));
        set_uniform_semantic_label(*parts.back(), PrimitiveSemanticLabel::SIDE);

        // -Z quad
        transformation.setIdentity();
        transformation.translate(Vector3S(w / 2, -h / 2, -d / 2 - r));
        transformation.rotate(rot_y_180);
        uv_transformation.setIdentity();
        uv_transformation.scale(1.0f / s);
        uv_transformation.translate(Vector2S(2 * d + 3 * t + w, d + t));
        parts.push_back(
            generate_flat_quad<MeshType>(
                w,
                h,
                num_width_segments,
                num_height_segments,
                transformation,
                uv_transformation));
        set_uniform_semantic_label(*parts.back(), PrimitiveSemanticLabel::SIDE);
    }

    if (d > tolerance && h > tolerance) {
        // +X quad
        transformation.setIdentity();
        transformation.translate(Vector3S(w / 2 + r, -h / 2, d / 2));
        transformation.rotate(rot_y_90);
        uv_transformation.setIdentity();
        uv_transformation.scale(1.0f / s);
        uv_transformation.translate(Vector2S(d + 2 * t + w, d + t));
        parts.push_back(
            generate_flat_quad<MeshType>(
                d,
                h,
                num_depth_segments,
                num_height_segments,
                transformation,
                uv_transformation));
        set_uniform_semantic_label(*parts.back(), PrimitiveSemanticLabel::SIDE);

        // -X quad
        transformation.setIdentity();
        transformation.translate(Vector3S(-w / 2 - r, -h / 2, -d / 2));
        transformation.rotate(rot_y_90.transpose());
        uv_transformation.setIdentity();
        uv_transformation.scale(1.0f / s);
        uv_transformation.translate(Vector2S(0, d + t));
        parts.push_back(
            generate_flat_quad<MeshType>(
                d,
                h,
                num_depth_segments,
                num_height_segments,
                transformation,
                uv_transformation));
        set_uniform_semantic_label(*parts.back(), PrimitiveSemanticLabel::SIDE);
    }

    if (w > tolerance && d > tolerance) {
        // +Y quad
        transformation.setIdentity();
        transformation.translate(Vector3S(-w / 2, h / 2 + r, d / 2));
        transformation.rotate(rot_x_90.transpose());
        uv_transformation.setIdentity();
        uv_transformation.scale(1.0f / s);
        uv_transformation.translate(Vector2S(d + t, d + 2 * t + h));
        parts.push_back(
            generate_flat_quad<MeshType>(
                w,
                d,
                num_width_segments,
                num_depth_segments,
                transformation,
                uv_transformation));
        set_uniform_semantic_label(*parts.back(), PrimitiveSemanticLabel::TOP);

        // -Y quad
        transformation.setIdentity();
        transformation.translate(Vector3S(-w / 2, -h / 2 - r, -d / 2));
        transformation.rotate(rot_x_90);
        uv_transformation.setIdentity();
        uv_transformation.scale(1.0f / s);
        uv_transformation.translate(Vector2S(d + t, 0));
        parts.push_back(
            generate_flat_quad<MeshType>(
                w,
                d,
                num_width_segments,
                num_depth_segments,
                transformation,
                uv_transformation));
        set_uniform_semantic_label(*parts.back(), PrimitiveSemanticLabel::BOTTOM);
    }
}
} // End namespace Cube

/**
 * Configuration struct for rounded cube.
 */
struct RoundedCubeConfig
{
    using Scalar = float;
    using Index = uint32_t;

    ///
    /// @name Shape parameters.
    /// @{

    Scalar width = 1;
    Scalar height = 1;
    Scalar depth = 1;
    Scalar radius = 0;
    Index num_round_segments = 8;
    Index num_width_segments = 1;
    Index num_height_segments = 1;
    Index num_depth_segments = 1;
    Eigen::Matrix<Scalar, 3, 1> center{0, 0, 0};

    /// @}
    /// @name Output parameters.
    /// @{

    bool output_normals = true;

    /// @}
    /// @name Tolerances.
    /// @{

    /**
     * An edge is sharp iff its dihedral angle is larger than `angle_threshold`.
     */
    Scalar angle_threshold = static_cast<Scalar>(11 * lagrange::internal::pi / 180);

    /**
     * Two vertices are considered coinciding iff the distance between them is
     * smaller than `dist_threshold`.
     */
    Scalar dist_threshold = static_cast<Scalar>(1e-6);
    /// @}

    /**
     * Project config setting into valid range.
     *
     * This method ensure all lengths parameters are non-negative, and clip the
     * radius parameter to its valid range.
     */
    void project_to_valid_range()
    {
        width = std::max<Scalar>(width, 0);
        height = std::max<Scalar>(height, 0);
        depth = std::max<Scalar>(depth, 0);

        num_round_segments = std::max<Index>(num_round_segments, 1);
        num_width_segments = std::max<Index>(num_width_segments, 1);
        num_height_segments = std::max<Index>(num_height_segments, 1);
        num_depth_segments = std::max<Index>(num_depth_segments, 1);

        Scalar max_acceptable_radius = (std::min(std::min(width, height), depth)) / 2;
        radius = std::min(std::max(radius, Scalar(0)), max_acceptable_radius);

        angle_threshold = std::max<Scalar>(angle_threshold, 0);
        dist_threshold = std::max<Scalar>(dist_threshold, 0);
    }
};

/**
 * Generate rounded cube.
 *
 * @param[in] config  Configuration for the rounded cube.
 *
 * @return The generated mesh.  UVs and normals are initialized based on the
 *         config setting.
 */
template <typename MeshType>
std::unique_ptr<MeshType> generate_rounded_cube(RoundedCubeConfig config)
{
    using namespace Cube;
    using Scalar = typename MeshType::Scalar;
    using Index = typename MeshType::Index;

    config.project_to_valid_range();

    std::vector<std::unique_ptr<MeshType>> parts;
    parts.reserve(26);
    if (config.radius > config.dist_threshold && config.num_round_segments > 0) {
        generate_rounded_corners(
            config.width,
            config.height,
            config.depth,
            config.radius,
            safe_cast<Index>(config.num_round_segments),
            parts);
        generate_rounded_edges(
            config.width,
            config.height,
            config.depth,
            config.radius,
            safe_cast<Index>(config.num_round_segments),
            safe_cast<Index>(config.num_width_segments),
            safe_cast<Index>(config.num_height_segments),
            safe_cast<Index>(config.num_depth_segments),
            config.dist_threshold,
            parts);
    }
    generate_flat_quads(
        config.width,
        config.height,
        config.depth,
        config.radius,
        safe_cast<Index>(config.num_width_segments),
        safe_cast<Index>(config.num_height_segments),
        safe_cast<Index>(config.num_depth_segments),
        config.dist_threshold,
        parts);

    if (parts.size() == 0) {
        return create_empty_mesh<typename MeshType::VertexArray, typename MeshType::FacetArray>();
    }

    auto cube_mesh = combine_mesh_list(parts, true);
    {
        compute_edge_lengths(*cube_mesh);
        const auto& edge_lengths = cube_mesh->get_edge_attribute("length");
        cube_mesh = bvh::zip_boundary(
            *cube_mesh,
            std::min(
                static_cast<Scalar>(edge_lengths.array().mean() * 1e-2f),
                static_cast<Scalar>(config.dist_threshold)));
    }

    // Apply post-generation transformations.
    if (config.center.squaredNorm() > config.dist_threshold) {
        typename MeshType::VertexArray vertices;
        cube_mesh->export_vertices(vertices);
        vertices.rowwise() += config.center.transpose().template cast<Scalar>();
        cube_mesh->import_vertices(vertices);
    }

    // Compute corner normals
    if (config.output_normals) {
        compute_normal(*cube_mesh, config.angle_threshold);
        la_runtime_assert(cube_mesh->has_indexed_attribute("normal"));
    }

    return cube_mesh;
}

template <typename MeshType>
std::unique_ptr<MeshType> generate_rounded_cube(
    typename MeshType::Scalar width,
    typename MeshType::Scalar height,
    typename MeshType::Scalar depth,
    typename MeshType::Scalar radius,
    typename MeshType::Index num_round_segments,
    const Eigen::Matrix<typename MeshType::Scalar, 3, 1>& center = {0.0f, 0.0f, 0.0f},
    typename MeshType::Index num_width_segments = 1,
    typename MeshType::Index num_height_segments = 1,
    typename MeshType::Index num_depth_segments = 1)
{
    using Scalar = typename RoundedCubeConfig::Scalar;
    using Index = typename RoundedCubeConfig::Index;

    RoundedCubeConfig config;
    config.width = safe_cast<Scalar>(width);
    config.height = safe_cast<Scalar>(height);
    config.depth = safe_cast<Scalar>(depth);
    config.radius = safe_cast<Scalar>(radius);
    config.num_round_segments = safe_cast<Index>(num_round_segments);
    config.center = center.template cast<Scalar>();
    config.num_width_segments = safe_cast<Index>(num_width_segments);
    config.num_height_segments = safe_cast<Index>(num_height_segments);
    config.num_depth_segments = safe_cast<Index>(num_depth_segments);

    return generate_rounded_cube<MeshType>(std::move(config));
}

} // namespace legacy
} // namespace primitive
} // namespace lagrange
