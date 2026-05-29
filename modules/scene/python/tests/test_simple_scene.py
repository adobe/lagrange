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


class TestSimpleScene:
    def test_empty_scene(self):
        scene = lagrange.scene.SimpleScene3D()
        assert scene.num_meshes == 0
        assert scene.total_num_instances == 0

    def create_simple_scene(self):
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
        scene, mesh_id, instance_id = self.create_simple_scene()
        assert scene.num_meshes == 1
        assert scene.num_instances(mesh_id) == 1
        assert scene.total_num_instances == 1

        assert scene.get_instance(mesh_id, instance_id).mesh_index == mesh_id

    def test_multiple_instances(self):
        scene, mesh_id, instance_id = self.create_simple_scene()

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

    def test_scene_convert(self, single_triangle):
        scene = lagrange.scene.mesh_to_simple_scene(single_triangle)
        scene2 = lagrange.scene.meshes_to_simple_scene([single_triangle, single_triangle])
        mesh = lagrange.scene.simple_scene_to_mesh(scene)
        mesh_alt = lagrange.combine_meshes(lagrange.scene.simple_scene_to_meshes(scene))
        mesh2 = lagrange.scene.simple_scene_to_mesh(scene2)
        mesh2_alt = lagrange.combine_meshes(lagrange.scene.simple_scene_to_meshes(scene2))

        assert mesh.num_vertices == 3
        assert mesh.num_facets == 1
        assert mesh2.num_vertices == 6
        assert mesh2.num_facets == 2
        assert np.all(mesh.vertices == mesh_alt.vertices) and np.all(mesh.facets == mesh_alt.facets)
        assert np.all(mesh2.vertices == mesh2_alt.vertices) and np.all(
            mesh2.facets == mesh2_alt.facets
        )


class TestComputeMeshWeights:
    def make_scene(self):
        """Scene with two meshes: m1 has 1 facet (area 0.5), m2 has 2 facets (area 4.0)."""
        m1 = lagrange.SurfaceMesh()
        m1.vertices = np.array([[0, 0, 0], [1, 0, 0], [0, 1, 0]], dtype=np.float64)
        m1.add_triangle(0, 1, 2)

        m2 = lagrange.SurfaceMesh()
        m2.vertices = np.array([[0, 0, 0], [2, 0, 0], [0, 2, 0], [2, 2, 0]], dtype=np.float64)
        m2.add_triangle(0, 1, 2)
        m2.add_triangle(1, 3, 2)

        scene = lagrange.scene.SimpleScene3D()
        idx0 = scene.add_mesh(m1)
        idx1 = scene.add_mesh(m2)

        inst0 = lagrange.scene.MeshInstance3D()
        inst0.mesh_index = idx0
        scene.add_instance(inst0)

        inst1 = lagrange.scene.MeshInstance3D()
        inst1.mesh_index = idx1
        scene.add_instance(inst1)

        return scene

    def test_even_split(self):
        scene = self.make_scene()
        weights = lagrange.scene.compute_mesh_weights(
            scene, lagrange.scene.FacetAllocationStrategy.EvenSplit
        )
        assert len(weights) == 2
        assert sum(weights) == pytest.approx(1.0, abs=1e-10)
        assert weights[0] == pytest.approx(0.5, abs=1e-10)
        assert weights[1] == pytest.approx(0.5, abs=1e-10)

    def test_default_strategy_is_even_split(self):
        scene = self.make_scene()
        weights = lagrange.scene.compute_mesh_weights(scene)
        assert len(weights) == 2
        assert weights[0] == pytest.approx(0.5, abs=1e-10)
        assert weights[1] == pytest.approx(0.5, abs=1e-10)

    def test_relative_to_num_facets(self):
        scene = self.make_scene()
        weights = lagrange.scene.compute_mesh_weights(
            scene, lagrange.scene.FacetAllocationStrategy.RelativeToNumFacets
        )
        assert len(weights) == 2
        assert sum(weights) == pytest.approx(1.0, abs=1e-10)
        # m1 has 1 facet, m2 has 2 → weights 1/3 and 2/3
        assert weights[0] == pytest.approx(1.0 / 3.0, abs=1e-10)
        assert weights[1] == pytest.approx(2.0 / 3.0, abs=1e-10)

    def test_relative_to_mesh_area(self):
        scene = self.make_scene()
        weights = lagrange.scene.compute_mesh_weights(
            scene, lagrange.scene.FacetAllocationStrategy.RelativeToMeshArea
        )
        assert len(weights) == 2
        assert sum(weights) == pytest.approx(1.0, abs=1e-10)
        # m1: right triangle legs 1,1 → area 0.5
        # m2: two right triangles legs 2,2 each → area 4.0
        total = 4.5
        assert weights[0] == pytest.approx(0.5 / total, abs=1e-10)
        assert weights[1] == pytest.approx(4.0 / total, abs=1e-10)

    def test_synchronized_raises(self):
        scene = self.make_scene()
        with pytest.raises(RuntimeError):
            lagrange.scene.compute_mesh_weights(
                scene, lagrange.scene.FacetAllocationStrategy.Synchronized
            )
