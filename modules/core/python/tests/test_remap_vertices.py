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

import numpy as np
import pytest

from .assets import cube, cube_with_uv


class TestRemapVertices:
    def test_cube(self, cube):
        mesh = cube
        mapping = np.zeros(mesh.num_vertices, dtype=np.intc)
        lagrange.remap_vertices(mesh, mapping)
        assert mesh.num_vertices == 1
        assert mesh.num_facets == 6

    def test_identity(self, cube):
        mesh = cube
        old_vertices = np.copy(mesh.vertices)
        mapping = np.arange(mesh.num_vertices, dtype=np.intc)
        lagrange.remap_vertices(mesh, mapping)
        assert mesh.num_vertices == 8
        assert mesh.num_facets == 6
        assert np.all(old_vertices == mesh.vertices)

    def test_reverse(self, cube):
        mesh = cube
        old_vertices = np.copy(mesh.vertices)
        mapping = np.flip(np.arange(mesh.num_vertices, dtype=np.intc))
        lagrange.remap_vertices(mesh, mapping)
        assert mesh.num_vertices == 8
        assert mesh.num_facets == 6
        assert np.all(old_vertices == np.flip(mesh.vertices, axis=0))

    def test_invalid_mapping(self, cube):
        mesh = cube
        mapping = np.array([0, 0, 0, 0, 1, 1, 1, 3], dtype=np.intc)
        with pytest.raises(Exception):
            lagrange.remap_vertices(mesh, mapping)

    def test_with_uv(self, cube_with_uv):
        mesh = cube_with_uv
        assert mesh.has_attribute("uv")
        uv_attr = mesh.indexed_attribute("uv")
        uv_values = np.copy(uv_attr.values.data)
        uv_indices = np.copy(uv_attr.indices.data)

        mapping = np.array([0, 0, 0, 0, 1, 2, 3, 4], dtype=np.intc)
        lagrange.remap_vertices(mesh, mapping)
        assert mesh.num_vertices == 5
        assert mesh.num_facets == 6

        assert mesh.has_attribute("uv")
        uv_attr = mesh.indexed_attribute("uv")
        assert np.all(uv_attr.values.data == uv_values)
        assert np.all(uv_attr.indices.data == uv_indices)

    def test_policy(self, cube):
        mesh = cube
        id = mesh.create_attribute(
            "vertex_index",
            lagrange.AttributeElement.Vertex,
            lagrange.AttributeUsage.Scalar,
            np.arange(8, dtype=np.uint32),
            np.array([])
        )
        mapping = np.array([0, 0, 0, 0, 1, 2, 3, 4], dtype=np.intc)
        options = lagrange.RemapVerticesOptions()
        options.collision_policy_float = lagrange.CollisionPolicy.Average
        options.collision_policy_integral = lagrange.CollisionPolicy.KeepFirst

        lagrange.remap_vertices(mesh, mapping)
        id_attr = mesh.attribute(id)
        assert np.all(mesh.vertices[0] == [0.5, 0.5, 0])
        assert id_attr.data[0] == 0

        options.collision_policy_float = lagrange.CollisionPolicy.KeepFirst

        options.collision_policy_float = lagrange.CollisionPolicy.Error
        with pytest.raises(Exception):
            lagrange.remap_vertices(mesh, mapping)
