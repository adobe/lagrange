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
import lagrange

import numpy as np
import pytest


class TestMeshBBox:
    def test_cube(self, cube):
        bbox_min, bbox_max = lagrange.mesh_bbox(cube)
        assert bbox_min == pytest.approx([0, 0, 0])
        assert bbox_max == pytest.approx([1, 1, 1])

    def test_single_triangle(self, single_triangle):
        bbox_min, bbox_max = lagrange.mesh_bbox(single_triangle)
        assert bbox_min == pytest.approx([0, 0, 0])
        assert bbox_max == pytest.approx([1, 1, 1])

    def test_2d(self):
        mesh = lagrange.SurfaceMesh(2)
        mesh.add_vertex([0, 0])
        mesh.add_vertex([2, 1])
        mesh.add_vertex([1, 3])
        mesh.add_triangle(0, 1, 2)
        bbox_min, bbox_max = lagrange.mesh_bbox(mesh)
        assert bbox_min.size == 2
        assert bbox_max.size == 2
        assert bbox_min == pytest.approx([0, 0])
        assert bbox_max == pytest.approx([2, 3])

    def test_empty(self):
        mesh = lagrange.SurfaceMesh()
        bbox_min, bbox_max = lagrange.mesh_bbox(mesh)
        assert bbox_min.size == 3
        assert bbox_max.size == 3
        assert np.all(bbox_min > bbox_max)
