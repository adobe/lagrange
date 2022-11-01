#
# Copyright 2022 Adobe. All rights reserved.
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
from numpy.linalg import norm
import math
import pytest


class TestNormalizeMeshes:
    def test_empty_mesh(self):
        mesh = lagrange.SurfaceMesh()
        lagrange.normalize_mesh(mesh)
        assert mesh.num_vertices == 0

    def test_single_point(self):
        mesh = lagrange.SurfaceMesh()
        mesh.add_vertex([0, 0, 0])
        lagrange.normalize_mesh(mesh)
        assert mesh.num_vertices == 1

    def test_triangle(self):
        mesh = lagrange.SurfaceMesh()
        mesh.vertices = np.array([[0, 0, 0], [10, 0, 0], [0, 10, 0]])
        lagrange.normalize_mesh(mesh)
        bbox_max = np.amax(mesh.vertices, axis=0)
        bbox_min = np.amin(mesh.vertices, axis=0)
        bbox_center = (bbox_max + bbox_min) / 2
        assert bbox_center == pytest.approx(np.zeros(3))
        assert norm(bbox_max - bbox_min) == pytest.approx(2)

    def test_small_triangle(self):
        mesh = lagrange.SurfaceMesh()
        mesh.vertices = np.array([[0, 0, 0], [1e-3, 0, 0], [0, 1e-3, 0]])
        lagrange.normalize_mesh(mesh)
        bbox_max = np.amax(mesh.vertices, axis=0)
        bbox_min = np.amin(mesh.vertices, axis=0)
        bbox_center = (bbox_max + bbox_min) / 2
        assert bbox_center == pytest.approx(np.zeros(3))
        assert norm(bbox_max - bbox_min) == pytest.approx(2)

    def test_two_meshes(self):
        mesh1 = lagrange.SurfaceMesh()
        mesh1.vertices = np.array([[0, 0, 0], [1, 0, 0], [0, 1, 0]])

        mesh2 = lagrange.SurfaceMesh()
        mesh2.vertices = np.array(
            [
                [0, 0, 10],
                [1, 0, 10],
                [0, 1, 10],
            ]
        )

        lagrange.normalize_meshes([mesh1, mesh2])

        vertices = np.vstack([mesh1.vertices, mesh2.vertices])
        bbox_max = np.amax(vertices, axis=0)
        bbox_min = np.amin(vertices, axis=0)
        bbox_center = (bbox_max + bbox_min) / 2
        assert bbox_center == pytest.approx(np.zeros(3))
        assert norm(bbox_max - bbox_min) == pytest.approx(2)
