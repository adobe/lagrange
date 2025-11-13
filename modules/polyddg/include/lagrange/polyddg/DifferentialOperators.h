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
#pragma once

#include <lagrange/SurfaceMesh.h>

#include <Eigen/Core>
#include <Eigen/Sparse>

namespace lagrange::polyddg {

/// @addtogroup module-polyddg
/// @{

///
/// Polygonal mesh discrete differential operators.
///
/// This class implements various discrete differential operators on polygonal meshes based on the
/// following paper:
///
/// De Goes, Fernando, Andrew Butts, and Mathieu Desbrun. "Discrete differential operators on
/// polygonal meshes." ACM Transactions on Graphics (TOG) 39.4 (2020): 110-1.
///
/// @tparam Scalar Scalar type.
/// @tparam Index  Index type.
///
template <typename Scalar, typename Index>
class DifferentialOperators
{
public:
    using Vector = Eigen::Matrix<Scalar, 1, 3>;

public:
    ///
    /// Precomputes necessary attributes for the differential operators.
    ///
    /// @param[in] mesh Input surface mesh (must be 3D).
    ///
    DifferentialOperators(SurfaceMesh<Scalar, Index>& mesh);

public:
    ///
    /// Compute the discrete polygonal gradient operator.
    ///
    /// The discrete gradient operator is of size #F * 3 by #V. It maps a discrete scalar function
    /// defined on the vertices to its gradient vector field defined on the facets.
    ///
    /// @return A sparse matrix representing the gradient operator.
    ///
    Eigen::SparseMatrix<Scalar> gradient() const;

    ///
    /// Compute the discrete d0 operator.
    ///
    /// The d0 operator computes the exterior derivative of a 0-form and outputs a 1-form.
    /// The discrete d0 operator is a matrix of size #E by #V.
    ///
    /// @return A sparse matrix representing the discrete d0 operator.
    ///
    Eigen::SparseMatrix<Scalar> d0() const;

    ///
    /// Compute the discrete d1 operator.
    ///
    /// The d1 operator computes the exterior derivative of a 1-form and outputs a 2-form.
    /// The discrete d1 operator is a matrix of size #F by #E.
    ///
    /// @return A sparse matrix representing the discrete d1 operator.
    ///
    Eigen::SparseMatrix<Scalar> d1() const;

    ///
    /// Compute the discrete Hodge star operator for 0-form.
    ///
    /// The Hodge star operator maps a k-form to a dual (n-k)-form, where n is the dimension of the
    /// manifold. The discrete Hodge star operator for 0-form is a matrix of size #V by #V.
    ///
    /// @return A sparse matrix representing the discrete Hodge star operator for 0-forms.
    ///
    Eigen::SparseMatrix<Scalar> star0() const;

    ///
    /// Compute the discrete Hodge star operator for 1-form.
    ///
    /// The Hodge star operator maps a k-form to a dual (n-k)-form, where n is the dimension of the
    /// manifold. The discrete Hodge star operator for 1-form is a matrix of size #E by #E.
    ///
    /// @return A sparse matrix representing the discrete Hodge star operator for 1-forms.
    ///
    Eigen::SparseMatrix<Scalar> star1() const;

    ///
    /// Compute the discrete Hodge star operator for 2-form.
    ///
    /// The Hodge star operator maps a k-form to a dual (n-k)-form, where n is the dimension of the
    /// manifold. The discrete Hodge star operator for 2-form is a matrix of size #F by #F.
    ///
    /// @return A sparse matrix representing the discrete Hodge star operator for 2-forms.
    ///
    Eigen::SparseMatrix<Scalar> star2() const;

    ///
    /// Compute the discrete flat operator.
    ///
    /// The flat operator maps a vector field to a 1-form. The discrete flat operator is a matrix
    /// of size #E by #F*3. It maps a vector field defined on the facets to a 1-form defined on the
    /// edges. Note that for non-boundary edges, the contribution from its incident facets are
    /// averaged.
    ///
    /// @return A sparse matrix representing the discrete flat operator.
    ///
    Eigen::SparseMatrix<Scalar> flat() const;

    ///
    /// Compute the discrete sharp operator.
    ///
    /// The sharp operator maps a 1-form to a vector field. The discrete sharp operator is a
    /// matrix of size #F*3 by #E.
    ///
    /// @return A sparse matrix representing the discrete sharp operator.
    ///
    Eigen::SparseMatrix<Scalar> sharp() const;

    ///
    /// Compute the projection operator.
    ///
    /// The projection operator measures the information loss when extracting the part of the
    /// 1-form associated with a vector field. It is a matrix of size #E by #E.
    ///
    /// @return A sparse matrix representing the projection operator.
    ///
    Eigen::SparseMatrix<Scalar> projection() const;

    ///
    /// Compute the discrete inner product operator for 0-forms.
    ///
    /// The inner product operator computes the inner product of two 0-forms.
    /// The discrete inner product operator for 0-form is a matrix of size #V by #V.
    ///
    /// @return A sparse matrix representing the discrete inner product operator.
    ///
    Eigen::SparseMatrix<Scalar> inner_product_0_form() const;

    ///
    /// Compute the discrete inner product operator for 1-forms.
    ///
    /// The inner product operator computes the inner product of two 1-forms.
    /// The discrete inner product operator for 1-form is a matrix of size #E by #E.
    ///
    /// @param[in] lambda Weight of projection term (default: 1).
    ///
    /// @return A sparse matrix representing the discrete inner product operator.
    ///
    Eigen::SparseMatrix<Scalar> inner_product_1_form(Scalar lambda = 1) const;

    ///
    /// Compute the discrete inner product operator for 2-forms.
    ///
    /// The inner product operator computes the inner product of two 2-forms.
    /// The discrete inner product operator for 2-form is a matrix of size #F by #F.
    ///
    /// @return A sparse matrix representing the discrete inner product operator.
    ///
    Eigen::SparseMatrix<Scalar> inner_product_2_form() const;

    ///
    /// Compute the discrete divergence operator.
    ///
    /// The divergence operator computes the divergence of a 1-form (i.e. vector field).
    /// The discrete divergence operator is a matrix of size #V by #E.
    ///
    /// @param[in] lambda Weight of projection term for the 1-form inner product (default: 1).
    ///
    /// @return A sparse matrix representing the discrete divergence operator.
    ///
    Eigen::SparseMatrix<Scalar> divergence(Scalar lambda = 1) const;

    ///
    /// Compute the discrete curl operator.
    ///
    /// The curl operator computes the curl of a 1-form (i.e. vector field).
    /// The discrete curl operator is a matrix of size #F by #E.
    ///
    /// @return A sparse matrix representing the discrete curl operator.
    ///
    Eigen::SparseMatrix<Scalar> curl() const;

    ///
    /// Compute the discrete Laplacian operator.
    ///
    /// The Laplacian operator computes the Laplacian of a 0-form (i.e. scalar field).
    /// The discrete Laplacian operator is a matrix of size #V by #V.
    ///
    /// @param[in] lambda Weight of projection term for the 1-form inner product (default: 1).
    ///
    /// @return A sparse matrix representing the discrete Laplacian operator.
    ///
    Eigen::SparseMatrix<Scalar> laplacian(Scalar lambda = 1) const;

    ///
    /// Compute the coordinate transformation that maps a per-vertex tangent vector field expressed
    /// in the global 3D coordinate to the local tangent basis at each vertex.
    ///
    /// The transformation matrix is of size #V * 2 by #V * 3.
    ///
    /// @return A matrix representing the coordinate transformation.
    ///
    Eigen::SparseMatrix<Scalar> vertex_tangent_coordinates() const;

    ///
    /// Compute the coordinate transformation that maps a per-facet tangent vector field expressed
    /// in the global 3D coordinate to the local tangent basis at each facet.
    ///
    /// The transformation matrix is of size #F * 2 by #F * 3.
    ///
    /// @return A matrix representing the coordinate transformation.
    ///
    Eigen::SparseMatrix<Scalar> facet_tangent_coordinates() const;

    ///
    /// Compute the discrete Levi-Civita connection.
    ///
    /// The discrete Levi-Civita connection parallel transports tangent vectors defined on vertices
    /// to tangent vectors defined on corners. It is represented as a matrix of size #C * 2 by #V *
    /// 2. All tangent vectors are expressed in its local tangent basis.
    ///
    /// @return A sparse matrix representing the discrete Levi-Civita connection.
    ///
    Eigen::SparseMatrix<Scalar> levi_civita() const;

    ///
    /// Compute the discrete Levi-Civita connection for n-rosy fields.
    ///
    /// The discrete Levi-Civita connection parallel transports tangent vectors defined on vertices
    /// to tangent vectors defined on corners. It is represented as a matrix of size #C * 2 by #V *
    /// 2. All tangent vectors are expressed in its local tangent basis.
    ///
    /// @param[in] n Number of times to apply the connection.
    ///
    /// @note The parameter n is designed to work with n-rosy field, where a representative
    /// tangent vector is the n-time rotation of any of the n vectors in a n-rosy field.
    ///
    /// @return A sparse matrix representing the discrete Levi-Civita connection.
    ///
    Eigen::SparseMatrix<Scalar> levi_civita_nrosy(Index n) const;

    ///
    /// Compute the discrete covariant derivative operator.
    ///
    /// The covariance derivative operator measures the change of a tangent vector field with
    /// respect to another tangent vector field using the Levi-Civita connection. In the discrete
    /// setting, both vector fields are defined on the vertices. The discrete covariance derivative
    /// operator is represented as a matrix of size #F * 4 by #V * 2. The output covariant
    /// derivative is a flattened 2 by 2 matrix defined on each facet.
    ///
    /// @return A sparse matrix representing the discrete covariant derivative operator.
    ///
    Eigen::SparseMatrix<Scalar> covariant_derivative() const;

    ///
    /// Compute the discrete covariant derivative operator for n-rosy fields.
    ///
    /// The covariance derivative operator measures the change of a tangent vector field with
    /// respect to another tangent vector field using the Levi-Civita connection. In the discrete
    /// setting, both vector fields are defined on the vertices. The discrete covariance derivative
    /// operator is represented as a matrix of size #F * 4 by #V * 2. The output covariant
    /// derivative is a flattened 2 by 2 matrix defined on each facet.
    ///
    /// @param[in] n Number of times to apply the connection.
    ///
    /// @note The parameter n is designed to work with n-rosy field, where a representative
    /// tangent vector is the n-time rotation of any of the n vectors in a n-rosy field.
    ///
    /// @return A sparse matrix representing the discrete covariant derivative operator.
    ///
    Eigen::SparseMatrix<Scalar> covariant_derivative_nrosy(Index n) const;

    ///
    /// Compute the connection Laplacian operator.
    ///
    /// The connection Laplacian operator computes the Laplacian of a tangent vector field defined
    /// on the vertices using Levi-Civita connection for parallel transport. It is represented as a
    /// matrix of size #V * 2 by #V * 2.
    ///
    /// @param[in] lambda Weight of projection term for the 1-form inner product (default: 1).
    ///
    /// @return A sparse matrix representing the discrete connection Laplacian operator.
    ///
    Eigen::SparseMatrix<Scalar> connection_laplacian(Scalar lambda = 1) const;

    ///
    /// Compute the connection Laplacian operator for n-rosy fields.
    ///
    /// The connection Laplacian operator computes the Laplacian of a tangent vector field defined
    /// on the vertices using Levi-Civita connection for parallel transport. It is represented as a
    /// matrix of size #V * 2 by #V * 2.
    ///
    /// @param[in] n Number of times to apply the connection.
    /// @param[in] lambda Weight of projection term for the 1-form inner product (default: 1).
    ///
    /// @note The parameter n is designed to work with n-rosy field, where a representative
    /// tangent vector is the n-time rotation of any of the n vectors in a n-rosy field.
    ///
    /// @return A sparse matrix representing the discrete connection Laplacian operator.
    ///
    Eigen::SparseMatrix<Scalar> connection_laplacian_nrosy(Index n, Scalar lambda = 1) const;

public:
    ///
    /// Compute the gradient for a single facet.
    ///
    /// The gradient for a single facet is a 3 by nf vector, where nf is the number vertices in the
    /// facet. It represents the gradient of a linear function defined on the facet.
    ///
    /// @param[in] fid Facet index.
    ///
    /// @return A matrix representing the gradient for the given facet.
    ///
    Eigen::Matrix<Scalar, 3, Eigen::Dynamic> gradient(Index fid) const;

    ///
    /// Compute the d0 operator for a single facet.
    ///
    /// The discrete d0 operator for a single facet is a nf by nf matrix, where nf is the number of
    /// vertices in the facet. It computes the exterior derivative of a 0-form (vertex values) to
    /// a 1-form (edge values) restricted to the edges of the facet.
    ///
    /// @param[in] fid Facet index.
    ///
    /// @return A matrix representing the d0 operator for the given facet.
    ///
    Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic> d0(Index fid) const;

    ///
    /// Compute the d1 operator for a single facet.
    ///
    /// The discrete d1 operator for a single facet is a row vector of size 1 by n, where n is the
    /// number of edges in the facet. It computes the exterior derivative of a 1-form (edge values)
    /// to a 2-form (facet value).
    ///
    /// @param[in] fid Facet index.
    ///
    /// @return A row vector representing the d1 operator for the given facet.
    ///
    Eigen::Matrix<Scalar, 1, Eigen::Dynamic> d1(Index fid) const;

    ///
    /// Compute the flat operator for a single facet.
    ///
    /// The discrete flat operator for a single fact is a nf by 3 matrix, where nf is the number of
    /// vertices of the facet. It maps a vector field defined on the facet to a 1-form defined on
    /// the edges of the facet.
    ///
    /// @param[in] fid Facet index.
    ///
    /// @return A matrix representing the flat operator for the given facet.
    ///
    Eigen::Matrix<Scalar, Eigen::Dynamic, 3> flat(Index fid) const;

    ///
    /// Compute the sharp operator for a single facet.
    ///
    /// The discrete sharp operator for a single fact is a 3 by nf matrix, where nf is the number of
    /// vertices of the facet. It maps a 1-form defined on the edges of the facet to a vector
    /// field defined on the facet.
    ///
    /// @param[in] fid Facet index.
    ///
    /// @return A matrix representing the sharp operator for the given facet.
    ///
    Eigen::Matrix<Scalar, 3, Eigen::Dynamic> sharp(Index fid) const;

    ///
    /// Compute the projection operator for a single facet.
    ///
    /// The discrete projection operator for a single fact is a nf by nf matrix, where nf is the
    /// number of vertices of the facet. It measures the information loss when extracting the part
    /// of the 1-form associated with a vector field.
    ///
    /// @param[in] fid Facet index.
    ///
    /// @return A matrix representing the projection operator for the given facet.
    ///
    Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic> projection(Index fid) const;

    ///
    /// Compute the inner product operator for 0-forms for a single facet.
    ///
    /// The discrete inner product operator for 0-forms for a single facet is a nf by nf matrix,
    /// where nf is the number of vertices of the facet. It computes the inner product of two
    /// 0-forms restricted to the vertices of the facet.
    ///
    /// @param[in] fid Facet index.
    ///
    /// @return A matrix representing the inner product operator for 0-forms for the given facet.
    ///
    Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic> inner_product_0_form(Index fid) const;

    ///
    /// Compute the inner product operator for 1-forms for a single facet.
    ///
    /// The discrete inner product operator for 1-forms for a single facet is a nf by nf
    /// matrix, where nf is the number of vertices of the facet. It computes the inner product
    /// of two 1-forms restricted to the edges of the facet.
    ///
    /// @param[in] fid    Facet index.
    /// @param[in] lambda Weight of projection term (default: 1).
    ///
    /// @return A matrix representing the inner product operator for 1-forms for the given facet.
    ///
    Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic> inner_product_1_form(
        Index fid,
        Scalar lambda = 1) const;

    ///
    /// Compute the inner product operator for 2-forms for a single facet.
    ///
    /// The discrete inner product operator for 2-forms for a single facet is a
    /// matrix (1 by 1). It computes the inner product of two 2-forms restricted to the facet.
    ///
    /// @param[in] fid Facet index.
    ///
    /// @return A matrix representing the inner product operator for 2-forms for the given facet.
    ///
    Eigen::Matrix<Scalar, 1, 1> inner_product_2_form(Index fid) const;

    ///
    /// Compute the Laplacian operator for a single facet.
    ///
    /// The discrete Laplacian operator for a single facet is a nf by nf matrix, where nf is the
    /// number of vertices of the facet. It computes the Laplacian of a 0-form (scalar field)
    /// restricted to the facet.
    ///
    /// @param[in] fid    Facet index.
    /// @param[in] lambda Weight of projection term for the 1-form inner product (default: 1).
    ///
    /// @return A matrix representing the Laplacian operator for the given facet.
    ///
    Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic> laplacian(Index fid, Scalar lambda = 1)
        const;

    ///
    /// Compute the discrete Levi-Civita connection that parallel transport a tangent vector from a
    /// vertex to an incident facet.
    ///
    /// @param[in] fid Facet index.
    /// @param[in] lv  Local vertex index in the facet.
    ///
    /// @return A 2 by 2 matrix representing the Levi-Civita connection.
    ///
    Eigen::Matrix<Scalar, 2, 2> levi_civita(Index fid, Index lv) const;

    ///
    /// Compute the discrete Levi-Civita connection that parallel transport a tangent vector from a
    /// vertex to an incident facet for n-rosy fields.
    ///
    /// @param[in] fid Facet index.
    /// @param[in] lv  Local vertex index in the facet.
    /// @param[in] n   Number of times to apply the connection.
    ///
    /// @note The parameter n is designed to work with n-rosy field, where a representative
    /// tangent vector is the n-time rotation of any of the n vectors in a n-rosy field.
    ///
    /// @return A 2 by 2 matrix representing the Levi-Civita connection.
    ///
    Eigen::Matrix<Scalar, 2, 2> levi_civita_nrosy(Index fid, Index lv, Index n) const;

    ///
    /// Compute the discrete Levi-Civita connection for a single facet.
    ///
    /// The per-facet Levi-Civita connection is a 2*nf by 2*nf block diagonal matrix that parallel
    /// transports tangent vectors from the vertex tangent space to the tangent space of the facet.
    /// Here nf is the number of vertices in the facet.
    ///
    /// @param[in] fid Facet index.
    ///
    /// @return A matrix representing the Levi-Civita connection.
    ///
    Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic> levi_civita(Index fid) const;

    ///
    /// Compute the discrete Levi-Civita connection for a single facet for n-rosy fields.
    ///
    /// The per-facet Levi-Civita connection is a 2*nf by 2*nf block diagonal matrix that parallel
    /// transports tangent vectors from the vertex tangent space to the tangent space of the facet.
    /// Here nf is the number of vertices in the facet.
    ///
    /// @param[in] fid Facet index.
    /// @param[in] n   Number of times to apply the connection.
    ///
    /// @note The parameter n is designed to work with n-rosy field, where a representative
    /// tangent vector is the n-time rotation of any of the n vectors in a n-rosy field.
    ///
    /// @return A matrix representing the Levi-Civita connection.
    ///
    Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic> levi_civita_nrosy(Index fid, Index n)
        const;

    ///
    /// Compute the discrete covariant derivative operator for a single facet.
    ///
    /// The discrete covariant derivative operator is a 4 by 2 * nf matrix that maps a tangent vector
    /// defined on a vertex to a flattened 2 by 2 covariant derivative matrix defined on the facet.
    /// Here nf is the number of vertices in the facet.
    ///
    /// @param[in] fid Facet index.
    ///
    /// @return A matrix representing the discrete covariant derivative operator.
    ///
    Eigen::Matrix<Scalar, 4, Eigen::Dynamic> covariant_derivative(Index fid) const;

    ///
    /// Compute the discrete covariant derivative operator for a single facet for n-rosy fields.
    ///
    /// The discrete covariant derivative operator is a 4 by 2 * nf matrix that maps a tangent vector
    /// defined on a vertex to a flattened 2 by 2 covariant derivative matrix defined on the facet.
    /// Here nf is the number of vertices in the facet.
    ///
    /// @param[in] fid Facet index.
    /// @param[in] n   Number of times to apply the connection.
    ///
    /// @note The parameter n is designed to work with n-rosy field, where a representative
    /// tangent vector is the n-time rotation of any of the n vectors in a n-rosy field.
    ///
    /// @return A matrix representing the discrete covariant derivative operator.
    ///
    Eigen::Matrix<Scalar, 4, Eigen::Dynamic> covariant_derivative_nrosy(Index fid, Index n) const;

    ///
    /// Compute the discrete covariant projection operator for a single facet.
    ///
    /// The discrete covariant projection operator for a single fact is a 2*nf by 2*nf matrix, where nf
    /// is the number vertices in facet. It measures the information loss when extracting the part
    /// of a tangent vector field associated with a covariant derivative.
    ///
    /// @param[in] fid Facet index.
    ///
    /// @return A matrix representing the discrete covariant projection operator.
    ///
    Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic> covariant_projection(Index fid) const;

    ///
    /// Compute the discrete covariant projection operator for a single facet for n-rosy fields.
    ///
    /// The discrete covariant projection operator for a single fact is a 2*nf by 2*nf matrix, where nf
    /// is the number vertices in facet. It measures the information loss when extracting the part
    /// of a tangent vector field associated with a covariant derivative.
    ///
    /// @param[in] fid Facet index.
    /// @param[in] n   Number of times to apply the connection.
    ///
    /// @note The parameter n is designed to work with n-rosy field, where a representative
    /// tangent vector is the n-time rotation of any of the n vectors in a n-rosy field.
    ///
    /// @return A matrix representing the discrete covariant projection operator.
    ///
    Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic> covariant_projection_nrosy(
        Index fid,
        Index n) const;

    ///
    /// Compute the discrete connection Laplacian operator for a single facet.
    ///
    /// The discrete connection Laplacian operator for a single facet is a 2*nf by 2*nf matrix,
    /// where nf is the number vertices in facet. It computes the Laplacian of a tangent vector
    /// field defined on the vertices of the facet using Levi-Civita connection for parallel
    /// transport.
    ///
    /// @param[in] fid    Facet index.
    /// @param[in] lambda Weight of projection term for the 1-form inner product (default: 1).
    ///
    /// @return A matrix representing the discrete connection Laplacian operator.
    ///
    Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic> connection_laplacian(
        Index fid,
        Scalar lambda = 1) const;

    ///
    /// Compute the discrete connection Laplacian operator for a single facet for n-rosy fields.
    ///
    /// The discrete connection Laplacian operator for a single facet is a 2*nf by 2*nf matrix,
    /// where nf is the number vertices in facet. It computes the Laplacian of a tangent vector
    /// field defined on the vertices of the facet using Levi-Civita connection for parallel
    /// transport.
    ///
    /// @param[in] fid    Facet index.
    /// @param[in] n      Number of times to apply the connection.
    /// @param[in] lambda Weight of projection term for the 1-form inner product (default: 1).
    ///
    /// @note The parameter n is designed to work with n-rosy field, where a representative
    /// tangent vector is the n-time rotation of any of the n vectors in a n-rosy field.
    ///
    /// @return A matrix representing the discrete connection Laplacian operator.
    ///
    Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic>
    connection_laplacian_nrosy(Index fid, Index n, Scalar lambda = 1) const;

private:
    ///
    /// Compute the per-facet vector area attribute and store it in the mesh.
    ///
    void compute_vertex_normal_from_vector_area();

public:
    ///
    /// Compute the local tangent basis for a single facet.
    ///
    /// @param[in] fid Facet index.
    ///
    /// @return A 3 by 2 matrix representing the tangent basis for the given facet.
    ///
    Eigen::Matrix<Scalar, 3, 2> facet_basis(Index fid) const;

    ///
    /// Compute the local tangent basis for a single vertex.
    ///
    /// @param[in] vid Vertex index.
    ///
    /// @return A 3 by 2 matrix representing the tangent basis for the given vertex.
    ///
    Eigen::Matrix<Scalar, 3, 2> vertex_basis(Index vid) const;

public:
    ///
    /// Get the attribute ID for the per-facet vector area.
    ///
    /// @return Attribute ID of vector area attribute.
    ///
    AttributeId get_vector_area_attribute_id() const { return m_vector_area_id; }

    ///
    /// Get the attribute ID for the per-facet centroid.
    ///
    /// @return Attribute ID of centroid attribute.
    ///
    AttributeId get_centroid_attribute_id() const { return m_centroid_id; }

    ///
    /// Get the attribute ID for the per-vertex normal.
    ///
    /// @return Attribute ID of vertex normal attribute.
    ///
    AttributeId get_vertex_normal_attribute_id() const { return m_vertex_normal_id; }

private:
    SurfaceMesh<Scalar, Index>& m_mesh;
    AttributeId m_vector_area_id = invalid_attribute_id();
    AttributeId m_centroid_id = invalid_attribute_id();
    AttributeId m_vertex_normal_id = invalid_attribute_id();
};

/// @}

} // namespace lagrange::polyddg
