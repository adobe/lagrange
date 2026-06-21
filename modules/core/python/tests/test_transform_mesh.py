#
# Copyright 2023 Adobe. All rights reserved.
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


class TestTransformMesh:
    def test_identity(self, single_triangle):
        mesh = single_triangle
        orig_pos = mesh.vertices.copy()
        M = np.eye(4)

        r = lagrange.transform_mesh(mesh, M)
        assert r is None
        assert np.allclose(mesh.vertices, orig_pos)

        r = lagrange.transform_mesh(mesh, M, in_place=False)
        assert isinstance(r, lagrange.SurfaceMesh)
        assert np.allclose(r.vertices, orig_pos)

    def test_rotation(self, single_triangle):
        mesh = single_triangle
        normal_attr_id = lagrange.compute_facet_normal(mesh)
        orig_pos = mesh.vertices.copy()
        orig_normals = mesh.attribute(normal_attr_id).data.copy()
        M = np.array(
            [
                [0.0, 1.0, 0.0, 0.0],
                [0.0, 0.0, 1.0, 0.0],
                [1.0, 0.0, 0.0, 0.0],
                [0.0, 0.0, 0.0, 1.0],
            ]
        )

        lagrange.transform_mesh(mesh, M)
        expected_pos = orig_pos[:, [1, 2, 0]]
        assert np.allclose(mesh.vertices, expected_pos)

        expected_normals = orig_normals[:, [1, 2, 0]]
        normals = mesh.attribute(normal_attr_id).data
        assert np.allclose(normals, expected_normals)

    def test_reorient(self, single_triangle):
        # Reflection along the x-axis: negative determinant.
        M = np.diag([-1.0, 1.0, 1.0, 1.0])
        assert np.linalg.det(M[:3, :3]) < 0

        # reorient=True flips facet winding and reorients the facet normal.
        mesh = single_triangle
        normal_attr_id = lagrange.compute_facet_normal(mesh)
        orig_facets = mesh.facets.copy()

        lagrange.transform_mesh(mesh, M, reorient=True)

        # Facet winding is reversed (the triangle's vertex order is flipped).
        assert np.all(mesh.facets[0] == orig_facets[0][::-1])

        # The stored normal matches a freshly recomputed normal of the
        # reflected mesh, i.e. it is consistent with the flipped winding.
        stored_normals = mesh.attribute(normal_attr_id).data.copy()
        recomputed_attr_id = lagrange.compute_facet_normal(mesh)
        recomputed_normals = mesh.attribute(recomputed_attr_id).data
        assert np.allclose(stored_normals, recomputed_normals)

    def test_no_reorient(self, single_triangle):
        # Reflection along the x-axis: negative determinant.
        M = np.diag([-1.0, 1.0, 1.0, 1.0])

        # reorient=False (the default) leaves facet winding untouched.
        mesh = single_triangle
        orig_facets = mesh.facets.copy()

        lagrange.transform_mesh(mesh, M)

        assert np.all(mesh.facets == orig_facets)
