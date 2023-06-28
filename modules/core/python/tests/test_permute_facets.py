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
import pytest


class TestPermuteFacets:
    def test_cube(self, cube):
        mesh = cube
        id = mesh.create_attribute(
            "facet_index",
            lagrange.AttributeElement.Facet,
            lagrange.AttributeUsage.Scalar,
            np.arange(6, dtype=np.int64),
        )
        ori_facets = mesh.facets.copy()

        permutation = np.array([3, 2, 1, 0, 5, 4], dtype=np.int64)
        lagrange.permute_facets(mesh, permutation)

        assert np.allclose(mesh.facets, ori_facets[permutation])

        new_facet_index = mesh.attribute(id).data
        assert np.all(new_facet_index == permutation)

    def test_bad_permuation(self, cube):
        mesh = cube
        permutation = np.array([0, 2, 1, 0, 5, 4], dtype=np.int64)

        with pytest.raises(RuntimeError):
            lagrange.permute_facets(mesh, permutation)
