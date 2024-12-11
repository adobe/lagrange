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
from .assets import cube

import numpy as np
import pytest


class TestOrientOutward:
    def test_cube(self, cube):
        mesh1 = cube.clone()
        lagrange.orient_outward(mesh1)
        mesh2 = cube.clone()
        lagrange.orient_outward(mesh2, positive=False)

        assert not np.allclose(mesh1.facets, mesh2.facets)
