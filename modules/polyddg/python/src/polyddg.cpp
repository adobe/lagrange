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
            R"(Compute the discrete Hodge star operator for 0-forms.

The Hodge star operator maps a k-form to a dual (n-k)-form, where n is the dimension of the manifold.

:return: A sparse matrix representing the discrete Hodge star operator for 0-forms.)")
        .def(
            "star1",
            [](const polyddg::DifferentialOperators<Scalar, Index>& self) { return self.star1(); },
            R"(Compute the discrete Hodge star operator for 1-forms.

The Hodge star operator maps a k-form to a dual (n-k)-form, where n is the dimension of the manifold.

:return: A sparse matrix representing the discrete Hodge star operator for 1-forms.)")
        .def(
            "star2",
            [](const polyddg::DifferentialOperators<Scalar, Index>& self) { return self.star2(); },
            R"(Compute the discrete Hodge star operator for 2-forms.

The Hodge star operator maps a k-form to a dual (n-k)-form, where n is the dimension of the manifold.

:return: A sparse matrix representing the discrete Hodge star operator for 2-forms.)")
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

:return: A sparse matrix representing the Laplacian operator.)")
        .def(
            "vertex_tangent_coordinates",
            [](const polyddg::DifferentialOperators<Scalar, Index>& self) {
                return self.vertex_tangent_coordinates();
            },
            R"(Compute the coordinate transformation that maps a per-vertex tangent vector field expressed in the global 3D coordinate to the local tangent basis at each vertex.

:return: A sparse matrix representing the coordinate transformation.)")
        .def(
            "facet_tangent_coordinates",
            [](const polyddg::DifferentialOperators<Scalar, Index>& self) {
                return self.facet_tangent_coordinates();
            },
            R"(Compute the coordinate transformation that maps a per-facet tangent vector field expressed in the global 3D coordinate to the local tangent basis at each facet.

:return: A sparse matrix representing the coordinate transformation.)")
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
            R"(Compute the discrete covariant derivative operator for n-rosy fields.

:param n: Number of times to apply the connection.

:return: A sparse matrix representing the covariant derivative operator.)")
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
            R"(Compute the discrete Levi-Civita connection for n-rosy fields.

:param n: Number of times to apply the connection.

:return: A sparse matrix representing the Levi-Civita connection.)")
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
            R"(Compute the discrete connection Laplacian operator for n-rosy fields.

:param n: Number of times to apply the connection.
:param beta: Weight of projection term for the 1-form inner product (default: 1).

:return: A sparse matrix representing the connection Laplacian operator.)")
        .def(
            "gradient",
            [](const polyddg::DifferentialOperators<Scalar, Index>& self, Index fid) {
                return self.gradient(fid);
            },
            "fid"_a,
            R"(Compute the discrete gradient operator for a single facet.

The discrete gradient operator for a single facet is a 3 by n vector, where n is the number vertices
in the facet. It maps a scalar functions defined on the vertices to a gradient vector defined on the
facet.

:param fid: Facet index.

:return: A dense matrix representing the per-facet gradient operator.)")
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
            R"(Compute the discrete Levi-Civita connection from a vertex to a facet for n-rosy fields.

:param fid: Facet index.
:param lv: Local vertex index within the facet.
:param n: Number of times to apply the connection.

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
            R"(Compute the discrete Levi-Civita connection for a single facet for n-rosy fields.

:param fid: Facet index.
:param n: Number of times to apply the connection.

:return: A dense matrix representing the per-facet Levi-Civita connection.)")
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
            R"(Compute the discrete covariant derivative operator for a single facet for n-rosy fields.

:param fid: Facet index.
:param n: Number of times to apply the connection.

:return: A dense matrix representing the per-facet covariant derivative operator.)")
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
            R"(Compute the discrete covariant projection operator for a single facet for n-rosy fields.

:param fid: Facet index.
:param n: Number of times to apply the connection.

:return: A dense matrix representing the per-facet covariant projection operator.)")
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
            R"(Compute the discrete connection Laplacian operator for a single facet for n-rosy fields.

:param fid: Facet index.
:param n: Number of times to apply the connection.
:param beta: Weight of projection term (default: 1).

:return: A dense matrix representing the per-facet connection Laplacian operator.)")
        .def_prop_ro(
            "vector_area_attribute_id",
            &polyddg::DifferentialOperators<Scalar, Index>::get_vector_area_attribute_id,
            "Get the attribute ID of the per-facet vector area attribute used in the differential "
            "operators.")
        .def_prop_ro(
            "centroid_attribute_id",
            &polyddg::DifferentialOperators<Scalar, Index>::get_centroid_attribute_id,
            "Get the attribute ID of the per-facet centroid attribute used in the differential "
            "operators.")
        .def_prop_ro(
            "vertex_normal_attribute_id",
            &polyddg::DifferentialOperators<Scalar, Index>::get_vertex_normal_attribute_id,
            "Get the attribute ID of the per-vertex normal attribute used in the differential "
            "operators.");
}

} // namespace lagrange::python
