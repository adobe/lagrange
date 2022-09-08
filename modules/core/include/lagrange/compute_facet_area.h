/*
 * Copyright 2017 Adobe. All rights reserved.
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

#include <Eigen/Dense>
#include <exception>

#include <lagrange/Mesh.h>
#include <lagrange/MeshTrait.h>
#include <lagrange/common.h>
#include <lagrange/utils/range.h>

namespace lagrange {
namespace internal {

///
/// Calculates the triangle areas.
///
/// @param[in]  mesh      Input triangle mesh.
///
/// @tparam     MeshType  Mesh type.
///
/// @return     #F x 1 array of triangle areas.
///
template <typename MeshType>
auto compute_triangle_areas(const MeshType& mesh) -> AttributeArrayOf<MeshType>
{
    using Scalar = typename MeshType::Scalar;
    using Index = typename MeshType::Index;
    using Vector = Eigen::Matrix<Scalar, 3, 1>;

    const Index dim = mesh.get_dim();
    const Index num_facets = mesh.get_num_facets();
    const auto& vertices = mesh.get_vertices();
    const auto& facets = mesh.get_facets();
    AttributeArrayOf<MeshType> areas(num_facets, 1);

    if (dim == 2) {
        for (Index i = 0; i < num_facets; i++) {
            const Index i0 = facets(i, 0);
            const Index i1 = facets(i, 1);
            const Index i2 = facets(i, 2);
            Vector v0, v1, v2;
            v0 << vertices(i0, 0), vertices(i0, 1), 0.0;
            v1 << vertices(i1, 0), vertices(i1, 1), 0.0;
            v2 << vertices(i2, 0), vertices(i2, 1), 0.0;
            Vector e1 = v1 - v0;
            Vector e2 = v2 - v0;
            areas(i, 0) = Scalar(0.5) * (e1.cross(e2)).norm();
        }
    } else if (dim == 3) {
        for (Index i = 0; i < num_facets; i++) {
            const Index i0 = facets(i, 0);
            const Index i1 = facets(i, 1);
            const Index i2 = facets(i, 2);
            Vector v0, v1, v2;
            v0 << vertices(i0, 0), vertices(i0, 1), vertices(i0, 2);
            v1 << vertices(i1, 0), vertices(i1, 1), vertices(i1, 2);
            v2 << vertices(i2, 0), vertices(i2, 1), vertices(i2, 2);
            Vector e1 = v1 - v0;
            Vector e2 = v2 - v0;
            areas(i, 0) = Scalar(0.5) * (e1.cross(e2)).norm();
        }
    } else {
        throw std::runtime_error("Unsupported dimention.");
    }

    return areas;
}

///
/// Calculates the quad areas.
///
/// @param[in]  mesh      Input quad mesh.
///
/// @tparam     MeshType  Mesh type.
///
/// @return     #F x 1 array of quad areas.
///
template <typename MeshType>
auto compute_quad_areas(const MeshType& mesh) -> AttributeArrayOf<MeshType>
{
    using Scalar = typename MeshType::Scalar;
    using Index = typename MeshType::Index;
    using Vector = Eigen::Matrix<Scalar, 3, 1>;

    const Index dim = mesh.get_dim();
    const Index num_facets = mesh.get_num_facets();
    const auto& vertices = mesh.get_vertices();
    const auto& facets = mesh.get_facets();
    AttributeArrayOf<MeshType> areas(num_facets, 1);

    if (dim == 2) {
        for (Index i = 0; i < num_facets; i++) {
            const Index i0 = facets(i, 0);
            const Index i1 = facets(i, 1);
            const Index i2 = facets(i, 2);
            const Index i3 = facets(i, 3);
            Vector v0, v1, v2, v3;
            v0 << vertices(i0, 0), vertices(i0, 1), 0.0f;
            v1 << vertices(i1, 0), vertices(i1, 1), 0.0f;
            v2 << vertices(i2, 0), vertices(i2, 1), 0.0f;
            v3 << vertices(i3, 0), vertices(i3, 1), 0.0f;
            Vector e10 = v1 - v0;
            Vector e30 = v3 - v0;
            Vector e12 = v1 - v2;
            Vector e32 = v3 - v2;
            areas(i, 0) = 0.5f * (e10.cross(e30)).norm() + 0.5f * (e12.cross(e32)).norm();
        }
    } else if (dim == 3) {
        for (Index i = 0; i < num_facets; i++) {
            const Index i0 = facets(i, 0);
            const Index i1 = facets(i, 1);
            const Index i2 = facets(i, 2);
            const Index i3 = facets(i, 3);
            Vector v0, v1, v2, v3;
            v0 << vertices(i0, 0), vertices(i0, 1), vertices(i0, 2);
            v1 << vertices(i1, 0), vertices(i1, 1), vertices(i1, 2);
            v2 << vertices(i2, 0), vertices(i2, 1), vertices(i2, 2);
            v3 << vertices(i3, 0), vertices(i3, 1), vertices(i3, 2);
            Vector e10 = v1 - v0;
            Vector e30 = v3 - v0;
            Vector e12 = v1 - v2;
            Vector e32 = v3 - v2;
            areas(i, 0) = 0.5f * (e10.cross(e30)).norm() + 0.5f * (e12.cross(e32)).norm();
        }
    } else {
        throw std::runtime_error("Unsupported dimention.");
    }

    return areas;
}
} // namespace internal

///
/// Calculates the facet areas.
///
/// @param[in]  mesh      Input mesh.
///
/// @tparam     MeshType  Mesh type.
///
/// @return     #F x 1 array of facet areas.
///
template <typename MeshType>
auto compute_facet_area_raw(const MeshType& mesh) -> AttributeArrayOf<MeshType>
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");
    using Index = typename MeshType::Index;
    const Index vertex_per_facet = mesh.get_vertex_per_facet();
    if (vertex_per_facet == 3) {
        return internal::compute_triangle_areas(mesh);
    } else if (vertex_per_facet == 4) {
        return internal::compute_quad_areas(mesh);
    } else {
        throw std::runtime_error("Unsupported facet type.");
    }
}

///
/// Calculates the facet areas. The result is stored as a new facet attribute `area`.
///
/// @param[in,out] mesh      Input mesh.
///
/// @tparam        MeshType  Mesh type.
///
template <typename MeshType>
void compute_facet_area(MeshType& mesh)
{
    static_assert(MeshTrait<MeshType>::is_mesh(), "Input type is not Mesh");
    auto areas = compute_facet_area_raw(mesh);
    mesh.add_facet_attribute("area");
    mesh.import_facet_attribute("area", areas);
}

///
/// Calculates the uv areas.
///
/// @param[in]  uv         UV vertex positions.
/// @param[in]  triangles  UV triangle indices.
///
/// @tparam     DerivedUV  Matrix type of UV vertex positions.
/// @tparam     DerivedF   Matrix type of UV triangle indices.
///
/// @return     #F x 1 array or triangle areas.
///
template <typename DerivedUV, typename DerivedF>
Eigen::Matrix<typename DerivedUV::Scalar, Eigen::Dynamic, 1> compute_uv_area_raw(
    const Eigen::MatrixBase<DerivedUV>& uv,
    const Eigen::MatrixBase<DerivedF>& triangles)
{
    using Scalar = typename DerivedUV::Scalar;
    using Index = typename DerivedF::Scalar;

    la_runtime_assert(uv.cols() == 2);
    la_runtime_assert(triangles.cols() == 3);
    const auto num_triangles = triangles.rows();

    auto compute_single_triangle_area = [&uv, &triangles](Index i) {
        const auto& f = triangles.row(i);
        const auto& v0 = uv.row(f[0]);
        const auto& v1 = uv.row(f[1]);
        const auto& v2 = uv.row(f[2]);
        return 0.5f * (v0[0] * v1[1] + v1[0] * v2[1] + v2[0] * v0[1] - v0[0] * v2[1] -
                       v1[0] * v0[1] - v2[0] * v1[1]);
    };

    Eigen::Matrix<Scalar, Eigen::Dynamic, 1> areas(num_triangles);
    for (auto i : range(safe_cast<Index>(num_triangles))) {
        areas[i] = compute_single_triangle_area(i);
    }
    return areas;
}
} // namespace lagrange
