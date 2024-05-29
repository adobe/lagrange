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
import pytest

from .assets import cube


class TestComputeEdgeLengths:
    def test_cube(self, cube):
        mesh = cube
        attr_id = lagrange.compute_edge_lengths(mesh, output_attribute_name="edge_lengths")
        assert mesh.has_attribute("edge_lengths")
        assert attr_id == mesh.get_attribute_id("edge_lengths")

        edge_lengths = mesh.attribute("edge_lengths").data
        assert edge_lengths.shape == (12,)
        assert np.all(edge_lengths == pytest.approx(1))
