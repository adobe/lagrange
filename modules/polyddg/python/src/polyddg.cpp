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
            [](const polyddg::DifferentialOperators<Scalar, Index>& self) { return self.star1(); },
            R"(Compute the discrete Hodge star operator for 1-forms (diagonal mass matrix, size #E x #E).

:return: A diagonal sparse matrix of size (#E, #E).)")
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
            "Attribute ID of the per-vertex normal attribute.");

    // Default attribute names are taken directly from PrincipalCurvaturesOptions to avoid
    // duplication if the defaults change.
    static const polyddg::PrincipalCurvaturesOptions default_pc_opts{};

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
}

} // namespace lagrange::python
