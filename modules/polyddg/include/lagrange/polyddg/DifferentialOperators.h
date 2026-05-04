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
    /// Compute the discrete Hodge star operator for 0-forms (diagonal mass matrix, size #V x #V).
    ///
    /// @return A diagonal sparse matrix of size #V x #V.
    ///
    Eigen::SparseMatrix<Scalar> star0() const;

    ///
    /// Compute the discrete Hodge star operator for 1-forms (size #E x #E).
    ///
    /// Following de Goes, Butts and Desbrun (ACM Trans. Graph. 2020, Section 4.4), the 1-form
    /// Hodge star is the VEM-stabilized inner product
    ///
    /// @f[
    ///   M_1 = \sum_f \text{area}^f U_f^T U_f + \lambda P_f^T P_f
    /// @f]
    ///
    /// where @f$ U_f @f$ is the per-face sharp operator and @f$ P_f = I - V_f U_f @f$ is the projection onto the
    /// kernel of @f$ U_f @f$.  This matrix is symmetric positive-definite, scale-invariant, and
    /// constant-precise for arbitrary polygons (including non-planar faces).
    ///
    /// This mirrors the convention used for star0() and star2(), which also delegate to their
    /// respective inner-product operators.  For triangulated meshes, λ=1 is recommended
    /// and coincides with the standard cotangent Laplacian construction.
    ///
    /// @param[in] lambda  Stabilization weight for the VEM projection term (default 1).
    ///
    /// @return A symmetric positive-definite sparse matrix of size #E x #E.
    ///
    Eigen::SparseMatrix<Scalar> star1(Scalar lambda = 1) const;

    ///
    /// Compute the discrete Hodge star operator for 2-forms (diagonal mass matrix, size #F x #F).
    ///
    /// @return A diagonal sparse matrix of size #F x #F.
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
    /// Compute the discrete weak-form Laplacian operator (@f$ \Delta : \Omega^0 \to \tilde{\Omega}^2 @f$).
    ///
    /// Maps a primal 0-form (per-vertex scalar) to a dual 2-form (integrated value over each dual
    /// cell, also per-vertex). This is the weak-form (Galerkin) Laplacian: @f$ \Delta = d_0^T \cdot M_1 \cdot d_0 @f$,
    /// so the right-hand side of @f$ \Delta u = f @f$ must be assembled as a dual 2-form (i.e. @f$ \int f \varphi_v dA @f$ per
    /// vertex), not as pointwise vertex values. The matrix is of size #V by #V.
    ///
    /// @param[in] lambda Weight of projection term for the 1-form inner product (default: 1).
    ///
    /// @return A sparse matrix representing the discrete Laplacian operator.
    ///
    Eigen::SparseMatrix<Scalar> laplacian(Scalar lambda = 1) const;

    ///
    /// Compute the discrete weak-form co-differential operator (@f$ \delta_1 : \Omega^1 \to \tilde{\Omega}^2 @f$).
    ///
    /// This is a weak-form (Galerkin) operator. It maps a primal 1-form (per-edge scalar, size #E)
    /// to a dual 2-form (integrated value over each dual cell, per-vertex, size #V). The output is
    /// NOT a primal 0-form; to recover a primal 0-form one would need to apply the inverse vertex
    /// mass matrix @f$ M_0^{-1} @f$ (strong form: @f$ M_0^{-1} d_0^T M_1 @f$).
    ///
    /// Equals divergence(lambda): @f$ \delta_1 = d_0^T \cdot M_1(\lambda) @f$. The matrix is of size #V by #E.
    ///
    /// @param[in] lambda Weight of projection term for the 1-form inner product (default: 1).
    ///
    /// @return A sparse matrix representing the weak-form 1-form co-differential operator.
    ///
    Eigen::SparseMatrix<Scalar> delta1(Scalar lambda = 1) const;

    ///
    /// Compute the discrete weak-form co-differential operator (@f$ \delta_2 : \Omega^2 \to \tilde{\Omega}^1 @f$).
    ///
    /// This is a weak-form (Galerkin) operator. It maps a primal 2-form (per-facet scalar, size #F)
    /// to a dual 1-form (integrated value over each dual edge, per-edge, size #E). The output is
    /// NOT a primal 1-form; to recover a primal 1-form one would need to apply the inverse edge
    /// mass matrix @f$ M_1^{-1} @f$ (strong form: @f$ M_1^{-1} d_1^T M_2 @f$).
    ///
    /// @f$ \delta_2 = d_1^T \cdot M_2 @f$ where @f$ M_2 = @f$ inner_product_2_form(). The matrix is of size #E by #F.
    ///
    /// @return A sparse matrix representing the weak-form 2-form co-differential operator.
    ///
    Eigen::SparseMatrix<Scalar> delta2() const;

    ///
    /// Compute the discrete weak-form Laplacian operator on 2-forms (@f$ \Delta_2 : \Omega^2 \to \tilde{\Omega}^0 @f$).
    ///
    /// Maps a primal 2-form (per-facet scalar, size #F) to a dual 0-form (integrated value over
    /// each dual vertex, per-facet, size #F). This is a weak-form operator: @f$ \Delta_2 = d_1 \cdot \delta_2 = d_1 \cdot d_1^T \cdot M_2 @f$.
    /// The matrix is of size #F by #F.
    ///
    /// @return A sparse matrix representing the weak-form 2-form Laplacian operator.
    ///
    Eigen::SparseMatrix<Scalar> laplacian2() const;

    ///
    /// Compute the discrete weak-form Hodge Laplacian on 1-forms (@f$ \Delta_1 : \Omega^1 \to \tilde{\Omega}^1 @f$).
    ///
    /// Maps a primal 1-form (per-edge scalar, size #E) to a dual 1-form (per-edge, size #E).
    /// Decomposes into an exact part (@f$ d_0 \delta_1 @f$) and a co-exact part (@f$ \delta_2 d_1 @f$), both weak-form:
    /// @f[
    ///   \Delta_1 = d_0 \cdot \delta_1(\lambda) + \delta_2 \cdot d_1
    /// @f]
    ///
    /// This operator is required for Helmholtz-Hodge decomposition of 1-forms and is distinct
    /// from connection_laplacian(), which operates on tangent vector fields at vertices.
    /// The matrix is of size #E by #E.
    ///
    /// @param[in] lambda Weight of projection term for the 1-form inner product (default: 1).
    ///
    /// @return A sparse matrix representing the weak-form Hodge Laplacian on 1-forms.
    ///
    Eigen::SparseMatrix<Scalar> laplacian1(Scalar lambda = 1) const;

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
    /// n-rosy variant of levi_civita().
    ///
    /// @param[in] n Symmetry order of the rosy field (applies the connection n times).
    ///
    /// @return A sparse matrix of size (#C * 2) x (#V * 2).
    ///
    Eigen::SparseMatrix<Scalar> levi_civita_nrosy(Index n) const;

    ///
    /// Compute the discrete covariant derivative operator.
    ///
    /// The covariant derivative operator measures the change of a tangent vector field with
    /// respect to another tangent vector field using the Levi-Civita connection. In the discrete
    /// setting, both vector fields are defined on the vertices. The output covariant derivative is
    /// a flattened 2x2 matrix defined on each facet.
    ///
    /// @return A sparse matrix of size (#F * 4) x (#V * 2) representing the discrete covariant
    ///         derivative operator.
    ///
    Eigen::SparseMatrix<Scalar> covariant_derivative() const;

    ///
    /// n-rosy variant of covariant_derivative().
    ///
    /// @param[in] n Symmetry order of the rosy field (applies the connection n times).
    ///
    /// @return A sparse matrix of size (#F * 4) x (#V * 2).
    ///
    Eigen::SparseMatrix<Scalar> covariant_derivative_nrosy(Index n) const;

    ///
    /// Compute the global discrete shape operator (Eq. (23), de Goes et al. 2020).
    ///
    /// Applies the per-facet shape operator to the precomputed per-vertex normals and assembles
    /// the result into a sparse linear map. It maps a per-vertex 3-D normal field (#V * 3 values,
    /// three interleaved components per vertex) to per-facet 2x2 symmetrized shape operators
    /// (#F * 4 values, row-major per facet: [S(0,0), S(0,1), S(1,0), S(1,1)]).
    ///
    /// @return A sparse matrix of size (#F * 4) x (#V * 3).
    ///
    Eigen::SparseMatrix<Scalar> shape_operator() const;

    ///
    /// Compute the global discrete adjoint gradient operator (Eq. (24), de Goes et al. 2020).
    ///
    /// The adjoint gradient is the vertex-centered dual of the per-facet gradient. It maps a
    /// per-facet scalar field (#F values) to a per-vertex 3-D tangent vector field. The returned
    /// sparse matrix has shape (#V * 3) x #F, with three interleaved components per vertex row.
    ///
    /// @return A sparse matrix of size (#V * 3) x #F.
    ///
    Eigen::SparseMatrix<Scalar> adjoint_gradient() const;

    ///
    /// Compute the global discrete adjoint shape operator (Eq. (26), de Goes et al. 2020).
    ///
    /// The adjoint shape operator is the vertex-centered dual of the per-facet shape operator. It
    /// maps a per-facet 3-D normal field (#F * 3 values, three interleaved components per facet)
    /// to per-vertex 2x2 symmetrized shape operators (#V * 4 values, row-major per vertex:
    /// [S(0,0), S(0,1), S(1,0), S(1,1)]).
    ///
    /// @return A sparse matrix of size (#V * 4) x (#F * 3).
    ///
    Eigen::SparseMatrix<Scalar> adjoint_shape_operator() const;

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
    /// n-rosy variant of connection_laplacian().
    ///
    /// @param[in] n      Symmetry order of the rosy field (applies the connection n times).
    /// @param[in] lambda Weight of projection term for the 1-form inner product (default: 1).
    ///
    /// @return A sparse matrix of size (#V * 2) x (#V * 2).
    ///
    Eigen::SparseMatrix<Scalar> connection_laplacian_nrosy(Index n, Scalar lambda = 1) const;

public:
    ///
    /// Compute the per-corner gradient vectors for a single facet (Eq. (8), de Goes et al. 2020).
    ///
    /// Returns a 3 x nf matrix whose column l is (a_f x e_f^l) / (2 * |a_f|^2), where a_f is
    /// the facet vector area and e_f^l = x_{l-1} - x_{l+1} spans the opposite edge.
    ///
    /// @param[in] fid Facet index.
    ///
    /// @return A 3 x nf matrix of per-corner gradient vectors.
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
    /// Maps a vector field defined on the facet to a 1-form on its edges. The matrix has size
    /// nf x 3, where nf is the number of vertices of the facet.
    ///
    /// @param[in] fid Facet index.
    ///
    /// @return An nf x 3 matrix representing the flat operator for the given facet.
    ///
    Eigen::Matrix<Scalar, Eigen::Dynamic, 3> flat(Index fid) const;

    ///
    /// Compute the sharp operator for a single facet.
    ///
    /// Maps a 1-form on the edges of the facet to a vector field on the facet. The matrix has
    /// size 3 x nf, where nf is the number of vertices of the facet.
    ///
    /// @param[in] fid Facet index.
    ///
    /// @return A 3 x nf matrix representing the sharp operator for the given facet.
    ///
    Eigen::Matrix<Scalar, 3, Eigen::Dynamic> sharp(Index fid) const;

    ///
    /// Compute the projection operator for a single facet.
    ///
    /// Measures the information loss when extracting the part of a 1-form associated with a
    /// vector field. The matrix has size nf x nf, where nf is the number of vertices.
    ///
    /// @param[in] fid Facet index.
    ///
    /// @return An nf x nf matrix representing the projection operator for the given facet.
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
    /// n-rosy variant of levi_civita(fid, lv).
    ///
    /// @param[in] fid Facet index.
    /// @param[in] lv  Local vertex index in the facet.
    /// @param[in] n   Symmetry order of the rosy field (applies the connection n times).
    ///
    /// @return A 2x2 matrix representing the Levi-Civita connection.
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
    /// n-rosy variant of levi_civita(fid).
    ///
    /// @param[in] fid Facet index.
    /// @param[in] n   Symmetry order of the rosy field (applies the connection n times).
    ///
    /// @return A (2*nf) x (2*nf) block-diagonal matrix representing the Levi-Civita connection.
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
    /// n-rosy variant of covariant_derivative(fid).
    ///
    /// @param[in] fid Facet index.
    /// @param[in] n   Symmetry order of the rosy field (applies the connection n times).
    ///
    /// @return A 4 x (2*nf) matrix representing the covariant derivative operator.
    ///
    Eigen::Matrix<Scalar, 4, Eigen::Dynamic> covariant_derivative_nrosy(Index fid, Index n) const;

    ///
    /// Compute the discrete shape operator for a single facet (Eq. (23), de Goes et al. 2020).
    ///
    /// Applies the per-facet gradient to the precomputed per-vertex normals and symmetrizes the
    /// result in the facet tangent plane. The returned 2x2 matrix is symmetric; its trace divided
    /// by two gives the mean curvature at the facet, and its determinant gives the Gaussian
    /// curvature.
    ///
    /// @param[in] fid Facet index.
    ///
    /// @return A 2x2 symmetric matrix representing the shape operator for the given facet.
    ///
    Eigen::Matrix<Scalar, 2, 2> shape_operator(Index fid) const;

    ///
    /// Compute the adjoint gradient operator for a single vertex (Eq. (24), de Goes et al. 2020).
    ///
    /// Returns a 3 x k dense matrix whose columns are the area-weighted, parallel-transported
    /// per-corner gradient vectors for vertex @p vid, where k is the number of incident faces.
    /// Columns are in the order of the incident-face traversal via get_first_corner_around_vertex(),
    /// consistent with adjoint_shape_operator().
    ///
    /// @note There is a sign correction for Eq. (24) in the implementation.
    ///
    /// @param[in] vid Vertex index.
    ///
    /// @return A 3 x k dense matrix (k = number of incident faces).
    ///
    Eigen::Matrix<Scalar, 3, Eigen::Dynamic> adjoint_gradient(Index vid) const;

    ///
    /// Compute the adjoint shape operator for a single vertex (Eq. (26), de Goes et al. 2020).
    ///
    /// The adjoint shape operator is the vertex-centered dual of the per-facet shape operator. It
    /// applies the adjoint gradient to the unit normals of the incident faces and symmetrizes the
    /// result in the vertex tangent plane. The returned 2x2 matrix is symmetric; its trace divided
    /// by two gives the mean curvature at the vertex, and its determinant gives the Gaussian
    /// curvature.
    ///
    /// @param[in] vid Vertex index.
    ///
    /// @return A 2x2 symmetric matrix representing the adjoint shape operator at the vertex.
    ///
    Eigen::Matrix<Scalar, 2, 2> adjoint_shape_operator(Index vid) const;

    ///
    /// Compute the discrete covariant projection operator for a single facet.
    ///
    /// Measures the information loss when extracting the part of a tangent vector field associated
    /// with a covariant derivative. The matrix has size (2*nf) x (2*nf), where nf is the number of
    /// vertices in the facet.
    ///
    /// @param[in] fid Facet index.
    ///
    /// @return A (2*nf) x (2*nf) matrix representing the covariant projection operator.
    ///
    Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic> covariant_projection(Index fid) const;

    ///
    /// n-rosy variant of covariant_projection(fid).
    ///
    /// @param[in] fid Facet index.
    /// @param[in] n   Symmetry order of the rosy field (applies the connection n times).
    ///
    /// @return A (2*nf) x (2*nf) matrix representing the covariant projection operator.
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
    /// n-rosy variant of connection_laplacian(fid).
    ///
    /// @param[in] fid    Facet index.
    /// @param[in] n      Symmetry order of the rosy field (applies the connection n times).
    /// @param[in] lambda Weight of projection term for the 1-form inner product (default: 1).
    ///
    /// @return A (2*nf) x (2*nf) matrix representing the connection Laplacian operator.
    ///
    Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic>
    connection_laplacian_nrosy(Index fid, Index n, Scalar lambda = 1) const;

private:
    ///
    /// Compute per-vertex normals as the sum of incident facet vector areas and store them in the
    /// mesh.
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
