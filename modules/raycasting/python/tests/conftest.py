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


def _centered_cube(size):
    """Cube of given side length centered at origin, outward-winding triangles."""
    s = size / 2
    vertices = np.array(
        [
            [-s, -s, -s],
            [+s, -s, -s],
            [+s, +s, -s],
            [-s, +s, -s],
            [-s, -s, +s],
            [+s, -s, +s],
            [+s, +s, +s],
            [-s, +s, +s],
        ],
        dtype=float,
    )
    facets = np.array(
        [
            [0, 3, 2],
            [2, 1, 0],  # z = -s
            [4, 5, 6],
            [6, 7, 4],  # z = +s
            [1, 2, 6],
            [6, 5, 1],  # x = +s
            [4, 7, 3],
            [3, 0, 4],  # x = -s
            [2, 3, 7],
            [7, 6, 2],  # y = +s
            [0, 1, 5],
            [5, 4, 0],  # y = -s
        ],
        dtype=np.uint32,
    )
    mesh = lagrange.SurfaceMesh()
    mesh.vertices = vertices
    mesh.facets = facets
    return mesh


@pytest.fixture(params=[(2.0, 0.5)])
def nested_cubes_scene(request):
    """Outer cube fully enclosing an inner cube, both centered at origin."""
    outer_size, inner_size = request.param
    scene = lagrange.scene.SimpleScene3D()
    outer_id = scene.add_mesh(_centered_cube(outer_size))
    inner_id = scene.add_mesh(_centered_cube(inner_size))
    for mi in (outer_id, inner_id):
        instance = lagrange.scene.MeshInstance3D()
        instance.mesh_index = mi
        scene.add_instance(instance)
    return scene
