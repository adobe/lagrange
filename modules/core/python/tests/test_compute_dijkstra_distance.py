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

from .assets import single_triangle, cube


class TestComputeDijkstraDistance:
    def test_cube(self, cube):
        mesh = cube
        maybe_vertices = lagrange.compute_dijkstra_distance(
            mesh,
            seed_facet=0,
            barycentric_coords=[0.2, 0.3, 0.4, 0.1],
        )
        assert maybe_vertices is None
        assert mesh.has_attribute("@dijkstra_distance")

    def test_triangle(self, single_triangle):
        mesh = single_triangle
        maybe_vertices = lagrange.compute_dijkstra_distance(
            mesh, seed_facet=0, barycentric_coords=[0.2, 0.3, 0.5]
        )
        assert maybe_vertices is None
        assert mesh.has_attribute("@dijkstra_distance")

    def test_involved(self, single_triangle):
        mesh = single_triangle
        maybe_vertices = lagrange.compute_dijkstra_distance(
            mesh,
            seed_facet=0,
            barycentric_coords=[0.2, 0.3, 0.5],
            output_involved_vertices=True,
        )
        assert maybe_vertices is not None
        assert mesh.has_attribute("@dijkstra_distance")
