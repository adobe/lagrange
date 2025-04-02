#
# Copyright 2025 Adobe. All rights reserved.
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
import logging

from .assets import cube

class TestMeshSmoothing:
    def test_cube(self, cube):
        assert cube.num_vertices == 8
        assert cube.num_facets == 6
        lagrange.filtering.mesh_smoothing(cube)
        assert cube.num_vertices == 8
        assert cube.num_facets == 6
