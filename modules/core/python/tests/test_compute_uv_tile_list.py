#
# Copyright 2026 Adobe. All rights reserved.
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


class TestComputeUVTileList:
    def test_single_tile(self, cube_with_uv):
        tiles = lagrange.compute_uv_tile_list(cube_with_uv)
        assert set(tiles) == {(0, 0), (0, 1)}

    def test_no_uv(self, cube):
        tiles = lagrange.compute_uv_tile_list(cube)
        assert tiles == []

    def test_vertex_uv_last_element(self):
        mesh = lagrange.SurfaceMesh()
        mesh.add_vertex([0, 0, 0])
        mesh.add_vertex([1, 0, 0])
        mesh.add_vertex([1, 1, 0])
        mesh.add_vertex([0, 1, 0])
        mesh.add_triangle(0, 1, 2)
        mesh.add_triangle(0, 2, 3)
        mesh.create_attribute(
            "uv",
            element=lagrange.AttributeElement.Vertex,
            usage=lagrange.AttributeUsage.UV,
            initial_values=np.array([[0.1, 0.1], [0.2, 0.2], [0.3, 0.3], [2.5, 2.5]]),
        )
        tiles = lagrange.compute_uv_tile_list(mesh)
        assert set(tiles) == {(0, 0), (2, 2)}
