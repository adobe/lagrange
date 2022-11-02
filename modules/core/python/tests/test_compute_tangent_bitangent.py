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
import pytest

from .assets import single_triangle, single_triangle_with_uv, cube, cube_with_uv


class TestComputeTangentBitangent:
    def check_orthogonality(self, mesh, normal_id, tangent_id, bitangent_id):
        normal_attr = mesh.indexed_attribute(normal_id)
        tangent_attr = mesh.indexed_attribute(tangent_id)
        bitangent_attr = mesh.indexed_attribute(bitangent_id)

        normal_indices = normal_attr.indices.data
        tangent_indices = tangent_attr.indices.data
        bitangent_indices = bitangent_attr.indices.data

        normal_values = normal_attr.values.data
        tangent_values = tangent_attr.values.data
        bitangent_values = bitangent_attr.values.data

        for i, j, k in zip(normal_indices, tangent_indices, bitangent_indices):
            n = normal_values[i]
            t = tangent_values[j]
            b = bitangent_values[k]
            assert n.dot(t) == pytest.approx(0)
            assert n.dot(b) == pytest.approx(0)
            assert t.dot(b) == pytest.approx(0)

    def test_simple(self, single_triangle_with_uv):
        mesh = single_triangle_with_uv
        normal_id = lagrange.compute_normal(mesh)

        r = lagrange.compute_tangent_bitangent(mesh)
        tangent_id = r.tangent_id
        bitangent_id = r.bitangent_id

        assert mesh.is_attribute_indexed(tangent_id)
        assert mesh.is_attribute_indexed(bitangent_id)

        self.check_orthogonality(mesh, normal_id, tangent_id, bitangent_id)

    def test_cube(self, cube_with_uv):
        mesh = cube_with_uv

        normal_id = lagrange.compute_normal(mesh)
        opt = lagrange.TangentBitangentOptions()
        opt.output_element_type = lagrange.AttributeElement.Corner
        r = lagrange.compute_tangent_bitangent(mesh, opt)
        tangent_id = r.tangent_id
        bitangent_id = r.bitangent_id

        assert not mesh.is_attribute_indexed(tangent_id)
        assert not mesh.is_attribute_indexed(bitangent_id)

        tangent_id = lagrange.map_attribute_in_place(
            mesh, tangent_id, lagrange.AttributeElement.Indexed
        )
        bitangent_id = lagrange.map_attribute_in_place(
            mesh, bitangent_id, lagrange.AttributeElement.Indexed
        )

        self.check_orthogonality(mesh, normal_id, tangent_id, bitangent_id)
