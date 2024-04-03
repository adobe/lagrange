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

import pytest
import numpy as np


class TestScene:
    def test_empty_scene(self):
        scene = lagrange.scene.Scene()
        assert len(scene.nodes) == 0
        assert len(scene.root_nodes) == 0
        assert len(scene.meshes) == 0
        assert len(scene.images) == 0
        assert len(scene.textures) == 0
        assert len(scene.materials) == 0
        assert len(scene.lights) == 0
        assert len(scene.cameras) == 0
        assert len(scene.skeletons) == 0
        assert len(scene.animations) == 0

    def test_node(self):
        node = lagrange.scene.Node()
        assert len(node.children) == 0
        assert len(node.meshes) == 0
        assert len(node.cameras) == 0
        assert len(node.lights) == 0
        assert np.all(node.transform == np.eye(4))
        assert node.transform.flags["F_CONTIGUOUS"]
        assert not node.transform.flags["OWNDATA"]

        node.transform = np.arange(16).reshape((4, 4))
        assert np.all(node.transform.ravel() == np.arange(16))

    def test_scene_construction(self):
        mesh = lagrange.SurfaceMesh()
        scene = lagrange.scene.Scene()
        assert len(scene.meshes) == 0

        # Add mesh to scene
        mesh_id = scene.add(mesh)
        assert mesh_id == 0
        assert len(scene.meshes) == 1

        # Create mesh instance
        instance = lagrange.scene.SceneMeshInstance()
        instance.mesh = mesh_id

        # Create node
        node = lagrange.scene.Node()
        node.name = "test node"
        node.meshes.append(instance)
        assert len(node.meshes) == 1

        # Add node to the scene
        node_id = scene.add(node)
        assert len(scene.nodes) == 1
