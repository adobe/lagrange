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


class TestSelectFacetsByNormalSimilarity:
    def test_cube(self, single_triangle):
        mesh = single_triangle
        lagrange.select_facets_by_normal_similarity(mesh, 0, output_attribute_name="is_selected")
        is_selected = mesh.attribute("is_selected")
        assert is_selected.data[0] == 1
