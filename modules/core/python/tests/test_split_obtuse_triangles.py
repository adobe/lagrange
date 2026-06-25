#
# Copyright 2026 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#
import math

import numpy as np

import lagrange


def _max_interior_angle(mesh):
    V = mesh.vertices
    F = mesh.facets
    max_angle = 0.0
    for tri in F:
        p = V[tri]
        for d in range(3):
            v1 = p[d] - p[(d + 1) % 3]
            v2 = p[d] - p[(d + 2) % 3]
            n1 = v1 / np.linalg.norm(v1)
            n2 = v2 / np.linalg.norm(v2)
            angle = 2.0 * math.atan2(np.linalg.norm(n1 - n2), np.linalg.norm(n1 + n2))
            if angle > max_angle:
                max_angle = angle
    return max_angle


class TestSplitObtuseTriangles:
    def test_noop_equilateral(self):
        mesh = lagrange.SurfaceMesh()
        mesh.add_vertices(
            np.array([[0, 0, 0], [1, 0, 0], [0.5, math.sqrt(3) / 2, 0]], dtype=np.float64)
        )
        mesh.add_triangles(np.array([[0, 1, 2]], dtype=np.uint32))

        n = lagrange.split_obtuse_triangles(mesh)
        assert n == 0
        assert mesh.num_vertices == 3
        assert mesh.num_facets == 1

    def test_single_sliver(self):
        mesh = lagrange.SurfaceMesh()
        mesh.add_vertices(np.array([[0, 0, 0], [1, 0, 0], [0.5, 0.01, 0]], dtype=np.float64))
        mesh.add_triangles(np.array([[0, 1, 2]], dtype=np.uint32))

        n = lagrange.split_obtuse_triangles(mesh, max_iterations=1)
        assert n == 1
        assert mesh.num_vertices == 4
        assert mesh.num_facets == 2

    def test_recursive_convergence(self):
        mesh = lagrange.SurfaceMesh()
        mesh.add_vertices(
            np.array([[0, 0, 0], [10, 0, 0], [2, 5, 0], [5, -0.1, 0]], dtype=np.float64)
        )
        mesh.add_triangles(np.array([[0, 1, 2], [1, 0, 3]], dtype=np.uint32))

        n = lagrange.split_obtuse_triangles(mesh, max_angle=math.pi / 2, max_iterations=0)
        assert n > 0
        assert _max_interior_angle(mesh) <= math.pi / 2 + 1e-5
