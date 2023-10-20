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


class TestRemoveTopologicallyDegenerateFacets:
    def test_empty_mesh(self):
        mesh = lagrange.SurfaceMesh()
        lagrange.remove_topologically_degenerate_facets(mesh)
        assert mesh.num_vertices == 0
        assert mesh.num_facets == 0

    def test_triangle(self):
        mesh = lagrange.SurfaceMesh()
        mesh.add_vertex([0, 0, 0])
        mesh.add_vertex([1, 0, 0])
        mesh.add_vertex([0, 1, 0])
        mesh.add_triangle(0, 1, 2)

        lagrange.remove_topologically_degenerate_facets(mesh)
        assert mesh.num_vertices == 3
        assert mesh.num_facets == 1

    def test_triangle_with_topological_degeneracy(self):
        mesh = lagrange.SurfaceMesh()
        mesh.add_vertex([0, 0, 0])
        mesh.add_vertex([1, 0, 0])
        mesh.add_vertex([0, 1, 0])
        mesh.add_triangle(0, 1, 2)
        mesh.add_triangle(0, 1, 1)
        mesh.add_triangle(1, 1, 1)

        lagrange.remove_topologically_degenerate_facets(mesh)
        assert mesh.num_vertices == 3
        assert mesh.num_facets == 1

    def test_quad_with_topological_degeneracy(self):
        mesh = lagrange.SurfaceMesh()
        mesh.add_vertex([0, 0, 0])
        mesh.add_vertex([1, 0, 0])
        mesh.add_vertex([0, 1, 0])
        mesh.add_vertex([1, 1, 0])
        mesh.add_quad(0, 1, 3, 2)

        lagrange.remove_topologically_degenerate_facets(mesh)
        assert mesh.num_vertices == 4
        assert mesh.num_facets == 1
