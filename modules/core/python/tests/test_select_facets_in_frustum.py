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

from .assets import *


class TestSelectFacetsInFrustum:
    def test_cube_big_frustum(self, cube):
        mesh = cube
        points = np.array([[0, 0, -5566], [0, 0, 5566], [0, -5566, 0], [0, 5566, 0]])
        normals = np.array([[0, 0, 1], [0, 0, -1], [0, 1, 0], [0, -1, 0]])
        assert lagrange.select_facets_in_frustum(
            mesh, points, normals, output_attribute_name="is_selected"
        )
        is_selected = mesh.attribute("is_selected")
        assert (
            mesh.num_facets == np.asarray(is_selected.data).sum()
        )  # all facets should be selected
