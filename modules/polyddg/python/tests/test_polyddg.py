#
# Copyright 2025 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#
import lagrange
import lagrange.polyddg
import numpy as np


class TestPolyDDG:
    """Tests for the PolyDDG discrete differential operators based on the lemmas
    in the PolyDDG paper.
    """

    def lemma_1(self, mesh):
        """Lemma 1: The discrete gradient operator is linear-precise."""
        diff_ops = lagrange.polyddg.DifferentialOperators(mesh)
        G = diff_ops.gradient()
        assert G.shape == (mesh.num_facets * mesh.dimension, mesh.num_vertices)

        s = np.ones(3).reshape(-1, 1)
        r = 0.5
        theta = mesh.vertices @ s + r

        vec_area = mesh.attribute(diff_ops.vector_area_attribute_id).data
        assert vec_area.shape == (mesh.num_facets, mesh.dimension)
        area = np.linalg.norm(vec_area, axis=1)

        grad_theta = (G @ theta).reshape(mesh.num_facets, mesh.dimension)
        for f in range(mesh.num_facets):
            g = grad_theta[f]
            n = vec_area[f] / area[f]
            expected_g = ((np.eye(mesh.dimension) - np.outer(n, n)) @ s).ravel()
            assert np.allclose(g, expected_g)

    def lemma_2(self, mesh):
        """Lemma 2: The discrete gradient is the sharp of the discrete exterior derivative."""
        diff_ops = lagrange.polyddg.DifferentialOperators(mesh)
        G = diff_ops.gradient()
        D = diff_ops.d0()
        S = diff_ops.sharp()

        assert np.allclose(G.todense(), (S @ D).todense())

    def lemma_4(self, mesh):
        """Lemma 4: flat followed by sharp projects a vector onto the tangent plane per-facet."""
        diff_ops = lagrange.polyddg.DifferentialOperators(mesh)

        vec_area = mesh.attribute(diff_ops.vector_area_attribute_id).data
        assert vec_area.shape == (mesh.num_facets, mesh.dimension)
        area = np.linalg.norm(vec_area, axis=1)

        for fid in range(mesh.num_facets):
            Ff = diff_ops.flat(fid)
            Sf = diff_ops.sharp(fid)
            n = vec_area[fid] / area[fid]

            assert np.allclose(Sf @ Ff, np.eye(mesh.dimension) - np.outer(n, n))

    def lemma_5(self, mesh):
        """Lemma 5: Projection of flat is 0 per-facet."""
        diff_ops = lagrange.polyddg.DifferentialOperators(mesh)
        for fid in range(mesh.num_facets):
            nf = mesh.get_facet_size(fid)
            Ff = diff_ops.flat(fid)
            Pf = diff_ops.projection(fid)
            assert np.allclose(Pf @ Ff, np.zeros((nf, mesh.dimension)))

    def lemma_6(self, mesh):
        """Lemma 6: Projection operator is idempotent per-facet."""
        diff_ops = lagrange.polyddg.DifferentialOperators(mesh)
        for fid in range(mesh.num_facets):
            Pf = diff_ops.projection(fid)
            assert np.allclose(Pf @ Pf, Pf)

    def lemma_7(self, mesh):
        """Lemma 7: Image of the projection operator is in the kernel of sharp operator
        per-facet."""
        diff_ops = lagrange.polyddg.DifferentialOperators(mesh)
        for fid in range(mesh.num_facets):
            nf = mesh.get_facet_size(fid)
            Pf = diff_ops.projection(fid)
            Sf = diff_ops.sharp(fid)
            assert np.allclose(Sf @ Pf, np.zeros((mesh.dimension, nf)))

    def test_triangle(self, triangle):
        self.lemma_1(triangle)
        self.lemma_2(triangle)
        self.lemma_4(triangle)
        self.lemma_5(triangle)
        self.lemma_6(triangle)
        self.lemma_7(triangle)

    def test_nonplanar_quad(self, nonplanar_quad):
        self.lemma_1(nonplanar_quad)
        self.lemma_2(nonplanar_quad)
        self.lemma_4(nonplanar_quad)
        self.lemma_5(nonplanar_quad)
        self.lemma_6(nonplanar_quad)
        self.lemma_7(nonplanar_quad)

    def test_pyramid(self, pyramid):
        self.lemma_1(pyramid)
        self.lemma_2(pyramid)
        self.lemma_4(pyramid)
        self.lemma_5(pyramid)
        self.lemma_6(pyramid)
        self.lemma_7(pyramid)


class TestHodgeDecomposition:
    """Tests for Hodge decomposition functions."""

    def test_hodge_decomposition_vector_field_basic(self, octahedron):
        """Test basic vector field decomposition - reconstruction property."""
        ops = lagrange.polyddg.DifferentialOperators(octahedron)

        # Create random vector field
        np.random.seed(42)
        nv = octahedron.num_vertices
        V_input = np.random.randn(nv, 3)

        # Store as attribute
        input_attr = "@test_vf_input"
        octahedron.create_attribute(
            input_attr,
            element=lagrange.AttributeElement.Vertex,
            usage=lagrange.AttributeUsage.Vector,
            initial_values=V_input,
        )

        # Decompose
        exact_id, coexact_id, harmonic_id = lagrange.polyddg.hodge_decomposition_vector_field(
            octahedron,
            ops,
            input_attribute=input_attr,
            exact_attribute="@test_exact",
            coexact_attribute="@test_coexact",
            harmonic_attribute="@test_harmonic",
        )

        # Get results
        V_exact = np.array(octahedron.attribute("@test_exact").data).reshape(-1, 3)
        V_coexact = np.array(octahedron.attribute("@test_coexact").data).reshape(-1, 3)
        V_harmonic = np.array(octahedron.attribute("@test_harmonic").data).reshape(-1, 3)

        # Verify reconstruction: V = V_exact + V_coexact + V_harmonic
        residual = V_input - V_exact - V_coexact - V_harmonic
        assert np.linalg.norm(residual) < 1e-12

    def test_hodge_decomposition_vector_field_decompose_and_reconstruct(self, octahedron):
        """Test that decomposition components can be accessed."""
        ops = lagrange.polyddg.DifferentialOperators(octahedron)

        np.random.seed(123)
        nv = octahedron.num_vertices
        V_input = np.random.randn(nv, 3)

        input_attr = "@test_vf_genus0"
        octahedron.create_attribute(
            input_attr,
            element=lagrange.AttributeElement.Vertex,
            usage=lagrange.AttributeUsage.Vector,
            initial_values=V_input,
        )

        exact_id, coexact_id, harmonic_id = lagrange.polyddg.hodge_decomposition_vector_field(
            octahedron,
            ops,
            input_attribute=input_attr,
            exact_attribute="@test_exact_g0",
            coexact_attribute="@test_coexact_g0",
            harmonic_attribute="@test_harmonic_g0",
        )

        # Verify all three components are created
        assert octahedron.has_attribute("@test_exact_g0")
        assert octahedron.has_attribute("@test_coexact_g0")
        assert octahedron.has_attribute("@test_harmonic_g0")

        # Verify they are vertex vector attributes
        V_exact = np.array(octahedron.attribute("@test_exact_g0").data).reshape(-1, 3)
        V_coexact = np.array(octahedron.attribute("@test_coexact_g0").data).reshape(-1, 3)
        V_harmonic = np.array(octahedron.attribute("@test_harmonic_g0").data).reshape(-1, 3)

        assert V_exact.shape == (nv, 3)
        assert V_coexact.shape == (nv, 3)
        assert V_harmonic.shape == (nv, 3)

        # Verify reconstruction holds
        residual = V_input - V_exact - V_coexact - V_harmonic
        assert np.linalg.norm(residual) < 1e-12

    def test_hodge_decomposition_vector_field_nrosy(self, octahedron):
        """Test n-rosy decomposition (n=4) creates valid output."""
        ops = lagrange.polyddg.DifferentialOperators(octahedron)

        np.random.seed(456)
        nv = octahedron.num_vertices
        V_input = np.random.randn(nv, 3)

        input_attr = "@test_vf_4rosy"
        octahedron.create_attribute(
            input_attr,
            element=lagrange.AttributeElement.Vertex,
            usage=lagrange.AttributeUsage.Vector,
            initial_values=V_input,
        )

        exact_id, coexact_id, harmonic_id = lagrange.polyddg.hodge_decomposition_vector_field(
            octahedron,
            ops,
            input_attribute=input_attr,
            exact_attribute="@test_exact_4",
            coexact_attribute="@test_coexact_4",
            harmonic_attribute="@test_harmonic_4",
            nrosy=4,
        )

        # Verify all three components are created
        assert octahedron.has_attribute("@test_exact_4")
        assert octahedron.has_attribute("@test_coexact_4")
        assert octahedron.has_attribute("@test_harmonic_4")

        # Verify they are vertex vector attributes
        V_exact = np.array(octahedron.attribute("@test_exact_4").data).reshape(-1, 3)
        V_coexact = np.array(octahedron.attribute("@test_coexact_4").data).reshape(-1, 3)
        V_harmonic = np.array(octahedron.attribute("@test_harmonic_4").data).reshape(-1, 3)

        assert V_exact.shape == (nv, 3)
        assert V_coexact.shape == (nv, 3)
        assert V_harmonic.shape == (nv, 3)

    def test_hodge_decomposition_1_form_basic(self, octahedron):
        """Test basic 1-form decomposition - reconstruction property."""
        ops = lagrange.polyddg.DifferentialOperators(octahedron)

        # Create random 1-form (per-edge scalar)
        np.random.seed(789)
        ne = octahedron.num_edges
        omega_input = np.random.randn(ne)

        # Store as attribute
        input_attr = "@test_1form_input"
        octahedron.create_attribute(
            input_attr,
            element=lagrange.AttributeElement.Edge,
            usage=lagrange.AttributeUsage.Scalar,
            initial_values=omega_input,
        )

        # Decompose
        exact_id, coexact_id, harmonic_id = lagrange.polyddg.hodge_decomposition_1_form(
            octahedron,
            ops,
            input_attribute=input_attr,
            exact_attribute="@test_1form_exact",
            coexact_attribute="@test_1form_coexact",
            harmonic_attribute="@test_1form_harmonic",
        )

        # Get results
        omega_exact = np.array(octahedron.attribute("@test_1form_exact").data)
        omega_coexact = np.array(octahedron.attribute("@test_1form_coexact").data)
        omega_harmonic = np.array(octahedron.attribute("@test_1form_harmonic").data)

        # Verify reconstruction: omega = omega_exact + omega_coexact + omega_harmonic
        residual = omega_input - omega_exact - omega_coexact - omega_harmonic
        assert np.linalg.norm(residual) < 1e-12

    def test_hodge_decomposition_1_form_harmonic_vanishes_genus_0(self, octahedron):
        """Test that harmonic 1-form vanishes on genus-0 surfaces."""
        ops = lagrange.polyddg.DifferentialOperators(octahedron)

        np.random.seed(101)
        ne = octahedron.num_edges
        omega_input = np.random.randn(ne)

        input_attr = "@test_1form_genus0"
        octahedron.create_attribute(
            input_attr,
            element=lagrange.AttributeElement.Edge,
            usage=lagrange.AttributeUsage.Scalar,
            initial_values=omega_input,
        )

        lagrange.polyddg.hodge_decomposition_1_form(
            octahedron,
            ops,
            input_attribute=input_attr,
            exact_attribute="@test_1form_exact_g0",
            coexact_attribute="@test_1form_coexact_g0",
            harmonic_attribute="@test_1form_harmonic_g0",
        )

        omega_harmonic = np.array(octahedron.attribute("@test_1form_harmonic_g0").data)
        assert np.linalg.norm(omega_harmonic) < 1e-10
