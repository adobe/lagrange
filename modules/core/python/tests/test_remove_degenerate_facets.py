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
from .assets import cube, cube_triangular, cube_with_uv

import numpy as np


class TestRemoveDegenerateFacets:
    def test_empty_mesh(self):
        mesh = lagrange.SurfaceMesh()
        lagrange.remove_degenerate_facets(mesh)
        assert mesh.num_facets == 0
        assert mesh.num_vertices == 0

    def test_cube(self, cube_triangular):
        mesh = cube_triangular
        lagrange.remove_degenerate_facets(mesh)
        assert mesh.num_facets == 12

    def test_degenerate_cube(self, cube_triangular):
        mesh = cube_triangular.clone()
        vertices = mesh.vertices
        vertices[0:4] = np.zeros((4, 3))

        lagrange.remove_degenerate_facets(mesh)
        assert mesh.num_facets == 6
        assert mesh.num_vertices == 5

    def test_facet_attribute(self, cube_triangular):
        mesh = cube_triangular.clone()
        mesh.create_attribute("facet_index", initial_values=np.arange(12))
        vertices = mesh.vertices
        vertices[:, 2] = 0

        lagrange.remove_degenerate_facets(mesh)
        assert mesh.num_facets == 4
        assert mesh.num_vertices == 4

        assert mesh.has_attribute("facet_index")
        facet_indices = mesh.attribute("facet_index").data
        assert np.isin(facet_indices, [0, 1, 2, 3]).all()

    def test_indexed_attribute(self, cube_with_uv):
        mesh = cube_with_uv.clone()
        lagrange.triangulate_polygonal_facets(mesh)

        uv_id = mesh.get_matching_attribute_ids(usage=lagrange.AttributeUsage.UV)[0]
        uv_name = mesh.get_attribute_name(uv_id)

        vertices = mesh.vertices
        vertices[0:4] = np.zeros((4, 3))

        lagrange.remove_degenerate_facets(mesh)
        assert mesh.num_facets == 6
        assert mesh.num_vertices == 5

        assert mesh.has_attribute(uv_name)
        assert mesh.is_attribute_indexed(uv_name)

        uv_values = mesh.indexed_attribute(uv_name).values.data
        uv_indices = mesh.indexed_attribute(uv_name).indices.data.reshape((-1, 3))

        assert uv_values.shape == (14, 2)
        assert uv_indices.shape == (6, 3)

        uv_mesh = lagrange.SurfaceMesh(2)
        uv_mesh.add_vertices(uv_values)
        uv_mesh.add_triangles(uv_indices)
        lagrange.io.save_mesh("uv_mesh.obj", uv_mesh)

        degenerate_uv_facets = lagrange.detect_degenerate_facets(uv_mesh)
        assert len(degenerate_uv_facets) == 0
