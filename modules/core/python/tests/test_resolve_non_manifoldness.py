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


class TestRemoveVertexNonmanifoldness:
    def test_cube(self, cube):
        mesh = cube
        lagrange.resolve_vertex_nonmanifoldness(mesh)
        assert mesh.num_vertices == 8
        assert mesh.num_facets == 6

    def test_two_cubes(self, cube):
        cube2 = cube.clone()
        cube2.vertices[:] += 1
        mesh = lagrange.combine_meshes([cube, cube2])
        lagrange.remove_duplicate_vertices(mesh)

        assert (
            lagrange.compute_components(mesh, connectivity_type=lagrange.ConnectivityType.Vertex)
            == 1
        )

        lagrange.resolve_vertex_nonmanifoldness(mesh)
        assert (
            lagrange.compute_components(mesh, connectivity_type=lagrange.ConnectivityType.Vertex)
            == 2
        )
