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

from .assets import single_triangle, cube


class TestComputeNormal:
    def validate_normal(self, mesh, normal_attr):
        normal_values = normal_attr.values.data
        normal_indices = normal_attr.indices.data

        vertices = mesh.vertices
        facets = mesh.facets

        for ci in range(mesh.num_corners):
            fi = mesh.get_corner_facet(ci)
            n = normal_values[normal_indices[ci]]
            centroid = np.average(vertices[facets[fi]], axis=0)
            dot = np.dot(vertices[facets[fi]] - centroid, n)
            assert np.all(dot == pytest.approx(0))

    def test_simple(self, single_triangle):
        mesh = single_triangle
        opt = lagrange.NormalOptions()
        attr_id = lagrange.compute_normal(
            mesh=mesh,
            feature_angle_threshold=math.pi / 4,
            options=opt,
        )

        normal_attr = mesh.indexed_attribute(attr_id)
        self.validate_normal(mesh, normal_attr)

    def test_cube(self, cube):
        mesh = cube
        attr_id = lagrange.compute_normal(mesh)

        normal_attr = mesh.indexed_attribute(attr_id)
        self.validate_normal(mesh, normal_attr)
