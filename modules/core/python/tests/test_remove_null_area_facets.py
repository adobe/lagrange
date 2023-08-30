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
from .assets import cube

import numpy as np


class TestRemoveNullAreaFacets:
    def test_cube(self, cube):
        mesh = cube
        num_facets = mesh.num_facets
        lagrange.remove_null_area_facets(mesh)
        assert num_facets == mesh.num_facets

    def test_degenerate_cube(self, cube):
        mesh = cube
        vertices = mesh.vertices
        vertices[0:4] = np.zeros((4, 3))
        lagrange.remove_null_area_facets(mesh)
        assert mesh.num_facets == 5
