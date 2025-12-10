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

from .assets import single_triangle, cube, single_triangle_with_uv


class TestComputeFacetArea:
    def test_cube(self, cube):
        mesh = cube
        opt = lagrange.FacetAreaOptions()
        attr_id = lagrange.compute_facet_area(mesh, options=opt)
        assert mesh.get_attribute_name(attr_id) == opt.output_attribute_name

        area_attr = mesh.attribute(attr_id)
        assert area_attr.num_elements == 6
        assert area_attr.num_channels == 1
        assert np.all(area_attr.data == pytest.approx(1))

    def test_triangle(self, single_triangle):
        mesh = single_triangle
        opt = lagrange.FacetAreaOptions()
        attr_id = lagrange.compute_facet_area(mesh, options=opt)
        assert mesh.get_attribute_name(attr_id) == opt.output_attribute_name

        area_attr = mesh.attribute(attr_id)
        assert area_attr.num_elements == 1
        assert area_attr.num_channels == 1
        assert area_attr.data == pytest.approx(math.sqrt(3) / 2)

    def test_uv_triangle(self, single_triangle_with_uv):
        mesh = single_triangle_with_uv
        uv_area = lagrange.compute_uv_area(mesh)
        mesh_area = lagrange.compute_mesh_area(mesh)
        assert uv_area > 0
        assert mesh_area > 0

    def test_degenerate_triangle(self):
        mesh = lagrange.SurfaceMesh()
        mesh.add_vertex([0, 0, 0])
        mesh.add_vertex([1, 1, 1])
        mesh.add_vertex([2, 2, 2])
        mesh.add_triangle(0, 1, 2)

        attr_id = lagrange.compute_facet_area(mesh)
        area_attr = mesh.attribute(attr_id)
        assert area_attr.num_elements == 1
        assert area_attr.num_channels == 1
        assert area_attr.data == pytest.approx(0)

    def test_2D_triangle(self):
        mesh = lagrange.SurfaceMesh(2)
        mesh.add_vertex([0, 0])
        mesh.add_vertex([0, 1])
        mesh.add_vertex([1, 0])
        mesh.add_triangle(0, 1, 2)
        mesh.add_triangle(0, 2, 1)

        attr_id = lagrange.compute_facet_area(mesh)
        area_attr = mesh.attribute(attr_id)
        assert area_attr.num_elements == 2
        assert area_attr.num_channels == 1
        assert area_attr.data[0] == pytest.approx(-0.5)
        assert area_attr.data[1] == pytest.approx(0.5)
