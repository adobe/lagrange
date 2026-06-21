#
# Copyright 2026 Adobe. All rights reserved.
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


@pytest.fixture
def two_triangle_quad():
    mesh = lagrange.SurfaceMesh()
    mesh.add_vertex([0, 0, 0])
    mesh.add_vertex([1, 0, 0])
    mesh.add_vertex([0, 1, 0])
    mesh.add_vertex([1, 1, 0])
    mesh.add_triangle(0, 1, 2)
    mesh.add_triangle(1, 3, 2)
    return mesh


@pytest.fixture
def unwrapped_two_triangle_quad(two_triangle_quad):
    uv_values = np.array(
        [[0, 0], [0.4, 0], [0, 0.4], [0.6, 0], [1, 0], [0.6, 0.4]],
        dtype=np.float64,
    )
    uv_indices = np.array(
        [0, 1, 2, 3, 4, 5],
        dtype=np.uint32,
    )
    two_triangle_quad.create_attribute(
        name="uv",
        element=lagrange.AttributeElement.Indexed,
        usage=lagrange.AttributeUsage.UV,
        initial_values=uv_values,
        initial_indices=uv_indices,
    )
    return two_triangle_quad
