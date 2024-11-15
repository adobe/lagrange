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

from .assets import single_triangle


class TestSplitLongEdges:
    def test_triangle(self, single_triangle):
        mesh = lagrange.SurfaceMesh()
        mesh.add_vertex([0, 0, 0])
        mesh.add_vertex([1, 0, 0])
        mesh.add_vertex([0, 1, 0])
        mesh.add_triangle(0, 1, 2)

        # No split
        lagrange.split_long_edges(mesh, 10)
        assert mesh.num_vertices == 3
        assert mesh.num_facets == 1

        # Split with 0.5 threshold
        lagrange.split_long_edges(mesh, 0.5, recursive=False)
        assert mesh.num_vertices == 7
        assert mesh.num_facets == 5

        # Split with 0.5 threshold recursively
        lagrange.split_long_edges(mesh, 0.5, recursive=True)
        assert mesh.num_vertices == 9
        assert mesh.num_facets == 9

    def test_two_triangles(self):
        mesh = lagrange.SurfaceMesh()
        mesh.add_vertex([0, 0, 0])
        mesh.add_vertex([1, 0, 0])
        mesh.add_vertex([0, 1, 0])
        mesh.add_vertex([1, 1, 0])
        mesh.add_triangle(0, 1, 2)
        mesh.add_triangle(2, 1, 3)

        mesh.create_attribute("active", initial_values=[1, 0])
        lagrange.split_long_edges(mesh, 0.5, active_region_attribute="active", recursive=True)
        assert mesh.num_facets == 12

    def test_two_triangles_with_uv(self):
        mesh = lagrange.SurfaceMesh()
        mesh.add_vertex([0, 0, 0])
        mesh.add_vertex([1, 0, 0])
        mesh.add_vertex([0, 1, 0])
        mesh.add_vertex([1, 1, 0])
        mesh.add_triangle(0, 1, 2)
        mesh.add_triangle(2, 1, 3)

        mesh.create_attribute(
            "uv",
            usage=lagrange.AttributeUsage.UV,
            initial_values=np.array([[0, 0], [1, 0], [0, 1], [1, 1]], dtype=np.float32),
            initial_indices=np.array([[0, 1, 2], [2, 1, 3]], dtype=np.uint32),
        )
        lagrange.split_long_edges(mesh, 0.5, recursive=True)
        assert mesh.num_facets == 18
        assert mesh.has_attribute("uv")

        uv_attr = mesh.indexed_attribute("uv")
        uv_values = uv_attr.values.data
        uv_indices = uv_attr.indices.data

        assert np.allclose(mesh.vertices[mesh.facets.ravel(order="C"), :2], uv_values[uv_indices])
