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
from .assets import single_triangle

import numpy as np
import pytest


class TestIsoline:
    def test_trim(self, single_triangle):
        mesh = single_triangle
        id = mesh.create_attribute("value", "Vertex", "Scalar", np.array([0, 0, 1], dtype=float))
        trimmed = lagrange.trim_by_isoline(mesh, id, isovalue=0.5)
        assert mesh.num_vertices == 3
        assert mesh.num_facets == 1
        assert trimmed.num_vertices == 4
        assert trimmed.num_facets == 1
        assert trimmed.is_quad_mesh

    def test_extract(self, single_triangle):
        mesh = single_triangle
        id = mesh.create_attribute("value", "Vertex", "Scalar", np.array([0, 0, 1], dtype=float))
        trimmed = lagrange.extract_isoline(mesh, id, isovalue=0.5)
        assert mesh.num_vertices == 3
        assert mesh.num_facets == 1
        assert trimmed.num_vertices == 2
        assert trimmed.num_facets == 1
        assert trimmed.vertex_per_facet == 2
