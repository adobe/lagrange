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

import datetime
import numpy as np
import tempfile
import pathlib
import pytest


@pytest.fixture
def triangle():
    mesh = lagrange.SurfaceMesh()
    mesh.add_vertices(np.identity(3))
    mesh.add_triangle(0, 1, 2)
    return mesh


@pytest.fixture
def triangle_with_uv_normal_color(triangle):
    mesh = triangle
    uv_values = np.array([[0, 0], [1, 0], [0, 1]])
    uv_indices = np.array([[0, 1, 2]])
    mesh.create_attribute(
        "uv",
        lagrange.AttributeElement.Indexed,
        lagrange.AttributeUsage.UV,
        uv_values,
        uv_indices,
    )
    lagrange.compute_normal(mesh)

    color_values = np.array([[255, 255, 255]])
    color_indices = np.array([[0, 0, 0]])
    mesh.create_attribute(
        "color",
        lagrange.AttributeElement.Indexed,
        lagrange.AttributeUsage.Color,
        color_values,
        color_indices,
    )
    return mesh


def assert_same_vertices_and_facets(mesh, mesh2):
    assert mesh.num_vertices == mesh2.num_vertices
    assert mesh.num_facets == mesh2.num_facets
    assert mesh.vertices == pytest.approx(mesh2.vertices)
    assert np.all(mesh.facets == mesh2.facets)


def match_attribute(mesh, mesh2, id1, id2):
    attr_name = "__unit_test__"
    # Convert both attributes to corner attribute and compare the per-corner value.
    id1 = lagrange.map_attribute(
        mesh, id1, attr_name, lagrange.AttributeElement.Corner
    )
    id2 = lagrange.map_attribute(
        mesh2, id2, attr_name, lagrange.AttributeElement.Corner
    )

    attr1 = mesh.attribute(id1)
    attr2 = mesh2.attribute(id2)

    assert attr1.data == pytest.approx(attr2.data)
    mesh.delete_attribute(attr_name)
    mesh2.delete_attribute(attr_name)


def assert_same_attribute(mesh, mesh2, usage, required):
    elements = [
        lagrange.AttributeElement.Indexed,
        lagrange.AttributeElement.Vertex,
        lagrange.AttributeElement.Facet,
        lagrange.AttributeElement.Corner,
        lagrange.AttributeElement.Edge,
    ]

    ids_1 = []
    for elem in elements:
        ids_1 += mesh.get_matching_attribute_ids(element=elem, usage=usage)
    if len(ids_1) == 0:
        return

    ids_2 = []
    for elem in elements:
        ids_2 += mesh2.get_matching_attribute_ids(element=elem, usage=usage)

    assert not required or len(ids_2) > 0
    for id1 in ids_1:
        for id2 in ids_2:
            match_attribute(mesh, mesh2, id1, id2)


class TestIO:
    def __save_and_load__(
        self, suffix, mesh, binary, exact_match, selected_attributes
    ):
        with tempfile.TemporaryDirectory() as tmp_dir:
            tmp_dir_path = pathlib.Path(tmp_dir)
            stamp = datetime.datetime.now().isoformat().replace(":", ".")
            filename = tmp_dir_path / f"{stamp}{suffix}"
            lagrange.io.save_mesh(
                filename,
                mesh,
                binary=binary,
                exact_match=exact_match,
                selected_attributes=selected_attributes,
            )
            mesh2 = lagrange.io.load_mesh(filename)

            assert_same_vertices_and_facets(mesh, mesh2)

            required = not exact_match
            # Check special attributes.
            assert_same_attribute(
                mesh, mesh2, lagrange.AttributeUsage.UV, required
            )
            assert_same_attribute(
                mesh, mesh2, lagrange.AttributeUsage.Normal, required
            )
            if filename.suffix != ".obj":
                assert_same_attribute(
                    mesh, mesh2, lagrange.AttributeUsage.Color, required
                )

            if selected_attributes is not None:
                for attr_id in selected_attributes:
                    if mesh.is_attribute_indexed(attr_id):
                        pass
                    else:
                        attr_name = mesh.get_attribute_name(attr_id)
                        attr = mesh.attribute(attr_id)
                        assert mesh2.has_attribute(attr_name)

    def save_and_load(self, mesh, attribute_ids=None):
        for exact_match in [True, False]:
            # For ascii formats.
            for ext in [".obj", ".ply", ".msh"]:
                self.__save_and_load__(
                    ext,
                    mesh,
                    binary=False,
                    exact_match=exact_match,
                    selected_attributes=attribute_ids,
                )

            # For binary formats
            for ext in [".ply", ".msh"]:
                self.__save_and_load__(
                    ext,
                    mesh,
                    binary=True,
                    exact_match=exact_match,
                    selected_attributes=attribute_ids,
                )

    def test_single_triangle(self, triangle):
        mesh = triangle
        self.save_and_load(mesh)

    def test_single_triangle_with_attributes(
        self, triangle_with_uv_normal_color
    ):
        mesh = triangle_with_uv_normal_color
        self.save_and_load(mesh)

    def test_single_triangle_with_attributes_explicit(
        self, triangle_with_uv_normal_color
    ):
        mesh = triangle_with_uv_normal_color
        attr_ids = mesh.get_matching_attribute_ids()
        assert len(attr_ids) != 0
        self.save_and_load(mesh, attr_ids)

    def test_mesh_string_conversion(self, triangle_with_uv_normal_color):
        mesh = triangle_with_uv_normal_color

        ply_data = lagrange.io.mesh_to_string(mesh, "ply")
        mesh2 = lagrange.io.string_to_mesh(ply_data)
        assert_same_vertices_and_facets(mesh, mesh2)

        obj_data = lagrange.io.mesh_to_string(mesh, "obj")
        mesh2 = lagrange.io.string_to_mesh(obj_data)
        assert_same_vertices_and_facets(mesh, mesh2)

        msh_data = lagrange.io.mesh_to_string(mesh, "msh")
        mesh2 = lagrange.io.string_to_mesh(msh_data)
        assert_same_vertices_and_facets(mesh, mesh2)

        gltf_data = lagrange.io.mesh_to_string(mesh, "gltf")
        mesh2 = lagrange.io.string_to_mesh(gltf_data)
        assert_same_vertices_and_facets(mesh, mesh2)

        gltf_data = lagrange.io.mesh_to_string(mesh, "glb")
        mesh2 = lagrange.io.string_to_mesh(gltf_data)
        assert_same_vertices_and_facets(mesh, mesh2)

    def test_io_with_selected_attributed(self, triangle):
        mesh = triangle
        selected_attributes = [
            mesh.create_attribute(
                "_vertex_id",
                element=lagrange.AttributeElement.Vertex,
                usage=lagrange.AttributeUsage.Scalar,
                initial_values=np.array([0, 1, 2], dtype=np.float32),
            ),
            mesh.create_attribute(
                "_facet_id",
                element=lagrange.AttributeElement.Facet,
                usage=lagrange.AttributeUsage.Scalar,
                initial_values=np.array([0], dtype=np.float32),
            ),
        ]

        for mesh_format in ["ply", "msh", "gltf", "glb"]:
            data = lagrange.io.mesh_to_string(
                mesh, mesh_format, selected_attributes=selected_attributes
            )
            mesh2 = lagrange.io.string_to_mesh(data)
            assert_same_vertices_and_facets(mesh, mesh2)
            assert mesh2.has_attribute("_vertex_id")
            assert mesh2.has_attribute("_facet_id")
