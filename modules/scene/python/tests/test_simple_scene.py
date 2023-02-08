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

import pytest
import numpy as np


class TestSimpleScene:
    def test_empty_scene(self):
        scene = lagrange.scene.SimpleScene3D()
        assert scene.num_meshes == 0
        assert scene.total_num_instances == 0

    def create_scene(self):
        mesh = lagrange.SurfaceMesh()
        mesh.vertices = np.identity(3)
        mesh.add_triangle(0, 1, 2)

        scene = lagrange.scene.SimpleScene3D()
        assert scene.num_meshes == 0
        mesh_id = scene.add_mesh(mesh)
        assert scene.num_meshes == 1
        assert scene.num_instances(mesh_id) == 0

        instance = lagrange.scene.MeshInstance3D()
        instance.mesh_index = mesh_id
        instance_id = scene.add_instance(instance)

        return scene, mesh_id, instance_id

    def test_create_scene(self):
        scene, mesh_id, instance_id = self.create_scene()
        assert scene.num_meshes == 1
        assert scene.num_instances(mesh_id) == 1
        assert scene.total_num_instances == 1

        assert scene.get_instance(mesh_id, instance_id).mesh_index == mesh_id

    def test_multiple_instances(self):
        scene, mesh_id, instance_id = self.create_scene()

        instance2 = lagrange.scene.MeshInstance3D()
        instance2.mesh_index = mesh_id
        instance2.transform = np.array(
            [
                [1, 2, 3, 1],
                [0, 1, 0, 1],
                [0, 0, 1, 1],
                [0, 0, 0, 1],
            ]
        )
        instance_id2 = scene.add_instance(instance2)

        assert scene.num_instances(mesh_id) == 2
        assert scene.total_num_instances == 2

        instance = scene.get_instance(mesh_id, instance_id2)
        assert np.all(instance2.transform == instance.transform)

