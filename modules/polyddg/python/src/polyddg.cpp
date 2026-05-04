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

#include <lagrange/polyddg/DifferentialOperators.h>
#include <lagrange/polyddg/compute_principal_curvatures.h>
#include <lagrange/polyddg/compute_smooth_direction_field.h>
#include <lagrange/polyddg/hodge_decomposition.h>
#include <lagrange/python/binding.h>
#include <lagrange/python/polyddg.h>

namespace nb = nanobind;
using namespace nb::literals;

namespace lagrange::python {

void populate_polyddg_module(nb::module_& m)
{
    using Scalar = double;
    using Index = uint32_t;

    nb::class_<polyddg::DifferentialOperators<Scalar, Index>>(
        m,
        "DifferentialOperators",
        "Polygonal mesh discrete differential operators")
        .def(
            nb::init<SurfaceMesh<Scalar, Index>&>(),
            "mesh"_a,
            R"(Construct the differential operators for a given mesh.

:param mesh: Input surface mesh (must be 3D).)")
        .def(
            "gradient",
            [](const polyddg::DifferentialOperators<Scalar, Index>& self) {
                return self.gradient();
            },
            R"(Compute the discrete polygonal gradient operator.

:return: A sparse matrix representing the gradient operator.)")
        .def(
            "d0",
            [](const polyddg::DifferentialOperators<Scalar, Index>& self) { return self.d0(); },
            R"(Compute the discrete polygonal d0 operator.

:return: A sparse matrix representing the d0 operator.)")
        .def(
            "d1",
            [](const polyddg::DifferentialOperators<Scalar, Index>& self) { return self.d1(); },
            R"(Compute the discrete polygonal d1 operator.

:return: A sparse matrix representing the d1 operator.)")
        .def(
            "star0",
            [](const polyddg::DifferentialOperators<Scalar, Index>& self) { return self.star0(); },
            R"(Compute the discrete Hodge star operator for 0-forms (diagonal mass matrix, size #V x #V).

:return: A diagonal sparse matrix of size (#V, #V).)")
        .def(
            "star1",
            [](const polyddg::DifferentialOperators<Scalar, Index>& self, Scalar lambda) {
                return self.star1(lambda);
            },
            "beta"_a = Scalar(1),
            R"(Compute the discrete Hodge star operator for 1-forms (size #E x #E).

Following de Goes, Butts and Desbrun (ACM Trans. Graph. 2020, Section 4.4), this is the
VEM-stabilized 1-form inner product assembled from per-face Gram matrices::

    M_f = area_f * U_f^T U_f  +  beta * P_f^T P_f

where ``U_f`` is the per-face sharp operator and ``P_f = I - V_f U_f`` projects onto the
kernel of ``U_f``.  The result is a symmetric positive-definite sparse matrix (non-diagonal
for polygonal meshes).  ``beta = 1`` is recommended and gives the best accuracy.

This is consistent with :meth:`star0` and :meth:`star2`, which also delegate to their
respective inner-product operators.

:param beta: Stabilization weight for the VEM projection term (default 1).
:return: A symmetric positive-definite sparse matrix of size (#E, #E).)")
        .def(
            "star2",
            [](const polyddg::DifferentialOperators<Scalar, Index>& self) { return self.star2(); },
            R"(Compute the discrete Hodge star operator for 2-forms (diagonal mass matrix, size #F x #F).

:return: A diagonal sparse matrix of size (#F, #F).)")
        .def(
            "flat",
            [](const polyddg::DifferentialOperators<Scalar, Index>& self) { return self.flat(); },
            R"(Compute the discrete polygonal flat operator.

:return: A sparse matrix representing the flat operator.)")
        .def(
            "inner_product_0_form",
            [](const polyddg::DifferentialOperators<Scalar, Index>& self) {
                return self.inner_product_0_form();
            },
            R"(Compute the discrete polygonal inner product operator for 0-forms.

:return: A sparse matrix representing the inner product operator for 0-forms.)")
        .def(
            "inner_product_1_form",
            [](const polyddg::DifferentialOperators<Scalar, Index>& self, Scalar beta) {
                return self.inner_product_1_form(beta);
            },
            nb::kw_only(),
            "beta"_a = 1,
            R"(Compute the discrete polygonal inner product operator for 1-forms.

:param beta: Weight of projection term (default: 1).

:return: A sparse matrix representing the inner product operator for 1-forms.)")
        .def(
            "inner_product_2_form",
            [](const polyddg::DifferentialOperators<Scalar, Index>& self) {
                return self.inner_product_2_form();
            },
            R"(Compute the discrete polygonal inner product operator for 2-forms.

:return: A sparse matrix representing the inner product operator for 2-forms.)")
        .def(
            "divergence",
            [](const polyddg::DifferentialOperators<Scalar, Index>& self, Scalar beta) {
                return self.divergence(beta);
            },
            nb::kw_only(),
            "beta"_a = 1,
            R"(Compute the discrete polygonal divergence operator.

:param beta: Weight of projection term for the 1-form inner product (default: 1).

:return: A sparse matrix representing the divergence operator.)")
        .def(
            "curl",
            [](const polyddg::DifferentialOperators<Scalar, Index>& self) { return self.curl(); },
            R"(Compute the discrete polygonal curl operator.

:return: A sparse matrix representing the curl operator.)")
        .def(
            "sharp",
            [](const polyddg::DifferentialOperators<Scalar, Index>& self) { return self.sharp(); },
            R"(Compute the discrete polygonal sharp operator.

:return: A sparse matrix representing the sharp operator.)")
        .def(
            "projection",
            [](const polyddg::DifferentialOperators<Scalar, Index>& self) {
                return self.projection();
            },
            R"(Compute the projection operator.

The projection operator measures the information loss when extracting the part of the
1-form associated with a vector field. It is a matrix of size #E by #E.

:return: A sparse matrix representing the projection operator.)")
        .def(
            "laplacian",
            [](const polyddg::DifferentialOperators<Scalar, Index>& self, Scalar beta) {
                return self.laplacian(beta);
            },
            nb::kw_only(),
            "beta"_a = 1,
            R"(Compute the discrete polygonal Laplacian operator.

:param beta: Weight of projection term for the 1-form inner product (default: 1).

:return: A sparse matrix representing the Laplacian operator.)")
        .def(
            "delta1",
            [](const polyddg::DifferentialOperators<Scalar, Index>& self, Scalar beta) {
                return self.delta1(beta);
            },
            nb::kw_only(),
            "beta"_a = 1,
            R"(Compute the discrete co-differential operator :math:`\delta_1` in weak form.

The co-differential is the formal adjoint of ``d0`` with respect to the 1-form inner product.
In this implementation ``delta1`` returns the weak-form operator :math:`d_0^T \cdot M_1`: when applied to a
primal per-edge scalar 1-form it produces a dual 0-form (an integrated scalar per vertex star),
not a pointwise primal per-vertex quantity. To obtain a primal 0-form one must apply the inverse
vertex mass matrix, e.g. :math:`M_0^{-1} \cdot \text{delta1}(\text{beta}) \cdot \alpha` for a 1-form :math:`\alpha`.

Equal to ``divergence(beta)`` in weak form.

:param beta: Weight of projection term for the 1-form inner product (default: 1).

:return: A sparse matrix of size (#V, #E) implementing :math:`d_0^T \cdot M_1`.)")
        .def(
            "delta2",
            [](const polyddg::DifferentialOperators<Scalar, Index>& self) { return self.delta2(); },
            R"(Compute the discrete co-differential operator (:math:`\delta_2 : \Omega^2 \to \Omega^1`).

The co-differential is the formal adjoint of ``d1`` with respect to the 2-form inner product.
It maps a per-facet scalar 2-form to a per-edge scalar 1-form.

:return: A sparse matrix of size (#E, #F).)")
        .def(
            "laplacian2",
            [](const polyddg::DifferentialOperators<Scalar, Index>& self) {
                return self.laplacian2();
            },
            R"(Compute the discrete Laplacian on 2-forms (:math:`\Delta_2 : \Omega^2 \to \Omega^2`).

Equal to ``d1 · delta2()``. Analogous to ``laplacian()`` but acting on per-facet
scalar 2-forms. Required for computing the co-exact component in Helmholtz-Hodge decomposition.

:return: A sparse matrix of size (#F, #F).)")
        .def(
            "laplacian1",
            [](const polyddg::DifferentialOperators<Scalar, Index>& self, Scalar beta) {
                return self.laplacian1(beta);
            },
            nb::kw_only(),
            "beta"_a = 1,
            R"(Compute the discrete Hodge Laplacian on 1-forms (:math:`\Delta_1 : \Omega^1 \to \Omega^1`).

Combines the exact part (``d0 · delta1(beta)``) and the co-exact part
(``delta2() · d1()``). Required for full Helmholtz-Hodge decomposition.
Distinct from ``connection_laplacian()``, which acts on tangent vector fields at vertices.

:param beta: Weight of projection term for the 1-form inner product (default: 1).

:return: A sparse matrix of size (#E, #E).)")
        .def(
            "vertex_tangent_coordinates",
            [](const polyddg::DifferentialOperators<Scalar, Index>& self) {
                return self.vertex_tangent_coordinates();
            },
            R"(Compute the per-vertex coordinate transformation from the global 3D frame to the local tangent basis at each vertex.

:return: A sparse matrix of size (#V * 2, #V * 3).)")
        .def(
            "facet_tangent_coordinates",
            [](const polyddg::DifferentialOperators<Scalar, Index>& self) {
                return self.facet_tangent_coordinates();
            },
            R"(Compute the per-facet coordinate transformation from the global 3D frame to the local tangent basis at each facet.

:return: A sparse matrix of size (#F * 2, #F * 3).)")
        .def(
            "covariant_derivative",
            [](const polyddg::DifferentialOperators<Scalar, Index>& self) {
                return self.covariant_derivative();
            },
            R"(Compute the discrete covariant derivative operator.

:return: A sparse matrix representing the covariant derivative operator.)")
        .def(
            "covariant_derivative_nrosy",
            [](const polyddg::DifferentialOperators<Scalar, Index>& self, Index n) {
                return self.covariant_derivative_nrosy(n);
            },
            nb::kw_only(),
            "n"_a,
            R"(n-rosy variant of covariant_derivative().

:param n: Symmetry order of the rosy field (applies the connection n times).

:return: A sparse matrix of size (#F * 4, #V * 2).)")
        .def(
            "shape_operator",
            [](const polyddg::DifferentialOperators<Scalar, Index>& self) {
                return self.shape_operator();
            },
            R"(Compute the global discrete shape operator (Eq. (23), de Goes et al. 2020).

Maps a per-vertex 3-D normal field to per-facet 2x2 symmetrized shape operators. The returned
sparse matrix has shape ``(#F * 4, #V * 3)`` with input layout ``[n_v^x, n_v^y, n_v^z, ...]``
per vertex and output layout ``[S(0,0), S(0,1), S(1,0), S(1,1), ...]`` per facet.

:return: A sparse matrix of shape ``(#F * 4, #V * 3)``)")
        .def(
            "adjoint_gradient",
            [](const polyddg::DifferentialOperators<Scalar, Index>& self) {
                return self.adjoint_gradient();
            },
            R"(Compute the global discrete adjoint gradient operator (Eq. (24), de Goes et al. 2020).

The vertex-centred dual of the per-facet gradient. Maps a per-facet scalar field to per-vertex
3-D tangent vectors. The returned sparse matrix has shape ``(#V * 3, #F)``.

:return: A sparse matrix of shape ``(#V * 3, #F)``)")
        .def(
            "adjoint_shape_operator",
            [](const polyddg::DifferentialOperators<Scalar, Index>& self) {
                return self.adjoint_shape_operator();
            },
            R"(Compute the global discrete adjoint shape operator (Eq. (26), de Goes et al. 2020).

The vertex-centred dual of the per-facet shape operator. Maps a per-facet 3-D normal field to
per-vertex 2x2 symmetrized shape operators. The returned sparse matrix has shape
``(#V * 4, #F * 3)`` with input layout ``[n_f^x, n_f^y, n_f^z, ...]`` per facet and output
layout ``[S(0,0), S(0,1), S(1,0), S(1,1), ...]`` per vertex.

:return: A sparse matrix of shape ``(#V * 4, #F * 3)``)")
        .def(
            "levi_civita",
            [](const polyddg::DifferentialOperators<Scalar, Index>& self) {
                return self.levi_civita();
            },
            R"(Compute the discrete Levi-Civita connection.

:return: A sparse matrix representing the Levi-Civita connection.)")
        .def(
            "levi_civita_nrosy",
            [](const polyddg::DifferentialOperators<Scalar, Index>& self, Index n) {
                return self.levi_civita_nrosy(n);
            },
            nb::kw_only(),
            "n"_a,
            R"(n-rosy variant of levi_civita().

:param n: Symmetry order of the rosy field (applies the connection n times).

:return: A sparse matrix of size (#C * 2, #V * 2).)")
        .def(
            "connection_laplacian",
            [](const polyddg::DifferentialOperators<Scalar, Index>& self, Scalar beta) {
                return self.connection_laplacian(beta);
            },
            nb::kw_only(),
            "beta"_a = 1,
            R"(Compute the discrete connection Laplacian operator.

:param beta: Weight of projection term for the 1-form inner product (default: 1).

:return: A sparse matrix representing the connection Laplacian operator.)")
        .def(
            "connection_laplacian_nrosy",
            [](const polyddg::DifferentialOperators<Scalar, Index>& self, Index n, Scalar beta) {
                return self.connection_laplacian_nrosy(n, beta);
            },
            nb::kw_only(),
            "n"_a,
            "beta"_a = 1,
            R"(n-rosy variant of connection_laplacian().

:param n: Symmetry order of the rosy field (applies the connection n times).
:param beta: Weight of projection term for the 1-form inner product (default: 1).

:return: A sparse matrix of size (#V * 2, #V * 2).)")
        .def(
            "gradient",
            [](const polyddg::DifferentialOperators<Scalar, Index>& self, Index fid) {
                return self.gradient(fid);
            },
            "fid"_a,
            R"(Compute the per-corner gradient vectors for a single facet (Eq. (8), de Goes et al. 2020).

Returns a ``(3, nf)`` matrix whose column ``l`` is ``(a_f x e_f^l) / (2 * |a_f|^2)``, where
``a_f`` is the facet vector area and ``e_f^l = x_{l-1} - x_{l+1}`` spans the opposite edge.

:param fid: Facet index.

:return: A dense matrix of shape ``(3, nf)``.)")
        .def(
            "d0",
            [](const polyddg::DifferentialOperators<Scalar, Index>& self, Index fid) {
                return self.d0(fid);
            },
            "fid"_a,
            R"(Compute the discrete d0 operator for a single facet.

The discrete d0 operator for a single facet is a n by n matrix, where n is the number vertices/edges
in the facet. It maps a scalar functions defined on the vertices to a 1-form defined on the edges.

:param fid: Facet index.

:return: A dense matrix representing the per-facet d0 operator.)")
        .def(
            "d1",
            [](const polyddg::DifferentialOperators<Scalar, Index>& self, Index fid) {
                return self.d1(fid);
            },
            "fid"_a,
            R"(Compute the discrete d1 operator for a single facet.

The discrete d1 operator for a single facet is a row vector of size 1 by n, where n is the number
edges in the facet. It maps a 1-form defined on the edges to a 2-form defined on the facet.

:param fid: Facet index.

:return: A dense matrix representing the per-facet d1 operator.)")
        .def(
            "flat",
            [](const polyddg::DifferentialOperators<Scalar, Index>& self, Index fid) {
                return self.flat(fid);
            },
            "fid"_a,
            R"(Compute the discrete flat operator for a single facet.

The discrete flat operator for a single facet is a n by 3 matrix, where n is the number of
edges of the facet. It maps a vector field defined on the facet to a 1-form defined on
the edges of the facet.

:param fid: Facet index.

:return: A Nx3 dense matrix representing the per-facet flat operator.)")
        .def(
            "sharp",
            [](const polyddg::DifferentialOperators<Scalar, Index>& self, Index fid) {
                return self.sharp(fid);
            },
            "fid"_a,
            R"(Compute the discrete sharp operator for a single facet.

:param fid: Facet index.

:return: A 3xN dense matrix representing the per-facet sharp operator.)")
        .def(
            "projection",
            [](const polyddg::DifferentialOperators<Scalar, Index>& self, Index fid) {
                return self.projection(fid);
            },
            "fid"_a,
            R"(Compute the discrete projection operator for a single facet.

:param fid: Facet index.

:return: A dense matrix representing the per-facet projection operator.)")
        .def(
            "inner_product_0_form",
            [](const polyddg::DifferentialOperators<Scalar, Index>& self, Index fid) {
                return self.inner_product_0_form(fid);
            },
            "fid"_a,
            R"(Compute the discrete inner product operator for 0-forms for a single facet.

:param fid: Facet index.

:return: A dense matrix representing the per-facet inner product operator for 0-forms.)")
        .def(
            "inner_product_1_form",
            [](const polyddg::DifferentialOperators<Scalar, Index>& self, Index fid, Scalar beta) {
                return self.inner_product_1_form(fid, beta);
            },
            "fid"_a,
            "beta"_a = 1,
            R"(Compute the discrete inner product operator for 1-forms for a single facet.

:param fid: Facet index.
:param beta: Weight of projection term (default: 1).

:return: A dense matrix representing the per-facet inner product operator for 1-forms.)")
        .def(
            "inner_product_2_form",
            [](const polyddg::DifferentialOperators<Scalar, Index>& self, Index fid) {
                return self.inner_product_2_form(fid);
            },
            "fid"_a,
            R"(Compute the discrete inner product operator for 2-forms for a single facet.

:param fid: Facet index.

:return: A 1x1 dense matrix representing the per-facet inner product operator for 2-forms.)")
        .def(
            "laplacian",
            [](const polyddg::DifferentialOperators<Scalar, Index>& self, Index fid, Scalar beta) {
                return self.laplacian(fid, beta);
            },
            "fid"_a,
            nb::kw_only(),
            "beta"_a = 1,
            R"(Compute the discrete Laplacian operator for a single facet.

:param fid: Facet index.
:param beta: Weight of projection term (default: 1).

:return: A dense matrix representing the per-facet Laplacian operator.)")
        .def(
            "levi_civita",
            [](const polyddg::DifferentialOperators<Scalar, Index>& self, Index fid, Index lv) {
                return self.levi_civita(fid, lv);
            },
            "fid"_a,
            "lv"_a,
            R"(Compute the discrete Levi-Civita connection from a vertex to a facet.

:param fid: Facet index.
:param lv: Local vertex index within the facet.

:return: A 2x2 dense matrix representing the vertex-to-facet Levi-Civita connection.)")
        .def(
            "levi_civita_nrosy",
            [](const polyddg::DifferentialOperators<Scalar, Index>& self,
               Index fid,
               Index lv,
               Index n) { return self.levi_civita_nrosy(fid, lv, n); },
            "fid"_a,
            "lv"_a,
            nb::kw_only(),
            "n"_a,
            R"(n-rosy variant of levi_civita(fid, lv).

:param fid: Facet index.
:param lv: Local vertex index within the facet.
:param n: Symmetry order of the rosy field (applies the connection n times).

:return: A 2x2 dense matrix representing the vertex-to-facet Levi-Civita connection.)")
        .def(
            "levi_civita",
            [](const polyddg::DifferentialOperators<Scalar, Index>& self, Index fid) {
                return self.levi_civita(fid);
            },
            "fid"_a,
            R"(Compute the discrete Levi-Civita connection for a single facet.

:param fid: Facet index.

:return: A dense matrix representing the per-facet Levi-Civita connection.)")
        .def(
            "levi_civita_nrosy",
            [](const polyddg::DifferentialOperators<Scalar, Index>& self, Index fid, Index n) {
                return self.levi_civita_nrosy(fid, n);
            },
            "fid"_a,
            nb::kw_only(),
            "n"_a,
            R"(n-rosy variant of levi_civita(fid).

:param fid: Facet index.
:param n: Symmetry order of the rosy field (applies the connection n times).

:return: A dense matrix of size ``(2*nf, 2*nf)`` representing the per-facet Levi-Civita connection.)")
        .def(
            "covariant_derivative",
            [](const polyddg::DifferentialOperators<Scalar, Index>& self, Index fid) {
                return self.covariant_derivative(fid);
            },
            "fid"_a,
            R"(Compute the discrete covariant derivative operator for a single facet.

:param fid: Facet index.

:return: A dense matrix representing the per-facet covariant derivative operator.)")
        .def(
            "covariant_derivative_nrosy",
            [](const polyddg::DifferentialOperators<Scalar, Index>& self, Index fid, Index n) {
                return self.covariant_derivative_nrosy(fid, n);
            },
            "fid"_a,
            "n"_a,
            R"(n-rosy variant of covariant_derivative(fid).

:param fid: Facet index.
:param n: Symmetry order of the rosy field (applies the connection n times).

:return: A dense matrix of size ``(4, 2*nf)`` representing the per-facet covariant derivative.)")
        .def(
            "shape_operator",
            [](const polyddg::DifferentialOperators<Scalar, Index>& self, Index fid) {
                return self.shape_operator(fid);
            },
            "fid"_a,
            R"(Compute the discrete shape operator for a single facet (Eq. (23), de Goes et al. 2020).

Applies the per-facet gradient to the precomputed per-vertex normals and symmetrizes the result
in the facet tangent plane. The returned 2x2 matrix is symmetric; its trace divided by two gives
the mean curvature at the facet, and its determinant gives the Gaussian curvature.

:param fid: Facet index.

:return: A 2x2 dense symmetric matrix.)")
        .def(
            "adjoint_gradient",
            [](const polyddg::DifferentialOperators<Scalar, Index>& self, Index vid) {
                return self.adjoint_gradient(vid);
            },
            "vid"_a,
            R"(Compute the adjoint gradient operator for a single vertex (Eq. (24), de Goes et al. 2020).

Returns a ``(3, k)`` dense matrix of area-weighted, parallel-transported per-corner gradient
vectors, where k is the number of incident faces. Columns are in the same incident-face traversal
order used by ``adjoint_shape_operator(vid)``.

:param vid: Vertex index.

:return: A dense matrix of shape ``(3, k)`` where k is the number of incident faces.)")
        .def(
            "adjoint_shape_operator",
            [](const polyddg::DifferentialOperators<Scalar, Index>& self, Index vid) {
                return self.adjoint_shape_operator(vid);
            },
            "vid"_a,
            R"(Compute the adjoint shape operator for a single vertex (Eq. (26), de Goes et al. 2020).

The vertex-centred dual of the per-facet shape operator. Applies the adjoint gradient to the unit
normals of the incident faces and symmetrizes the result in the vertex tangent plane. The returned
2x2 matrix is symmetric; its trace divided by two gives the mean curvature at the vertex, and its
determinant gives the Gaussian curvature.

:param vid: Vertex index.

:return: A 2x2 dense symmetric matrix.)")
        .def(
            "covariant_projection",
            [](const polyddg::DifferentialOperators<Scalar, Index>& self, Index fid) {
                return self.covariant_projection(fid);
            },
            "fid"_a,
            R"(Compute the discrete covariant projection operator for a single facet.

:param fid: Facet index.

:return: A dense matrix representing the per-facet covariant projection operator.)")
        .def(
            "covariant_projection_nrosy",
            [](const polyddg::DifferentialOperators<Scalar, Index>& self, Index fid, Index n) {
                return self.covariant_projection_nrosy(fid, n);
            },
            "fid"_a,
            "n"_a,
            R"(n-rosy variant of covariant_projection(fid).

:param fid: Facet index.
:param n: Symmetry order of the rosy field (applies the connection n times).

:return: A dense matrix of size ``(2*nf, 2*nf)`` representing the per-facet covariant projection.)")
        .def(
            "connection_laplacian",
            [](const polyddg::DifferentialOperators<Scalar, Index>& self, Index fid, Scalar beta) {
                return self.connection_laplacian(fid, beta);
            },
            "fid"_a,
            nb::kw_only(),
            "beta"_a = 1,
            R"(Compute the discrete connection Laplacian operator for a single facet.

:param fid: Facet index.
:param beta: Weight of projection term (default: 1).

:return: A dense matrix representing the per-facet connection Laplacian operator.)")
        .def(
            "connection_laplacian_nrosy",
            [](const polyddg::DifferentialOperators<Scalar, Index>& self,
               Index fid,
               Index n,
               Scalar beta) { return self.connection_laplacian_nrosy(fid, n, beta); },
            "fid"_a,
            nb::kw_only(),
            "n"_a,
            "beta"_a = 1,
            R"(n-rosy variant of connection_laplacian(fid).

:param fid: Facet index.
:param n: Symmetry order of the rosy field (applies the connection n times).
:param beta: Weight of projection term (default: 1).

:return: A dense matrix of size ``(2*nf, 2*nf)`` representing the per-facet connection Laplacian.)")
        .def_prop_ro(
            "vector_area_attribute_id",
            &polyddg::DifferentialOperators<Scalar, Index>::get_vector_area_attribute_id,
            "Attribute ID of the per-facet vector area attribute.")
        .def_prop_ro(
            "centroid_attribute_id",
            &polyddg::DifferentialOperators<Scalar, Index>::get_centroid_attribute_id,
            "Attribute ID of the per-facet centroid attribute.")
        .def_prop_ro(
            "vertex_normal_attribute_id",
            &polyddg::DifferentialOperators<Scalar, Index>::get_vertex_normal_attribute_id,
            "Attribute ID of the per-vertex normal attribute.")
        .def(
            "vertex_basis",
            &polyddg::DifferentialOperators<Scalar, Index>::vertex_basis,
            "vid"_a,
            R"(Compute the local tangent basis for a single vertex.

:param vid: Vertex index.
:return: A 3x2 matrix whose columns are orthonormal tangent vectors.)")
        .def(
            "facet_basis",
            &polyddg::DifferentialOperators<Scalar, Index>::facet_basis,
            "fid"_a,
            R"(Compute the local tangent basis for a single facet.

:param fid: Facet index.
:return: A 3x2 matrix whose columns are orthonormal tangent vectors.)");

    // Default attribute names are taken directly from PrincipalCurvaturesOptions to avoid
    // duplication if the defaults change.
    const polyddg::PrincipalCurvaturesOptions default_pc_opts{};

    m.def(
        "compute_principal_curvatures",
        [](SurfaceMesh<Scalar, Index>& mesh,
           const polyddg::DifferentialOperators<Scalar, Index>& ops,
           std::string_view kappa_min_attribute,
           std::string_view kappa_max_attribute,
           std::string_view direction_min_attribute,
           std::string_view direction_max_attribute) {
            polyddg::PrincipalCurvaturesOptions opts;
            opts.kappa_min_attribute = kappa_min_attribute;
            opts.kappa_max_attribute = kappa_max_attribute;
            opts.direction_min_attribute = direction_min_attribute;
            opts.direction_max_attribute = direction_max_attribute;
            auto r = polyddg::compute_principal_curvatures(mesh, ops, opts);
            return std::make_tuple(
                r.kappa_min_id,
                r.kappa_max_id,
                r.direction_min_id,
                r.direction_max_id);
        },
        "mesh"_a,
        "ops"_a,
        nb::kw_only(),
        "kappa_min_attribute"_a = default_pc_opts.kappa_min_attribute,
        "kappa_max_attribute"_a = default_pc_opts.kappa_max_attribute,
        "direction_min_attribute"_a = default_pc_opts.direction_min_attribute,
        "direction_max_attribute"_a = default_pc_opts.direction_max_attribute,
        R"(Compute per-vertex principal curvatures and principal curvature directions.

Eigendecomposes the adjoint shape operator at each vertex using a precomputed
:class:`DifferentialOperators` instance. The eigenvalues (``kappa_min <= kappa_max``) are the
principal curvatures and the eigenvectors, mapped back to 3-D through the vertex tangent basis,
are the principal directions. All four quantities are stored as vertex attributes in the mesh.

:param mesh: Input surface mesh (modified in place with new attributes).
:param ops: Precomputed :class:`DifferentialOperators` for the mesh.
:param kappa_min_attribute: Output attribute name for the minimum principal curvature
    (default: ``"@kappa_min"``).
:param kappa_max_attribute: Output attribute name for the maximum principal curvature
    (default: ``"@kappa_max"``).
:param direction_min_attribute: Output attribute name for the kappa_min direction
    (default: ``"@principal_direction_min"``).
:param direction_max_attribute: Output attribute name for the kappa_max direction
    (default: ``"@principal_direction_max"``).

:return: A tuple ``(kappa_min_id, kappa_max_id, direction_min_id, direction_max_id)`` of attribute IDs.)");

    m.def(
        "compute_principal_curvatures",
        [](SurfaceMesh<Scalar, Index>& mesh,
           std::string_view kappa_min_attribute,
           std::string_view kappa_max_attribute,
           std::string_view direction_min_attribute,
           std::string_view direction_max_attribute) {
            polyddg::PrincipalCurvaturesOptions opts;
            opts.kappa_min_attribute = kappa_min_attribute;
            opts.kappa_max_attribute = kappa_max_attribute;
            opts.direction_min_attribute = direction_min_attribute;
            opts.direction_max_attribute = direction_max_attribute;
            auto r = polyddg::compute_principal_curvatures(mesh, opts);
            return std::make_tuple(
                r.kappa_min_id,
                r.kappa_max_id,
                r.direction_min_id,
                r.direction_max_id);
        },
        "mesh"_a,
        nb::kw_only(),
        "kappa_min_attribute"_a = default_pc_opts.kappa_min_attribute,
        "kappa_max_attribute"_a = default_pc_opts.kappa_max_attribute,
        "direction_min_attribute"_a = default_pc_opts.direction_min_attribute,
        "direction_max_attribute"_a = default_pc_opts.direction_max_attribute,
        R"(Compute per-vertex principal curvatures and principal curvature directions.

Eigendecomposes the adjoint shape operator at each vertex. A :class:`DifferentialOperators`
instance is constructed internally. The eigenvalues (``kappa_min <= kappa_max``) are the
principal curvatures and the eigenvectors, mapped back to 3-D through the vertex tangent basis,
are the principal directions. All four quantities are stored as vertex attributes in the mesh.

:param mesh: Input surface mesh (modified in place with new attributes).
:param kappa_min_attribute: Output attribute name for the minimum principal curvature
    (default: ``"@kappa_min"``).
:param kappa_max_attribute: Output attribute name for the maximum principal curvature
    (default: ``"@kappa_max"``).
:param direction_min_attribute: Output attribute name for the kappa_min direction
    (default: ``"@principal_direction_min"``).
:param direction_max_attribute: Output attribute name for the kappa_max direction
    (default: ``"@principal_direction_max"``).

:return: A tuple ``(kappa_min_id, kappa_max_id, direction_min_id, direction_max_id)`` of attribute IDs.)");

    // ---- compute_smooth_direction_field ----
    const polyddg::SmoothDirectionFieldOptions default_sdf_opts{};

    m.def(
        "compute_smooth_direction_field",
        [](SurfaceMesh<Scalar, Index>& mesh,
           const polyddg::DifferentialOperators<Scalar, Index>& ops,
           uint8_t nrosy,
           double beta,
           std::string_view alignment_attribute,
           double alignment_weight,
           std::string_view direction_field_attribute) {
            polyddg::SmoothDirectionFieldOptions opts;
            opts.nrosy = nrosy;
            opts.lambda = beta;
            opts.alignment_attribute = alignment_attribute;
            opts.alignment_weight = alignment_weight;
            opts.direction_field_attribute = direction_field_attribute;
            return polyddg::compute_smooth_direction_field(mesh, ops, opts);
        },
        "mesh"_a,
        "ops"_a,
        nb::kw_only(),
        "nrosy"_a = default_sdf_opts.nrosy,
        "beta"_a = default_sdf_opts.lambda,
        "alignment_attribute"_a = default_sdf_opts.alignment_attribute,
        "alignment_weight"_a = default_sdf_opts.alignment_weight,
        "direction_field_attribute"_a = default_sdf_opts.direction_field_attribute,
        R"(Compute the globally smoothest n-direction field on a surface mesh.

Based on: Knöppel et al., "Globally optimal direction fields", ACM ToG 32(4), 2013.

Without alignment constraints (``alignment_attribute`` is empty), solves the generalized
eigenvalue problem :math:`L u = \sigma M u` for the smallest eigenvector. The result
minimizes the Dirichlet energy of the connection.

With alignment constraints, reads per-vertex prescribed 3-D tangent vectors from the given
attribute (zero-length vectors are unconstrained) and solves the shifted linear system
:math:`(L - \alpha M) u = M q`, where :math:`q` is the M-normalized prescribed field and
:math:`\alpha = \texttt{alignment\_lambda} \cdot \sigma_{\min}`.

:param mesh: Input surface mesh (modified in place with the new attribute).
:param ops: Precomputed :class:`DifferentialOperators` for the mesh.
:param nrosy: Symmetry order of the direction field (1 = vector field, 2 = line field,
    4 = cross field, default: 4).
:param beta: Stabilization weight for the VEM projection term in the connection Laplacian
    (default: 1).
:param alignment_attribute: Name of a per-vertex 3-D alignment vector attribute (zero =
    unconstrained). If empty, the unconstrained smoothest field is computed.
:param alignment_weight: Scaling factor for the spectral shift (default: 1). The actual
    shift is ``alignment_weight * sigma_min``, where ``sigma_min`` is the smallest eigenvalue
    of the connection Laplacian (computed automatically). Values in (0, 1) give weaker
    alignment (more smoothness).
:param direction_field_attribute: Output attribute name for the per-vertex 3-D direction
    field (default: ``"@smooth_direction_field"``).

:return: Attribute ID of the output per-vertex direction field.)");

    // ---- hodge_decomposition_1_form ----
    constexpr polyddg::HodgeDecompositionOptions default_hd_1form_opts{};

    m.def(
        "hodge_decomposition_1_form",
        [](SurfaceMesh<Scalar, Index>& mesh,
           const polyddg::DifferentialOperators<Scalar, Index>& ops,
           std::string_view input_attribute,
           std::string_view exact_attribute,
           std::string_view coexact_attribute,
           std::string_view harmonic_attribute,
           Scalar beta) {
            polyddg::HodgeDecompositionOptions opts;
            opts.input_attribute = input_attribute;
            opts.exact_attribute = exact_attribute;
            opts.coexact_attribute = coexact_attribute;
            opts.harmonic_attribute = harmonic_attribute;
            opts.lambda = beta;
            auto r = polyddg::hodge_decomposition_1_form(mesh, ops, opts);
            return std::make_tuple(r.exact_id, r.coexact_id, r.harmonic_id);
        },
        "mesh"_a,
        "ops"_a,
        nb::kw_only(),
        "input_attribute"_a = default_hd_1form_opts.input_attribute,
        "exact_attribute"_a = default_hd_1form_opts.exact_attribute,
        "coexact_attribute"_a = default_hd_1form_opts.coexact_attribute,
        "harmonic_attribute"_a = default_hd_1form_opts.harmonic_attribute,
        "beta"_a = Scalar(1),
        R"(Compute the Helmholtz-Hodge decomposition of a 1-form on a closed surface mesh.

Takes a discrete 1-form (per-edge scalar) and decomposes it into three orthogonal
components, each stored as a per-edge scalar attribute:

.. math::
    \omega = \omega_{\text{exact}} + \omega_{\text{coexact}} + \omega_{\text{harmonic}}

:param mesh: Input surface mesh (modified in place with new attributes). The input attribute
    (per-edge scalar) must already exist on the mesh.
:param ops: Precomputed :class:`DifferentialOperators` for the mesh.
:param input_attribute: Edge attribute name of the input 1-form
    (default: ``"@hodge_1form_input"``).
:param exact_attribute: Output edge attribute name for the exact component
    (default: ``"@hodge_1form_exact"``).
:param coexact_attribute: Output edge attribute name for the co-exact component
    (default: ``"@hodge_1form_coexact"``).
:param harmonic_attribute: Output edge attribute name for the harmonic component
    (default: ``"@hodge_1form_harmonic"``).
:param beta: Stabilization weight for the VEM 1-form inner product (default: 1).

:return: A tuple ``(exact_id, coexact_id, harmonic_id)`` of edge attribute IDs.)");

    m.def(
        "hodge_decomposition_1_form",
        [](SurfaceMesh<Scalar, Index>& mesh,
           std::string_view input_attribute,
           std::string_view exact_attribute,
           std::string_view coexact_attribute,
           std::string_view harmonic_attribute,
           Scalar beta) {
            polyddg::HodgeDecompositionOptions opts;
            opts.input_attribute = input_attribute;
            opts.exact_attribute = exact_attribute;
            opts.coexact_attribute = coexact_attribute;
            opts.harmonic_attribute = harmonic_attribute;
            opts.lambda = beta;
            auto r = polyddg::hodge_decomposition_1_form(mesh, opts);
            return std::make_tuple(r.exact_id, r.coexact_id, r.harmonic_id);
        },
        "mesh"_a,
        nb::kw_only(),
        "input_attribute"_a = default_hd_1form_opts.input_attribute,
        "exact_attribute"_a = default_hd_1form_opts.exact_attribute,
        "coexact_attribute"_a = default_hd_1form_opts.coexact_attribute,
        "harmonic_attribute"_a = default_hd_1form_opts.harmonic_attribute,
        "beta"_a = Scalar(1),
        R"(Compute the Helmholtz-Hodge decomposition of a 1-form on a closed surface mesh.

Convenience overload that constructs a :class:`DifferentialOperators` instance internally.

:param mesh: Input surface mesh (modified in place with new attributes). The input attribute
    (per-edge scalar) must already exist on the mesh.
:param input_attribute: Edge attribute name of the input 1-form
    (default: ``"@hodge_1form_input"``).
:param exact_attribute: Output edge attribute name for the exact component
    (default: ``"@hodge_1form_exact"``).
:param coexact_attribute: Output edge attribute name for the co-exact component
    (default: ``"@hodge_1form_coexact"``).
:param harmonic_attribute: Output edge attribute name for the harmonic component
    (default: ``"@hodge_1form_harmonic"``).
:param beta: Stabilization weight for the VEM 1-form inner product (default: 1).

:return: A tuple ``(exact_id, coexact_id, harmonic_id)`` of edge attribute IDs.)");

    // ---- hodge_decomposition_vector_field ----
    constexpr polyddg::HodgeDecompositionOptions default_hd_vf_opts{};

    m.def(
        "hodge_decomposition_vector_field",
        [](SurfaceMesh<Scalar, Index>& mesh,
           const polyddg::DifferentialOperators<Scalar, Index>& ops,
           std::string_view input_attribute,
           std::string_view exact_attribute,
           std::string_view coexact_attribute,
           std::string_view harmonic_attribute,
           Scalar beta,
           uint8_t nrosy) {
            polyddg::HodgeDecompositionOptions opts;
            opts.input_attribute = input_attribute;
            opts.exact_attribute = exact_attribute;
            opts.coexact_attribute = coexact_attribute;
            opts.harmonic_attribute = harmonic_attribute;
            opts.lambda = beta;
            opts.nrosy = nrosy;
            auto r = polyddg::hodge_decomposition_vector_field(mesh, ops, opts);
            return std::make_tuple(r.exact_id, r.coexact_id, r.harmonic_id);
        },
        "mesh"_a,
        "ops"_a,
        nb::kw_only(),
        "input_attribute"_a = default_hd_vf_opts.input_attribute,
        "exact_attribute"_a = default_hd_vf_opts.exact_attribute,
        "coexact_attribute"_a = default_hd_vf_opts.coexact_attribute,
        "harmonic_attribute"_a = default_hd_vf_opts.harmonic_attribute,
        "beta"_a = Scalar(1),
        "nrosy"_a = uint8_t(1),
        R"(Compute the Helmholtz-Hodge decomposition of a per-vertex vector field on a surface mesh.

Takes a per-vertex vector field in global 3D coordinates and decomposes it into three
orthogonal components, each stored as a per-vertex 3D vector attribute:

.. math::
    V = V_{\text{exact}} + V_{\text{coexact}} + V_{\text{harmonic}}

Internally, the vector field is converted to a 1-form and decomposed via
:func:`hodge_decomposition_1_form`, then each component is converted back.

When ``nrosy > 1``, the input is treated as one representative vector of an n-rosy field.

:param mesh: Input surface mesh (modified in place with new attributes). The input attribute
    (per-vertex 3D vector) must already exist on the mesh.
:param ops: Precomputed :class:`DifferentialOperators` for the mesh.
:param input_attribute: Vertex attribute name of the input vector field
    (default: ``"@hodge_input"``).
:param exact_attribute: Output vertex attribute name for the exact component
    (default: ``"@hodge_exact"``).
:param coexact_attribute: Output vertex attribute name for the co-exact component
    (default: ``"@hodge_coexact"``).
:param harmonic_attribute: Output vertex attribute name for the harmonic component
    (default: ``"@hodge_harmonic"``).
:param beta: Stabilization weight for the VEM 1-form inner product (default: 1).
:param nrosy: N-rosy symmetry order (default: 1 for plain vector fields).

:return: A tuple ``(exact_id, coexact_id, harmonic_id)`` of vertex attribute IDs.)");

    m.def(
        "hodge_decomposition_vector_field",
        [](SurfaceMesh<Scalar, Index>& mesh,
           std::string_view input_attribute,
           std::string_view exact_attribute,
           std::string_view coexact_attribute,
           std::string_view harmonic_attribute,
           Scalar beta,
           uint8_t nrosy) {
            polyddg::HodgeDecompositionOptions opts;
            opts.input_attribute = input_attribute;
            opts.exact_attribute = exact_attribute;
            opts.coexact_attribute = coexact_attribute;
            opts.harmonic_attribute = harmonic_attribute;
            opts.lambda = beta;
            opts.nrosy = nrosy;
            auto r = polyddg::hodge_decomposition_vector_field(mesh, opts);
            return std::make_tuple(r.exact_id, r.coexact_id, r.harmonic_id);
        },
        "mesh"_a,
        nb::kw_only(),
        "input_attribute"_a = default_hd_vf_opts.input_attribute,
        "exact_attribute"_a = default_hd_vf_opts.exact_attribute,
        "coexact_attribute"_a = default_hd_vf_opts.coexact_attribute,
        "harmonic_attribute"_a = default_hd_vf_opts.harmonic_attribute,
        "beta"_a = Scalar(1),
        "nrosy"_a = uint8_t(1),
        R"(Compute the Helmholtz-Hodge decomposition of a per-vertex vector field on a surface mesh.

Convenience overload that constructs a :class:`DifferentialOperators` instance internally.

When ``nrosy > 1``, the input is treated as one representative vector of an n-rosy field.

:param mesh: Input surface mesh (modified in place with new attributes). The input attribute
    (per-vertex 3D vector) must already exist on the mesh.
:param input_attribute: Vertex attribute name of the input vector field
    (default: ``"@hodge_input"``).
:param exact_attribute: Output vertex attribute name for the exact component
    (default: ``"@hodge_exact"``).
:param coexact_attribute: Output vertex attribute name for the co-exact component
    (default: ``"@hodge_coexact"``).
:param harmonic_attribute: Output vertex attribute name for the harmonic component
    (default: ``"@hodge_harmonic"``).
:param beta: Stabilization weight for the VEM 1-form inner product (default: 1).
:param nrosy: N-rosy symmetry order (default: 1 for plain vector fields).

:return: A tuple ``(exact_id, coexact_id, harmonic_id)`` of vertex attribute IDs.)");
}

} // namespace lagrange::python
