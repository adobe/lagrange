#
# Copyright 2022 Adobe. All rights reserved.
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

from .assets import cube


class TestComputeComponents:
    def test_empty(self):
        mesh = lagrange.SurfaceMesh()
        n = lagrange.compute_components(mesh)
        assert n == 0

    def test_single_point(self):
        # Because components are facet-based, point cloud should always result in 0 components.
        mesh = lagrange.SurfaceMesh()
        mesh.add_vertex([0, 0, 0])
        n = lagrange.compute_components(mesh)
        assert n == 0

    def test_two_points(self):
        # Because components are facet-based, point cloud should always result in 0 components.
        mesh = lagrange.SurfaceMesh()
        mesh.add_vertex([0, 0, 0])
        mesh.add_vertex([1, 0, 0])
        n = lagrange.compute_components(mesh)
        assert n == 0

    def test_single_cube(self, cube):
        mesh = cube
        n = lagrange.compute_components(mesh)
        assert n == 1

    def test_two_cubes(self, cube):
        mesh = lagrange.combine_meshes([cube, cube], preserve_attributes=False)
        n = lagrange.compute_components(mesh)
        assert n == 2

    def test_mixed(self):
        mesh = lagrange.SurfaceMesh()
        mesh.vertices = np.array(
            [
                [0, 0, 0],
                [1, 0, 0],
                [0, 1, 0],
                [1, 1, 0],
                [0, 0, 1],
                [1, 0, 1],
                [1, 1, 1],
                [0, 1, 1],
            ],
            dtype=float,
        )
        mesh.add_quad(0, 1, 3, 2)
        mesh.add_triangle(4, 5, 6)

        n = lagrange.compute_components(
            mesh,
            output_attribute_name="id",
            connectivity_type=lagrange.ConnectivityType.Vertex,
        )
        assert n == 2
        assert mesh.has_attribute("id")
        component_id = mesh.attribute("id").data
        assert component_id[0] != component_id[1]

        # Add a trinagle connecting the two components via 2 vertices.
        mesh.add_triangle(0, 7, 4)
        n = lagrange.compute_components(
            mesh,
            output_attribute_name="id",
            connectivity_type=lagrange.ConnectivityType.Vertex,
        )
        assert n == 1
        assert mesh.has_attribute("id")
        component_id = mesh.attribute("id").data
        assert np.all(component_id == 0)

        # Change connectivity to edge.
        n = lagrange.compute_components(
            mesh,
            output_attribute_name="id",
            connectivity_type=lagrange.ConnectivityType.Edge,
        )
        assert n == 3

        # Add a quad connecting the two components via edge.
        mesh.remove_facets([2])
        assert mesh.num_facets == 2
        mesh.add_quad(2, 3, 5, 4)
        assert mesh.num_facets == 3
        n = lagrange.compute_components(
            mesh,
            output_attribute_name="id",
            connectivity_type=lagrange.ConnectivityType.Edge,
        )
        assert n == 1

        # With a blocker edge.
        eid = mesh.find_edge_from_vertices(2, 3)
        n = lagrange.compute_components(
            mesh,
            output_attribute_name="id",
            connectivity_type=lagrange.ConnectivityType.Edge,
            blocker_elements=[eid],
        )
        assert n == 2

        # With two blocker edges.
        eid2 = mesh.find_edge_from_vertices(5, 4)
        n = lagrange.compute_components(
            mesh,
            output_attribute_name="id",
            connectivity_type=lagrange.ConnectivityType.Edge,
            blocker_elements=[eid, eid2],
        )
        assert n == 3
