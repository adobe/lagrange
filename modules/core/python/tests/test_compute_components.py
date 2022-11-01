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

        opt = lagrange.ComponentOptions()
        opt.output_attribute_name = "id"
        opt.connectivity_type = lagrange.ConnectivityType.Vertex

        n = lagrange.compute_components(mesh, opt)
        assert n == 2
        assert mesh.has_attribute(opt.output_attribute_name)
        component_id = mesh.attribute(opt.output_attribute_name).data
        assert component_id[0] != component_id[1]

        # Add a trinagle connecting the two components via 2 vertices.
        mesh.add_triangle(0, 7, 4)
        n = lagrange.compute_components(mesh, opt)
        assert n == 1
        assert mesh.has_attribute(opt.output_attribute_name)
        component_id = mesh.attribute(opt.output_attribute_name).data
        assert np.all(component_id == 0)

        # Change connectivity to edge.
        opt.connectivity_type = lagrange.ConnectivityType.Edge
        n = lagrange.compute_components(mesh, opt)
        assert n == 3
