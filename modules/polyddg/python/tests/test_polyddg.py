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
import pytest
import numpy as np


@pytest.fixture
def triangle():
    mesh = lagrange.SurfaceMesh()
    mesh.add_vertices(np.eye(3))
    mesh.add_triangle(0, 1, 2)
    assert mesh.num_vertices == 3
    assert mesh.num_facets == 1
    return mesh


@pytest.fixture
def nonplanar_quad():
    mesh = lagrange.SurfaceMesh()
    mesh.add_vertex([0, 0, 0])
    mesh.add_vertex([1, 0, 1])
    mesh.add_vertex([1, 1, 0])
    mesh.add_vertex([0, 1, 1])
    mesh.add_quad(0, 1, 2, 3)
    return mesh


@pytest.fixture
def pyramid():
    mesh = lagrange.SurfaceMesh()
    mesh.add_vertex([0, 0, 0])
    mesh.add_vertex([1, 0, 0])
    mesh.add_vertex([1, 1, 0])
    mesh.add_vertex([0, 1, 0])
    mesh.add_vertex([0.5, 0.5, 1])
    mesh.add_triangle(0, 1, 4)
    mesh.add_triangle(1, 2, 4)
    mesh.add_triangle(2, 3, 4)
    mesh.add_triangle(3, 0, 4)
    mesh.add_quad(0, 3, 2, 1)
    return mesh


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
