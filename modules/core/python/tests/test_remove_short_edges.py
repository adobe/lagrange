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

from .assets import single_triangle

class TestRemoveShortEdges:
    def test_triangle(self, single_triangle):
        mesh = single_triangle
        lagrange.remove_short_edges(mesh, 0.1)
        assert mesh.num_vertices == 3
        assert mesh.num_facets == 1

        lagrange.remove_short_edges(mesh, 10)
        assert mesh.num_vertices == 0
        assert mesh.num_facets == 0

    def test_mixed_elements(self):
        mesh = lagrange.SurfaceMesh()
        mesh.add_vertex([0, 0, 0])
        mesh.add_vertex([1, 0, 0])
        mesh.add_vertex([0, 1, 0])
        mesh.add_vertex([1, 1, 0])
        mesh.add_vertex([0, 0, 0.1])
        mesh.add_vertex([1, 0, 0.1])
        mesh.add_vertex([0, 1, 0.1])
        mesh.add_vertex([1, 1, 0.1])

        mesh.add_quad(0, 2, 3, 1);
        mesh.add_quad(4, 5, 7, 6);
        mesh.add_triangle(0, 1, 4);
        mesh.add_triangle(4, 1, 5);

        lagrange.remove_short_edges(mesh, 0.2)
        assert mesh.num_facets == 2
        assert mesh.num_vertices == 6
