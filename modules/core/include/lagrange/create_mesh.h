/*
 * Copyright 2020 Adobe. All rights reserved.
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

#include <lagrange/ActingMeshGeometry.h>
#include <lagrange/GenuineMeshGeometry.h>
#include <lagrange/Mesh.h>
#include <lagrange/MeshGeometry.h>
#include <lagrange/common.h>

namespace lagrange {

template <typename DerivedV, typename DerivedF>
std::unique_ptr<Mesh<DerivedV, DerivedF>> create_empty_mesh()
{
    DerivedV vertices;
    DerivedF facets;

    auto geometry = std::make_unique<GenuineMeshGeometry<DerivedV, DerivedF>>(vertices, facets);
    return std::make_unique<Mesh<DerivedV, DerivedF>>(std::move(geometry));
}


/**
 * This function create a new mesh given the vertex and facet arrays by copying
 * data into the Mesh object.
 * Both arrays can be of type `Eigen::Matrix` or `Eigen::Map`.
 */
template <typename DerivedV, typename DerivedF>
auto create_mesh(
    const Eigen::MatrixBase<DerivedV>& vertices,
    const Eigen::MatrixBase<DerivedF>& facets)
{
    using VertexArray = Eigen::Matrix<
        typename DerivedV::Scalar,
        DerivedV::RowsAtCompileTime,
        DerivedV::ColsAtCompileTime,
        Eigen::AutoAlign |
            (DerivedV::Flags & Eigen::RowMajorBit ? Eigen::RowMajor : Eigen::ColMajor)>;
    using FacetArray = Eigen::Matrix<
        typename DerivedF::Scalar,
        DerivedF::RowsAtCompileTime,
        DerivedF::ColsAtCompileTime,
        Eigen::AutoAlign |
            (DerivedF::Flags & Eigen::RowMajorBit ? Eigen::RowMajor : Eigen::ColMajor)>;
    auto geometry = std::make_unique<GenuineMeshGeometry<VertexArray, FacetArray>>(
        vertices.derived(),
        facets.derived());
    return std::make_unique<Mesh<VertexArray, FacetArray>>(std::move(geometry));
}

/**
 * This function create a new mesh given the vertex and facet arrays by moving
 * data into the Mesh object.
 * Both arrays must be of type `Eigen::Matrix`.
 */
template <typename DerivedV, typename DerivedF>
std::unique_ptr<Mesh<DerivedV, DerivedF>> create_mesh(
    Eigen::PlainObjectBase<DerivedV>&& vertices,
    Eigen::PlainObjectBase<DerivedF>&& facets)
{
    auto geometry = std::make_unique<GenuineMeshGeometry<DerivedV, DerivedF>>();
    geometry->import_vertices(vertices);
    geometry->import_facets(facets);
    return std::make_unique<Mesh<DerivedV, DerivedF>>(std::move(geometry));
}

template <typename DerivedV, typename DerivedF>
std::unique_ptr<Mesh<DerivedV, DerivedF>> create_mesh(
    const Eigen::MatrixBase<DerivedV>& vertices,
    Eigen::MatrixBase<DerivedF>&& facets)
{
    auto geometry = std::make_unique<GenuineMeshGeometry<DerivedV, DerivedF>>();
    DerivedV vertices_copy = vertices;
    geometry->import_vertices(vertices_copy);
    geometry->import_facets(facets);
    return std::make_unique<Mesh<DerivedV, DerivedF>>(std::move(geometry));
}

template <typename DerivedV, typename DerivedF>
std::unique_ptr<Mesh<DerivedV, DerivedF>> create_mesh(
    Eigen::MatrixBase<DerivedV>&& vertices,
    const Eigen::MatrixBase<DerivedF>& facets)
{
    auto geometry = std::make_unique<GenuineMeshGeometry<DerivedV, DerivedF>>();
    DerivedF facets_copy = facets;
    geometry->import_vertices(vertices);
    geometry->import_facets(facets_copy);
    return std::make_unique<Mesh<DerivedV, DerivedF>>(std::move(geometry));
}

/**
 * This method creates a Mesh object that wraps around vertices and facets.
 *
 * Warning: vertices and facets are referenced instead of copied, and thus they
 * have to be valid throughout the duration of the lifetime of the object.
 * It is the user's responsibility of ensure it is the case.
 */
template <typename VertexArray, typename FacetArray>
auto wrap_with_mesh(
    const Eigen::MatrixBase<VertexArray>& vertices,
    const Eigen::MatrixBase<FacetArray>& facets)
{
    using MeshType = Mesh<VertexArray, FacetArray>;

    auto geometry = std::make_unique<ActingMeshGeometry<VertexArray, FacetArray>>(
        vertices.derived(),
        facets.derived());
    return std::make_unique<MeshType>(std::move(geometry));
}

// Prevent user from wrapping Mesh around temp variables.
template <typename VertexArray, typename FacetArray>
void wrap_with_mesh(
    const Eigen::MatrixBase<VertexArray>&& vertices,
    const Eigen::MatrixBase<FacetArray>&& facets)
{
    static_assert(
        StaticAssertableBool<VertexArray, FacetArray>::False,
        "vertices and facets cannot be rvalues");
}
template <typename VertexArray, typename FacetArray>
void wrap_with_mesh(
    const Eigen::MatrixBase<VertexArray>& vertices,
    const Eigen::MatrixBase<FacetArray>&& facets)
{
    static_assert(
        StaticAssertableBool<VertexArray, FacetArray>::False,
        "facets cannot be an rvalue");
}
template <typename VertexArray, typename FacetArray>
void wrap_with_mesh(
    const Eigen::MatrixBase<VertexArray>&& vertices,
    const Eigen::MatrixBase<FacetArray>& facets)
{
    static_assert(
        StaticAssertableBool<VertexArray, FacetArray>::False,
        "vertices cannot be an rvalue");
}


std::unique_ptr<Mesh<Vertices3D, Triangles>> create_cube();

std::unique_ptr<Mesh<Vertices3D, Triangles>> create_quad(const bool with_center_vertex);

std::unique_ptr<Mesh<Vertices3D, Triangles>> create_sphere(
    typename Triangles::Scalar refine_order = 2);

} // namespace lagrange
