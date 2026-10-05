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


@pytest.fixture
def shared_mesh_occlusion_scene():
    """One small-cube mesh shared by two instances: one enclosed by an outer cube (occluded) and one
    far away in the open (visible). Exercises the facet instancing policies."""
    scene = lagrange.scene.SimpleScene3D()
    outer_id = scene.add_mesh(_centered_cube(2.0))
    small_id = scene.add_mesh(_centered_cube(0.5))

    outer = lagrange.scene.MeshInstance3D()
    outer.mesh_index = outer_id
    scene.add_instance(outer)

    enclosed = lagrange.scene.MeshInstance3D()
    enclosed.mesh_index = small_id  # at origin, inside the outer cube -> fully occluded
    scene.add_instance(enclosed)

    exposed = lagrange.scene.MeshInstance3D()
    exposed.mesh_index = small_id
    exposed.transform = np.array(
        [[1, 0, 0, 10], [0, 1, 0, 0], [0, 0, 1, 0], [0, 0, 0, 1]], dtype=float
    )
    scene.add_instance(exposed)
    return scene


@pytest.fixture
def far_apart_triangles_scene():
    """Two open triangles whose area is tiny relative to the combined scene AABB."""
    mesh = lagrange.SurfaceMesh()
    mesh.vertices = np.array([[0, 0, 0], [1, 0, 0], [0, 1, 0]], dtype=float)
    mesh.facets = np.array([[0, 1, 2]], dtype=np.uint32)

    scene = lagrange.scene.SimpleScene3D()
    mesh_id = scene.add_mesh(mesh)
    for x in (0.0, 1_000_000.0):
        instance = lagrange.scene.MeshInstance3D()
        instance.mesh_index = mesh_id
        instance.transform = np.array(
            [[1, 0, 0, x], [0, 1, 0, 0], [0, 0, 1, 0], [0, 0, 0, 1]], dtype=float
        )
        scene.add_instance(instance)
    return scene


@pytest.fixture
def mixed_scale_triangles_scene():
    """One open mesh with a unit triangle and a much smaller, disjoint triangle."""
    eps = 1e-4
    mesh = lagrange.SurfaceMesh()
    mesh.vertices = np.array(
        [[0, 0, 0], [1, 0, 0], [0, 1, 0], [2, 0, 0], [2 + eps, 0, 0], [2, eps, 0]],
        dtype=float,
    )
    mesh.facets = np.array([[0, 1, 2], [3, 4, 5]], dtype=np.uint32)
    return lagrange.scene.mesh_to_simple_scene(mesh)


@pytest.fixture
def degenerate_triangle_scene():
    """A scene with one zero-area triangle, for no-progress termination tests."""
    mesh = lagrange.SurfaceMesh()
    mesh.vertices = np.array([[0, 0, 0], [1, 0, 0], [2, 0, 0]], dtype=float)
    mesh.facets = np.array([[0, 1, 2]], dtype=np.uint32)
    return lagrange.scene.mesh_to_simple_scene(mesh)


def _cube_with_corner_normals(size):
    """Centered cube carrying flat per-facet normals as an indexed "normal" attribute."""
    mesh = _centered_cube(size)
    v = mesh.vertices
    f = mesh.facets
    n = np.cross(v[f[:, 1]] - v[f[:, 0]], v[f[:, 2]] - v[f[:, 0]])
    n /= np.linalg.norm(n, axis=1, keepdims=True)
    mesh.create_attribute(
        "normal",
        element=lagrange.AttributeElement.Indexed,
        usage=lagrange.AttributeUsage.Normal,
        initial_values=n.astype(np.float64),
        initial_indices=np.repeat(np.arange(f.shape[0], dtype=np.uint32), 3).reshape(-1, 3),
    )
    return mesh


@pytest.fixture
def instanced_normals_scene():
    """A few well-separated instances of one normal-bearing cube (none occlude each other)."""
    scene = lagrange.scene.SimpleScene3D()
    mesh_id = scene.add_mesh(_cube_with_corner_normals(1.0))
    rot_z = np.array([[0, -1, 0, 0], [1, 0, 0, 0], [0, 0, 1, 0], [0, 0, 0, 1]], dtype=float)
    transforms = [np.identity(4)]
    for translation in ((10.0, 0.0, 0.0), (0.0, 10.0, 0.0)):
        m = rot_z.copy()
        m[:3, 3] = translation
        transforms.append(m)
    for m in transforms:
        instance = lagrange.scene.MeshInstance3D()
        instance.mesh_index = mesh_id
        instance.transform = m
        scene.add_instance(instance)
    return scene, transforms
