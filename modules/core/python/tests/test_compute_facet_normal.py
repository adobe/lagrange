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


class TestComputeFacetNormal:
    def validate_facet_normal(self, mesh, normal_attr):
        normals = normal_attr.data
        vertices = mesh.vertices
        facets = mesh.facets

        for fi in range(mesh.num_facets):
            n = normals[fi]
            centroid = np.average(vertices[facets[fi]], axis=0)
            dot = np.dot(vertices[facets[fi]] - centroid, n)
            assert np.all(dot == pytest.approx(0))

    def test_cube(self, cube):
        mesh = cube
        opt = lagrange.FacetNormalOptions()
        attr_id = lagrange.compute_facet_normal(mesh, options=opt)
        assert mesh.get_attribute_name(attr_id) == opt.output_attribute_name

        normal_attr = mesh.attribute(attr_id)
        assert normal_attr.num_elements == 6
        assert normal_attr.num_channels == 3
        self.validate_facet_normal(mesh, normal_attr)
