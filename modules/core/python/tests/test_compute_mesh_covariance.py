#
# Copyright 2024 Adobe. All rights reserved.
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
import numpy as np
import pytest

from .assets import cube, single_triangle


class TestComputeMeshCovariance:
    def test_triangle(self, single_triangle):
        mesh = single_triangle
        active_facets = np.array([0], dtype=np.uint8)
        mesh.wrap_as_attribute(
            "active_facets",
            lagrange.AttributeElement.Facet,
            lagrange.AttributeUsage.Vector,
            active_facets,
        )
        center = np.zeros(3)
        output = lagrange.compute_mesh_covariance(mesh, center, "active_facets")
        covariance = np.asarray(output)
        supposed_covariance = np.zeros(shape=(3, 3))
        assert np.linalg.norm(covariance - supposed_covariance) < 1e-10

    def test_cube(self, cube):
        mesh = cube.clone()
        lagrange.triangulate_polygonal_facets(mesh)
        M = np.array(lagrange.compute_mesh_covariance(mesh, np.zeros(3) + 0.5))
        assert M.shape == (3, 3)  # Size
        assert np.allclose(M, M.T)  # symmetric

        eig_values, eig_vectors = np.linalg.eig(M)
        assert np.all(eig_values >= 0)  # positive semi-definite
        assert np.allclose(eig_values, np.ones(3) * eig_values[0])  # isotropic
