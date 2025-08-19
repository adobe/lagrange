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

from .assets import cube


class TestWeldIndexedAttribute:
    def test_unique_values(self, cube):
        mesh = cube
        values = np.arange(mesh.num_corners)
        indices = np.arange(mesh.num_corners)

        id = mesh.create_attribute(
            "tmp",
            element=lagrange.AttributeElement.Indexed,
            usage=lagrange.AttributeUsage.Scalar,
            initial_values=values,
            initial_indices=indices,
        )

        lagrange.weld_indexed_attribute(mesh, id)

        attr = mesh.indexed_attribute(id)
        assert len(attr.values.data) == mesh.num_corners

    def test_same_values(self, cube):
        mesh = cube
        values = np.ones(mesh.num_corners)
        indices = np.arange(mesh.num_corners)

        id = mesh.create_attribute(
            "tmp",
            element=lagrange.AttributeElement.Indexed,
            usage=lagrange.AttributeUsage.Scalar,
            initial_values=values,
            initial_indices=indices,
        )

        lagrange.weld_indexed_attribute(mesh, id)

        attr = mesh.indexed_attribute(id)
        assert len(attr.values.data) == mesh.num_vertices

    def test_angle_threshold_small(self, cube):
        mesh = cube
        id = lagrange.compute_facet_normal(mesh)
        id = lagrange.map_attribute_in_place(mesh, id, lagrange.AttributeElement.Indexed)

        lagrange.weld_indexed_attribute(mesh, id, epsilon_abs=2, angle_abs=0)
        attr = mesh.indexed_attribute(id)
        assert len(attr.values.data) == mesh.num_facets

    def test_angle_threshold_large(self, cube):
        mesh = cube
        id = lagrange.compute_facet_normal(mesh)
        id = lagrange.map_attribute_in_place(mesh, id, lagrange.AttributeElement.Indexed)

        lagrange.weld_indexed_attribute(mesh, id, epsilon_abs=2, angle_abs=np.pi)
        attr = mesh.indexed_attribute(id)
        assert len(attr.values.data) == mesh.num_vertices
