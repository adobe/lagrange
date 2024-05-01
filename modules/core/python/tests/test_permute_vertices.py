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

from .assets import cube, cube_with_uv


class TestPermuteVertices:
    def test_cube(self, cube):
        mesh = cube
        old_vertices = np.copy(mesh.vertices)

        order = np.arange(0, mesh.num_vertices)
        lagrange.permute_vertices(mesh, order)
        assert np.all(mesh.vertices == old_vertices)

        order = np.flip(order)
        lagrange.permute_vertices(mesh, order)
        assert np.all(mesh.vertices == np.flip(old_vertices, axis=0))

    def test_invalid_permutation(self, cube):
        mesh = cube
        order = np.arange(0, mesh.num_vertices)
        order[-1] = 0
        with pytest.raises(Exception):
            lagrange.permute_vertices(mesh, order)

    def test_with_uv(self, cube_with_uv):
        mesh = cube_with_uv
        mesh.initialize_edges()
        mesh.create_attribute(
            name="corner_index",
            element=lagrange.AttributeElement.Corner,
            usage=lagrange.AttributeUsage.Scalar,
            initial_values=np.arange(0, mesh.num_corners, dtype=np.intc),
            initial_indices=np.array([], dtype=np.uint32),
        )
        assert mesh.has_attribute("uv")
        assert mesh.has_attribute("corner_index")
        uv_attr = mesh.indexed_attribute("uv")
        uv_coords = np.copy(uv_attr.values.data)
        uv_indices = np.copy(uv_attr.indices.data)

        order = np.arange(0, mesh.num_vertices)
        order = np.flip(order)
        lagrange.permute_vertices(mesh, order)

        # UV should be unchanged
        assert mesh.has_edges
        uv_attr = mesh.indexed_attribute("uv")
        assert np.all(uv_attr.values.data == uv_coords)
        assert np.all(uv_attr.indices.data == uv_indices)

        # Corner index should be unchnaged.
        corner_index_attr = mesh.attribute("corner_index")
        assert np.all(
            corner_index_attr.data == np.arange(mesh.num_corners, dtype=np.intc)
        )

        for i in range(mesh.num_vertices):
            ci = mesh.get_first_corner_around_vertex(i)
            while ci < mesh.num_corners:
                assert mesh.get_corner_vertex(ci) == i
                ci = mesh.get_next_corner_around_vertex(ci)
