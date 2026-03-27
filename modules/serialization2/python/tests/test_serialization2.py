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
import pathlib
import tempfile

import lagrange
import numpy as np
import pytest


def assert_same_mesh(mesh, mesh2):
    assert mesh.num_vertices == mesh2.num_vertices
    assert mesh.num_facets == mesh2.num_facets
    assert np.all(mesh.vertices.ravel() == mesh2.vertices.ravel())
    assert np.all(mesh.facets.ravel() == mesh2.facets.ravel())

    # Check attributes
    ids1 = mesh.get_matching_attribute_ids()
    ids2 = mesh2.get_matching_attribute_ids()
    names1 = sorted(mesh.get_attribute_name(i) for i in ids1)
    names2 = sorted(mesh2.get_attribute_name(i) for i in ids2)
    assert names1 == names2, f"Attribute mismatch: {names1} != {names2}"
    for name in names1:
        attr1 = mesh.attribute(name)
        attr2 = mesh2.attribute(name)
        assert attr1.element_type == attr2.element_type
        assert attr1.usage == attr2.usage
        assert attr1.num_channels == attr2.num_channels
        np.testing.assert_array_equal(attr1.data, attr2.data)


class TestImportName:
    def test_available_as_serialization(self):
        assert hasattr(lagrange, "serialization")

    def test_hidden_as_serialization2(self):
        assert not hasattr(lagrange, "serialization2")


class TestFormatVersion:
    def test_mesh_format_version(self):
        v = lagrange.serialization.mesh_format_version()
        assert isinstance(v, int)
        assert v >= 1

    def test_simple_scene_format_version(self):
        v = lagrange.serialization.simple_scene_format_version()
        assert isinstance(v, int)
        assert v >= 1

    def test_scene_format_version(self):
        v = lagrange.serialization.scene_format_version()
        assert isinstance(v, int)
        assert v >= 1


class TestMeshSerialization:
    def test_serialize_deserialize(self, triangle):
        data = lagrange.serialization.serialize_mesh(triangle)
        assert isinstance(data, bytes)
        assert len(data) > 0

        mesh2 = lagrange.serialization.deserialize_mesh(data)
        assert_same_mesh(triangle, mesh2)

    def test_serialize_uncompressed(self, triangle):
        data_compressed = lagrange.serialization.serialize_mesh(triangle, compress=True)
        data_uncompressed = lagrange.serialization.serialize_mesh(triangle, compress=False)
        assert isinstance(data_uncompressed, bytes)
        assert len(data_uncompressed) > 0

        # Uncompressed should generally be larger (or at least different)
        assert data_compressed != data_uncompressed

        mesh2 = lagrange.serialization.deserialize_mesh(data_uncompressed)
        assert_same_mesh(triangle, mesh2)

    def test_serialize_compression_level(self, triangle):
        data_low = lagrange.serialization.serialize_mesh(triangle, compression_level=1)
        data_high = lagrange.serialization.serialize_mesh(triangle, compression_level=19)
        assert isinstance(data_low, bytes)
        assert isinstance(data_high, bytes)

        mesh2 = lagrange.serialization.deserialize_mesh(data_low)
        assert_same_mesh(triangle, mesh2)

        mesh3 = lagrange.serialization.deserialize_mesh(data_high)
        assert_same_mesh(triangle, mesh3)

    def test_serialize_with_attribute(self, triangle_with_attribute):
        data = lagrange.serialization.serialize_mesh(triangle_with_attribute)
        mesh2 = lagrange.serialization.deserialize_mesh(data)
        assert_same_mesh(triangle_with_attribute, mesh2)
        assert mesh2.has_attribute("temperature")

    def test_save_load_file(self, triangle):
        with tempfile.TemporaryDirectory() as tmp_dir:
            filepath = pathlib.Path(tmp_dir) / "test.lgm"
            lagrange.serialization.save_mesh(filepath, triangle)
            assert filepath.exists()
            assert filepath.stat().st_size > 0

            mesh2 = lagrange.serialization.load_mesh(filepath)
            assert_same_mesh(triangle, mesh2)

    def test_save_load_uncompressed(self, triangle):
        with tempfile.TemporaryDirectory() as tmp_dir:
            filepath = pathlib.Path(tmp_dir) / "test.lgm"
            lagrange.serialization.save_mesh(filepath, triangle, compress=False)
            mesh2 = lagrange.serialization.load_mesh(filepath)
            assert_same_mesh(triangle, mesh2)

    def test_quad_mesh(self, quad):
        data = lagrange.serialization.serialize_mesh(quad)
        mesh2 = lagrange.serialization.deserialize_mesh(data)
        assert_same_mesh(quad, mesh2)

    def test_empty_mesh(self):
        mesh = lagrange.SurfaceMesh()
        data = lagrange.serialization.serialize_mesh(mesh)
        mesh2 = lagrange.serialization.deserialize_mesh(data)
        assert mesh2.num_vertices == 0
        assert mesh2.num_facets == 0

    def test_point_cloud(self):
        mesh = lagrange.SurfaceMesh()
        mesh.add_vertices(np.eye(3))
        data = lagrange.serialization.serialize_mesh(mesh)
        mesh2 = lagrange.serialization.deserialize_mesh(data)
        assert mesh2.num_vertices == 3
        assert mesh2.num_facets == 0
        assert mesh2.vertices == pytest.approx(mesh.vertices)


class TestSimpleSceneSerialization:
    @pytest.fixture
    def simple_scene(self, triangle):
        return lagrange.scene.mesh_to_simple_scene(triangle)

    def test_serialize_deserialize(self, simple_scene):
        data = lagrange.serialization.serialize_simple_scene(simple_scene)
        assert isinstance(data, bytes)
        assert len(data) > 0

        scene2 = lagrange.serialization.deserialize_simple_scene(data)
        assert scene2.num_meshes == simple_scene.num_meshes
        for i in range(simple_scene.num_meshes):
            assert scene2.num_instances(i) == simple_scene.num_instances(i)

    def test_save_load_file(self, simple_scene):
        with tempfile.TemporaryDirectory() as tmp_dir:
            filepath = pathlib.Path(tmp_dir) / "test.lgs"
            lagrange.serialization.save_simple_scene(filepath, simple_scene)
            assert filepath.exists()

            scene2 = lagrange.serialization.load_simple_scene(filepath)
            assert scene2.num_meshes == simple_scene.num_meshes
            for i in range(simple_scene.num_meshes):
                assert scene2.num_instances(i) == simple_scene.num_instances(i)

    def test_roundtrip_mesh_data(self, simple_scene, triangle):
        data = lagrange.serialization.serialize_simple_scene(simple_scene)
        scene2 = lagrange.serialization.deserialize_simple_scene(data)

        mesh2 = lagrange.scene.simple_scene_to_mesh(scene2)
        assert_same_mesh(triangle, mesh2)


class TestSceneSerialization:
    @pytest.fixture
    def scene(self, triangle):
        s = lagrange.scene.Scene()
        s.add(triangle)
        node = lagrange.scene.Node()
        instance = lagrange.scene.SceneMeshInstance()
        instance.mesh = 0
        node.meshes.append(instance)
        s.add(node)
        return s

    def test_serialize_deserialize(self, scene):
        data = lagrange.serialization.serialize_scene(scene)
        assert isinstance(data, bytes)
        assert len(data) > 0

        scene2 = lagrange.serialization.deserialize_scene(data)
        assert len(scene2.meshes) == len(scene.meshes)
        assert len(scene2.nodes) == len(scene.nodes)

    def test_save_load_file(self, scene):
        with tempfile.TemporaryDirectory() as tmp_dir:
            filepath = pathlib.Path(tmp_dir) / "test.lgs"
            lagrange.serialization.save_scene(filepath, scene)
            assert filepath.exists()

            scene2 = lagrange.serialization.load_scene(filepath)
            assert len(scene2.meshes) == len(scene.meshes)
            assert len(scene2.nodes) == len(scene.nodes)


class TestSceneConversion:
    def test_mesh_from_simple_scene_buffer(self, triangle):
        simple_scene = lagrange.scene.mesh_to_simple_scene(triangle)
        data = lagrange.serialization.serialize_simple_scene(simple_scene)

        # Without allow_scene_conversion, should fail
        with pytest.raises(RuntimeError):
            lagrange.serialization.deserialize_mesh(data)

        # With allow_scene_conversion, should succeed
        mesh2 = lagrange.serialization.deserialize_mesh(data, allow_scene_conversion=True)
        assert_same_mesh(triangle, mesh2)

    def test_simple_scene_from_mesh_buffer(self, triangle):
        data = lagrange.serialization.serialize_mesh(triangle)

        with pytest.raises(RuntimeError):
            lagrange.serialization.deserialize_simple_scene(data)

        scene2 = lagrange.serialization.deserialize_simple_scene(data, allow_scene_conversion=True)
        assert scene2.num_meshes == 1

    def test_quiet_suppresses_warnings(self, triangle):
        simple_scene = lagrange.scene.mesh_to_simple_scene(triangle)
        data = lagrange.serialization.serialize_simple_scene(simple_scene)

        # Should not raise, quiet just suppresses log warnings
        mesh2 = lagrange.serialization.deserialize_mesh(
            data, allow_scene_conversion=True, quiet=True
        )
        assert_same_mesh(triangle, mesh2)
