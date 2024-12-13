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

from .assets import single_triangle, cube


class TestThickenAndCloseMesh:
    def test_triangle(self, single_triangle):
        mesh = single_triangle
        mesh = lagrange.thicken_and_close_mesh(mesh, 0.1)
        assert mesh.num_vertices == 6
        assert mesh.num_facets == 8

    def test_cube(self, cube):
        mesh = cube
        mesh2 = lagrange.thicken_and_close_mesh(mesh, 0.1)
        assert mesh2.num_vertices == mesh.num_vertices * 2
        assert mesh2.num_facets == mesh.num_facets * 2
