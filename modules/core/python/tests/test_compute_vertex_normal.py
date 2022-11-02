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
import math
import pytest

from .assets import cube

class TestComputeVertexNormal:
    def test_cube(self, cube):
        mesh = cube
        opt = lagrange.VertexNormalOptions()
        attr_id = lagrange.compute_vertex_normal(mesh, opt)
        assert mesh.get_attribute_name(attr_id) == opt.output_attribute_name

        normal_attr = mesh.attribute(attr_id)
        assert normal_attr.num_elements == 8
        assert normal_attr.num_channels == 3

        for fi in range(mesh.num_facets):
            n = normal_attr.data[fi]
            assert np.all(np.absolute(n) == pytest.approx(np.ones(3) / math.sqrt(3)))
