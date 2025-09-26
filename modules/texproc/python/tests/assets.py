#
# Copyright 2025 Adobe. All rights reserved.
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

from pathlib import Path
import numpy as np
import pytest
import math


def make_translation(aa):
    return np.array(
        [
            [1.0, 0.0, 0.0, aa[0]],
            [0.0, 1.0, 0.0, aa[1]],
            [0.0, 0.0, 1.0, aa[2]],
            [0.0, 0.0, 0.0, 1.0],
        ],
        dtype=np.float32,
    )


@pytest.fixture
def quad_tex():
    return np.zeros(shape=(128, 128, 4), dtype=np.float32)


@pytest.fixture
def quad_mesh():
    mesh = lagrange.SurfaceMesh()
    mesh.add_vertex([0, 0, 0])
    mesh.add_vertex([1, 0, 0])
    mesh.add_vertex([1, 1, 0])
    mesh.add_vertex([0, 1, 0])
    mesh.add_triangle(0, 1, 2)
    mesh.add_triangle(0, 2, 3)
    uvs = mesh.vertices[:, :2].copy()
    uvs = 0.1 + 0.8 * uvs
    mesh.create_attribute(
        "uv",
        element=lagrange.AttributeElement.Indexed,
        usage=lagrange.AttributeUsage.UV,
        initial_values=uvs,
        initial_indices=mesh.facets,
    )
    return mesh


@pytest.fixture
def cube_with_uv():
    mesh = lagrange.SurfaceMesh()
    mesh.vertices = np.array(
        [
            [0, 0, 0],
            [1, 0, 0],
            [1, 1, 0],
            [0, 1, 0],
            [0, 0, 1],
            [1, 0, 1],
            [1, 1, 1],
            [0, 1, 1],
        ],
        dtype=float,
    )
    mesh.facets = np.array(
        [
            [0, 3, 2, 1],
            [4, 5, 6, 7],
            [1, 2, 6, 5],
            [4, 7, 3, 0],
            [2, 3, 7, 6],
            [0, 1, 5, 4],
        ],
        dtype=np.uint32,
    )
    mesh.create_attribute(
        "uv",
        lagrange.AttributeElement.Indexed,
        lagrange.AttributeUsage.UV,
        np.array(
            [
                [0.25, 0],
                [0.5, 0],
                [0.25, 0.25],
                [0.5, 0.25],
                [0.25, 0.5],
                [0.5, 0.5],
                [0.25, 0.75],
                [0.5, 0.75],
                [0.25, 1],
                [0.5, 1],
                [0, 0.75],
                [0, 0.5],
                [0.75, 0.75],
                [0.75, 0.5],
            ]
        ),
        np.array(
            [
                [8, 6, 7, 9],
                [2, 3, 5, 4],
                [12, 7, 5, 13],
                [11, 4, 6, 10],
                [7, 6, 4, 5],
                [0, 1, 3, 2],
            ]
        ),
    )
    lagrange.triangulate_polygonal_facets(mesh)
    return mesh


@pytest.fixture
def quad_scene(quad_mesh):
    mesh = quad_mesh

    scene = lagrange.scene.Scene()

    # Add mesh
    assert len(scene.meshes) == 0
    mesh_id = scene.add(mesh)
    assert mesh_id == 0
    assert len(scene.meshes) == 1

    # Add image
    assert len(scene.images) == 0
    image = lagrange.scene.Image()
    image.image.data = np.zeros(shape=(128, 128, 4), dtype=np.uint8)
    image_id = scene.add(image)
    assert len(scene.images) == 1

    # Add texture
    assert len(scene.textures) == 0
    texture = lagrange.scene.Texture()
    texture.image = image_id
    texture_id = scene.add(texture)
    assert texture_id == 0
    assert len(scene.textures) == 1

    # Add material
    assert len(scene.materials) == 0
    material = lagrange.scene.Material()
    material.base_color_texture.index = texture_id
    material_id = scene.add(material)
    assert material_id == 0
    assert len(scene.materials) == 1

    # Create instance
    instance = lagrange.scene.SceneMeshInstance()
    instance.mesh = mesh_id
    instance.materials.append(material_id)

    # Create instance node
    node = lagrange.scene.Node()
    node.meshes.append(instance)
    assert len(node.meshes) == 1

    # Add instance node
    assert len(scene.nodes) == 0
    node_id = scene.add(node)
    assert node_id == 0
    assert len(scene.nodes) == 1

    for kk in range(8):
        # Add camera
        camera = lagrange.scene.Camera()
        camera.horizontal_fov = math.radians(50)
        camera.aspect_ratio = 4.0 / 3.0
        camera.position = np.array([0.5, 4.0, 0.5])
        camera.up = np.array([0.0, 0.0, 1.0])
        camera.look_at = np.zeros(3)
        camera_id = scene.add(camera)
        assert len(scene.cameras) == kk + 1

        # Create camera node
        node = lagrange.scene.Node()
        node.cameras.append(camera_id)
        node.transform = make_translation(np.array([kk - 3.5, 0.0, 10.0]) + 0.5)
        assert len(node.cameras) == 1

        # Add camera node
        assert len(scene.nodes) == kk + 1
        node_id = scene.add(node)
        assert len(scene.nodes) == kk + 2

    return scene
