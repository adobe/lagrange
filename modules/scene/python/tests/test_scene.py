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
import math

import lagrange
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

        # Add cameras to the scene
        camera = lagrange.scene.Camera()
        camera.name = "test camera"
        camera.horizontal_fov = math.radians(50)
        camera_id0 = scene.add(camera)
        assert len(scene.cameras) == 1
        camera_id1 = scene.add(camera)
        assert len(scene.cameras) == 2

        # Add lights to the scene
        light = lagrange.scene.Light()
        light.type = lagrange.scene.Light.Type.Point
        assert np.array_equal(light.up, [0, 0, 1])
        light.up = np.array([1, 0, 0], dtype=np.float32)  # TODO: Support assigning lists directly?
        assert np.array_equal(light.up, [1, 0, 0])
        light_id0 = scene.add(light)
        assert len(scene.lights) == 1
        light_id1 = scene.add(light)
        assert len(scene.lights) == 2
        scene.lights[light_id0].up[2] = 4
        assert np.array_equal(scene.lights[light_id0].up, [1, 0, 4])
        assert np.array_equal(scene.lights[light_id1].up, [1, 0, 0])

        # Add images to the scene
        image = lagrange.scene.Image()
        image.image.data = np.zeros((1024, 1024, 4), dtype=np.uint8)
        assert np.all(image.image.data == 0)
        image.image.data = np.ones((1024, 1024, 4), dtype=np.uint8)
        assert np.all(image.image.data == 1)
        image_id = scene.add(image)
        assert len(scene.images) == 1
        assert np.all(scene.images[image_id].image.data == 1)
        scene.images[image_id].image.data = np.ones((1024, 1024, 4), dtype=np.uint8) * 255
        assert np.all(scene.images[image_id].image.data == 255)

        # Add textures to the scene
        texture = lagrange.scene.Texture()
        texture.image = image_id
        assert texture.mag_filter == lagrange.scene.Texture.TextureFilter.Undefined
        texture.mag_filter = lagrange.scene.Texture.TextureFilter.Linear
        assert texture.mag_filter == lagrange.scene.Texture.TextureFilter.Linear
        assert texture.wrap_u == lagrange.scene.Texture.WrapMode.Wrap
        texture.wrap_u = lagrange.scene.Texture.WrapMode.Clamp
        assert texture.wrap_u == lagrange.scene.Texture.WrapMode.Clamp
        texture_id = scene.add(texture)
        assert len(scene.textures) == 1
        assert scene.textures[texture_id].mag_filter == lagrange.scene.Texture.TextureFilter.Linear
        scene.textures[texture_id].mag_filter = lagrange.scene.Texture.TextureFilter.Nearest
        assert scene.textures[texture_id].mag_filter == lagrange.scene.Texture.TextureFilter.Nearest
        assert scene.textures[texture_id].wrap_u == lagrange.scene.Texture.WrapMode.Clamp
        scene.textures[texture_id].wrap_u = lagrange.scene.Texture.WrapMode.Decal
        assert scene.textures[texture_id].wrap_u == lagrange.scene.Texture.WrapMode.Decal

        # Add materials to the scene
        material = lagrange.scene.Material()
        material.name = "test material"
        assert material.base_color_texture.index != texture_id
        material.base_color_texture.index = texture_id
        assert material.base_color_texture.index == texture_id
        material_id0 = scene.add(material)
        assert len(scene.materials) == 1
        material_id1 = scene.add(material)
        assert len(scene.materials) == 2

        # Add meshes to scene
        mesh_id0 = scene.add(mesh)
        assert mesh_id0 == 0
        assert len(scene.meshes) == 1
        mesh_id1 = scene.add(mesh)
        assert mesh_id1 == 1
        assert len(scene.meshes) == 2

        # Create mesh instance
        instance = lagrange.scene.SceneMeshInstance()
        instance.mesh = mesh_id0
        instance.materials.append(material_id0)
        assert len(instance.materials) == 1
        assert instance.materials[0] == material_id0
        instance.materials[0] = material_id1
        assert instance.materials[0] == material_id1

        # Create node
        node = lagrange.scene.Node()
        node.name = "test node"
        node.meshes.append(instance)
        assert len(node.meshes) == 1

        # Modify node mesh instance id
        assert node.meshes[0].mesh == mesh_id0
        node.meshes[0].mesh = mesh_id1
        assert node.meshes[0].mesh == mesh_id1
        assert node.meshes[0].materials[0] == material_id1
        node.meshes[0].materials[0] = material_id0
        assert node.meshes[0].materials[0] == material_id0

        # Modify node camera id
        assert len(node.cameras) == 0
        node.cameras.append(camera_id0)
        assert len(node.cameras) == 1
        assert node.cameras[0] == camera_id0
        node.cameras[0] = camera_id1
        assert node.cameras[0] == camera_id1

        # Modify node light id
        assert len(node.lights) == 0
        node.lights.append(light_id0)
        assert len(node.lights) == 1
        assert node.lights[0] == light_id0
        node.lights[0] = light_id1
        assert node.lights[0] == light_id1

        # Add node to the scene
        node_id = scene.add(node)
        assert len(scene.nodes) == 1

        # Modify node mesh instance id
        assert scene.nodes[node_id].meshes[0].mesh == mesh_id1
        scene.nodes[node_id].meshes[0].mesh = mesh_id0
        assert scene.nodes[node_id].meshes[0].mesh == mesh_id0

        # Modify node camera id
        assert scene.nodes[node_id].cameras[0] == camera_id1
        scene.nodes[node_id].cameras[0] = camera_id0
        assert scene.nodes[node_id].cameras[0] == camera_id0

        # Modify node light id
        assert scene.nodes[node_id].lights[0] == light_id1
        scene.nodes[node_id].lights[0] = light_id0
        assert scene.nodes[node_id].lights[0] == light_id0

        # Add skeletons to the scene
        skeleton = lagrange.scene.Skeleton()
        skeleton_id = scene.add(skeleton)
        assert skeleton_id == 0
        assert len(scene.skeletons) == 1

        # Add animations to the scene
        animation = lagrange.scene.Animation()
        animation.name = "test animation"
        animation_id = scene.add(animation)
        assert animation_id == 0
        assert len(scene.animations) == 1

    def test_scene_extension(self):
        # TODO: Fix our bindings to make this test work
        return

        scene = lagrange.scene.Scene()
        assert scene.extensions.size == 0
        # TODO: Add |= to nb::bind_map objects
        # scene.extensions.data |= {"extension0": {"key": [0, 1, 2]}}
        scene.extensions.data.update({"extension0": {"key": [0, 1, 2]}})
        assert scene.extensions.size == 1
        x = scene.extensions.data["extension0"]
        assert x == {"key": [0, 1, 2]}
        assert scene.extensions.data["extension0"] == {"key": [0, 1, 2]}
        scene.extensions.data.update({"extension1": {"key": "foo"}})
        assert scene.extensions.size == 2
        assert scene.extensions.data["extension1"] == {"key": "foo"}
        scene.extensions.data["extension0"]["key"][0] = 10
        assert scene.extensions.data["extension0"] == {"key": [10, 1, 2]}
        scene.extensions.data["extension0"]["key"] = [3, 4, 5]
        assert scene.extensions.data["extension0"] == {"key": [3, 4, 5]}

    def test_scene_convert(self, single_triangle):
        scene = lagrange.scene.mesh_to_scene(single_triangle)
        scene2 = lagrange.scene.meshes_to_scene([single_triangle, single_triangle])
        mesh = lagrange.scene.scene_to_mesh(scene)
        mesh_alt = lagrange.combine_meshes(lagrange.scene.scene_to_meshes(scene))
        mesh2 = lagrange.scene.scene_to_mesh(scene2)
        mesh2_alt = lagrange.combine_meshes(lagrange.scene.scene_to_meshes(scene2))

        assert mesh.num_vertices == 3
        assert mesh.num_facets == 1
        assert mesh2.num_vertices == 6
        assert mesh2.num_facets == 2
        assert np.all(mesh.vertices == mesh_alt.vertices) and np.all(mesh.facets == mesh_alt.facets)
        assert np.all(mesh2.vertices == mesh2_alt.vertices) and np.all(
            mesh2.facets == mesh2_alt.facets
        )

    def test_scene_to_simple_scene_empty(self):
        """An empty scene converts to an empty SimpleScene."""
        scene = lagrange.scene.Scene()
        root = lagrange.scene.Node()
        root.name = "root"
        root_id = scene.add(root)
        scene.root_nodes.append(root_id)

        simple = lagrange.scene.scene_to_simple_scene(scene)
        assert simple.num_meshes == 0

    def test_scene_to_simple_scene_basic(self, single_triangle):
        """A scene with one mesh instance converts to a SimpleScene with correct transform."""
        scene = lagrange.scene.Scene()
        mesh_id = scene.add(single_triangle)

        root = lagrange.scene.Node()
        root.name = "root"
        root_id = scene.add(root)
        scene.root_nodes.append(root_id)

        child = lagrange.scene.Node()
        child.name = "child"
        xf = np.eye(4, dtype=np.float32)
        xf[0, 3] = 1.0
        xf[1, 3] = 2.0
        xf[2, 3] = 3.0
        child.transform = xf
        child.parent = root_id

        mi = lagrange.scene.SceneMeshInstance()
        mi.mesh = mesh_id
        child.meshes.append(mi)

        child_id = scene.add(child)
        scene.add_child(root_id, child_id)

        simple = lagrange.scene.scene_to_simple_scene(scene)
        assert simple.num_meshes == 1
        assert simple.num_instances(0) == 1

        inst = simple.get_instance(0, 0)
        assert inst.mesh_index == 0
        assert np.allclose(inst.transform, xf, atol=1e-6)

    def test_scene_to_simple_scene_hierarchy(self, single_triangle):
        """World transform is the product of ancestor transforms."""
        scene = lagrange.scene.Scene()
        mesh_id = scene.add(single_triangle)

        root_xf = np.eye(4, dtype=np.float32)
        root_xf[0, 3] = 1.0  # translate x by 1

        root = lagrange.scene.Node()
        root.name = "root"
        root.transform = root_xf
        root_id = scene.add(root)
        scene.root_nodes.append(root_id)

        child_xf = np.eye(4, dtype=np.float32)
        child_xf[1, 3] = 2.0  # translate y by 2

        child = lagrange.scene.Node()
        child.name = "child"
        child.transform = child_xf
        child.parent = root_id
        mi = lagrange.scene.SceneMeshInstance()
        mi.mesh = mesh_id
        child.meshes.append(mi)
        child_id = scene.add(child)
        scene.add_child(root_id, child_id)

        simple = lagrange.scene.scene_to_simple_scene(scene)
        assert simple.num_meshes == 1
        assert simple.num_instances(0) == 1

        expected_xf = root_xf @ child_xf
        inst = simple.get_instance(0, 0)
        assert np.allclose(inst.transform, expected_xf, atol=1e-6)

    def test_simple_scene_to_scene_basic(self, single_triangle):
        """A SimpleScene converts to a Scene with correct structure."""
        simple = lagrange.scene.SimpleScene3D()
        mesh_idx = simple.add_mesh(single_triangle)

        inst = lagrange.scene.MeshInstance3D()
        inst.mesh_index = mesh_idx
        xf = np.eye(4, dtype=np.float64)
        xf[0, 3] = 5.0
        inst.transform = xf
        simple.add_instance(inst)

        scene = lagrange.scene.simple_scene_to_scene(simple)
        assert len(scene.meshes) == 1
        assert len(scene.root_nodes) == 1
        # root + 1 instance node
        assert len(scene.nodes) == 2

        root_id = scene.root_nodes[0]
        assert len(scene.nodes[root_id].children) == 1

        child_id = scene.nodes[root_id].children[0]
        child = scene.nodes[child_id]
        assert len(child.meshes) == 1
        assert child.meshes[0].mesh == 0
        # Compare as float32 since Scene node transforms are float.
        assert np.allclose(child.transform, xf.astype(np.float32), atol=1e-5)

    def test_scene_to_simple_scene_roundtrip(self, single_triangle):
        """Roundtrip Scene -> SimpleScene -> Scene preserves mesh data and world transforms."""
        scene = lagrange.scene.Scene()
        mesh_id = scene.add(single_triangle)

        root_xf = np.eye(4, dtype=np.float32)
        root_xf[0, 3] = 1.0

        root = lagrange.scene.Node()
        root.name = "root"
        root.transform = root_xf
        root_id = scene.add(root)
        scene.root_nodes.append(root_id)

        child_xf = np.eye(4, dtype=np.float32)
        child_xf[1, 3] = 2.0

        child = lagrange.scene.Node()
        child.name = "child"
        child.transform = child_xf
        child.parent = root_id
        mi = lagrange.scene.SceneMeshInstance()
        mi.mesh = mesh_id
        child.meshes.append(mi)
        child_id = scene.add(child)
        scene.add_child(root_id, child_id)

        # Roundtrip
        simple = lagrange.scene.scene_to_simple_scene(scene)
        scene2 = lagrange.scene.simple_scene_to_scene(simple)

        # Mesh data preserved
        assert len(scene2.meshes) == len(scene.meshes)
        assert np.all(scene2.meshes[0].vertices == scene.meshes[0].vertices)
        assert np.all(scene2.meshes[0].facets == scene.meshes[0].facets)

        # World transform preserved: the instance node in scene2 should carry
        # the flattened world transform root_xf @ child_xf.
        expected_world = root_xf @ child_xf
        root2_id = scene2.root_nodes[0]
        child2_id = scene2.nodes[root2_id].children[0]
        assert np.allclose(scene2.nodes[child2_id].transform, expected_world, atol=1e-5)
