/*
 * Copyright 2022 Adobe. All rights reserved.
 * This file is licensed to you under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under
 * the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
 * OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */

#include <Eigen/Geometry>

#include <lagrange/SurfaceMesh.h>
#include <lagrange/SurfaceMeshTypes.h>
#include <lagrange/normalize_meshes.h>
#include <lagrange/views.h>

#include <optional>

namespace lagrange {

template <size_t Dimension, typename Scalar, typename Index>
auto normalize_mesh_with_transform(
    SurfaceMesh<Scalar, Index>& mesh,
    const TransformOptions& options) -> Eigen::Transform<Scalar, Dimension, Eigen::Affine>
{
    auto ret = Eigen::Transform<Scalar, Dimension, Eigen::Affine>::Identity();
    la_debug_assert(std::fabs(ret.matrix().determinant() - 1.0) < 1e-7);

    if (mesh.get_dimension() != Dimension) return ret;

    const auto vertices = vertex_view(mesh);
    if (vertices.rows() == 0) return ret;

    const auto bbox_min = vertices.colwise().minCoeff().eval();
    const auto bbox_max = vertices.colwise().maxCoeff().eval();
    const auto bbox_center = ((bbox_min + bbox_max) / 2).eval();
    const auto bbox_scale = (bbox_max - bbox_min).norm() / 2;
    if (bbox_scale == 0) return ret;

    ret.translation() = bbox_center;
    ret.linear().diagonal().array() = bbox_scale;

    transform_mesh(mesh, ret.inverse(), options);

    return ret;
}

template <typename Scalar, typename Index>
void normalize_mesh(SurfaceMesh<Scalar, Index>& mesh, const TransformOptions& options)
{
    switch (mesh.get_dimension()) {
    case 2: normalize_mesh_with_transform<2>(mesh, options); break;
    case 3: normalize_mesh_with_transform<3>(mesh, options); break;
    default: la_runtime_assert(false); break;
    }
}

template <size_t Dimension, typename Scalar, typename Index>
auto normalize_meshes_with_transform(
    span<SurfaceMesh<Scalar, Index>*> meshes,
    const TransformOptions& options) -> Eigen::Transform<Scalar, Dimension, Eigen::Affine>
{
    auto ret = Eigen::Transform<Scalar, Dimension, Eigen::Affine>::Identity();
    la_debug_assert(std::fabs(ret.matrix().determinant() - 1.0) < 1e-7);

    Eigen::AlignedBox<Scalar, Dimension> bbox;
    for (auto mesh_ptr : meshes) {
        if (!mesh_ptr) return ret;
        auto& mesh = *mesh_ptr;

        if (mesh.get_dimension() != Dimension) return ret;

        auto vertices = vertex_view(mesh);
        if (vertices.rows() == 0) return ret;

        bbox.extend(vertices.colwise().minCoeff().transpose());
        bbox.extend(vertices.colwise().maxCoeff().transpose());
    }

    const auto bbox_min = bbox.min();
    const auto bbox_max = bbox.max();
    const auto bbox_center = ((bbox_min + bbox_max) / 2).eval();
    const auto bbox_scale = (bbox_max - bbox_min).norm() / 2;
    if (bbox_scale == 0) return ret;

    ret.translation() = bbox_center;
    ret.linear().diagonal().array() = bbox_scale;

    for (auto mesh_ptr : meshes) {
        la_debug_assert(mesh_ptr);
        la_debug_assert(mesh_ptr->get_dimension() == Dimension);
        transform_mesh(*mesh_ptr, ret.inverse(), options);
    }

    return ret;
}

template <typename Scalar, typename Index>
void normalize_meshes(span<SurfaceMesh<Scalar, Index>*> meshes, const TransformOptions& options)
{
    if (meshes.empty()) return;

    std::optional<Index> maybe_dim = std::nullopt;
    for (auto mesh_ptr : meshes) {
        la_runtime_assert(mesh_ptr);
        if (!maybe_dim) maybe_dim = mesh_ptr->get_dimension();
        la_debug_assert(maybe_dim);
        la_runtime_assert(*maybe_dim == mesh_ptr->get_dimension());
    }

    la_debug_assert(maybe_dim);
    switch (*maybe_dim) {
    case 2: normalize_meshes_with_transform<2>(meshes, options); break;
    case 3: normalize_meshes_with_transform<3>(meshes, options); break;
    default: la_runtime_assert(false); break;
    }
}

// clang-format off

#define LA_SURFACE_MESH_DIMENSION_X(mode, data) \
    LA_X_##mode(data, float, uint32_t, 2u) \
    LA_X_##mode(data, double, uint32_t, 2u) \
    LA_X_##mode(data, float, uint64_t, 2u) \
    LA_X_##mode(data, double, uint64_t, 2u) \
    LA_X_##mode(data, float, uint32_t, 3u) \
    LA_X_##mode(data, double, uint32_t, 3u) \
    LA_X_##mode(data, float, uint64_t, 3u) \
    LA_X_##mode(data, double, uint64_t, 3u)

// clang-format on

#define LA_X_normalize_meshes_with_transform(_, Scalar, Index, Dimension)                       \
    template LA_CORE_API auto normalize_mesh_with_transform<Dimension>(                         \
        SurfaceMesh<Scalar, Index> & mesh,                                                      \
        const TransformOptions& options) -> Eigen::Transform<Scalar, Dimension, Eigen::Affine>; \
    template LA_CORE_API auto normalize_meshes_with_transform<Dimension>(                       \
        span<SurfaceMesh<Scalar, Index>*> meshes,                                               \
        const TransformOptions& options) -> Eigen::Transform<Scalar, Dimension, Eigen::Affine>;
LA_SURFACE_MESH_DIMENSION_X(normalize_meshes_with_transform, 0)

#define LA_X_normalize_meshes(_, Scalar, Index)   \
    template LA_CORE_API void normalize_mesh(     \
        SurfaceMesh<Scalar, Index>& mesh,         \
        const TransformOptions& options);         \
    template LA_CORE_API void normalize_meshes(   \
        span<SurfaceMesh<Scalar, Index>*> meshes, \
        const TransformOptions& options);
LA_SURFACE_MESH_X(normalize_meshes, 0)

} // namespace lagrange
