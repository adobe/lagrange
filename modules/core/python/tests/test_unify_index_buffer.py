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

from .assets import cube

class TestUnifyIndexBuffer:
    def test_empty_mesh(self):
        input_mesh = lagrange.SurfaceMesh()
        output_mesh = lagrange.unify_index_buffer(input_mesh, [])
        assert output_mesh.num_vertices == 0

    def test_mesh_with_no_attributes(self, cube):
        mesh = cube
        output_mesh = lagrange.unify_index_buffer(mesh, [])
        assert output_mesh.num_vertices == mesh.num_vertices

    def test_mesh_with_attribute(self, cube):
        mesh = cube
        id = lagrange.compute_normal(mesh)
        assert mesh.is_attribute_indexed(id)

        output_mesh = lagrange.unify_index_buffer(mesh, [id])
        assert output_mesh.num_vertices != mesh.num_vertices
        assert output_mesh.num_vertices == 24
        assert output_mesh.num_facets == 6

        output_mesh2 = lagrange.unify_index_buffer(mesh, [mesh.get_attribute_name(id)])
        assert output_mesh2.num_vertices != mesh.num_vertices
        assert output_mesh2.num_vertices == 24
        assert output_mesh2.num_facets == 6

