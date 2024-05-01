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
from numpy.linalg import norm
import math
import pytest

from .assets import single_triangle, cube


class TestComputeCentroid:
    def test_cube(self, cube):
        mesh = cube
        c = lagrange.compute_mesh_centroid(mesh)
        assert np.all(c == [0.5, 0.5, 0.5])

    def test_triangle(self, single_triangle):
        mesh = single_triangle
        c = lagrange.compute_mesh_centroid(mesh)
        assert np.all(c == [1 / 3, 1 / 3, 1 / 3])

    def test_empty_mesh(self):
        mesh = lagrange.SurfaceMesh()
        with pytest.raises(Exception):
            c = lagrange.compute_mesh_centroid(mesh)
