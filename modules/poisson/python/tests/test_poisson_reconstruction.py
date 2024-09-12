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

import math
import pytest
import numpy as np


def fibonacci_sphere(num_points=100):

    points = []
    phi = math.radians(math.sqrt(5.) - 1.)

    for i in range(num_points):
        y = 1 - (i / float(num_points - 1)) * 2
        radius = math.sqrt(1 - y * y)

        theta = phi * i

        x = math.cos(theta) * radius
        z = math.sin(theta) * radius

        points.append((x, y, z))

    return np.array(points, order="C", dtype=np.float64)


class TestPoissonReconstruction:
    def test_triangle(self):
        points = fibonacci_sphere()
        normals = points
        print(type(points), points.shape)
        mesh = lagrange.poisson.mesh_from_oriented_points(points, normals)
        lagrange.io.save_mesh("sphere.ply", mesh)
