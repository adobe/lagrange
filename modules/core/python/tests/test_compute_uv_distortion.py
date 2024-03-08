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
import pytest


class TestComputeUVDistortion:
    def test_triangle(self):
        mesh = lagrange.SurfaceMesh()
        mesh.add_vertex([0, 0, 0])
        mesh.add_vertex([1, 0, 0])
        mesh.add_vertex([0, 1, 0])
        mesh.add_triangle(0, 1, 2)

        uv_name = "uv"
        mesh.wrap_as_indexed_attribute(
            uv_name,
            lagrange.AttributeUsage.UV,
            np.array([[0, 0], [1, 0], [0, 1]], dtype=float),
            np.array([[0, 1, 2]], dtype=np.intc),
        )

        distortion_id = lagrange.compute_uv_distortion(mesh, uv_name)
        distortion = mesh.attribute(distortion_id).data
        assert distortion[0] == 2

        uvs = mesh.indexed_attribute(uv_name).values.data
        uvs[1, 0] = 2
        distortion_id = lagrange.compute_uv_distortion(mesh, uv_name)
        distortion = mesh.attribute(distortion_id).data
        assert distortion[0] == pytest.approx(2.5)
