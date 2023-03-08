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
    @property
    def tmp_dir(self):
        return pathlib.Path(tempfile.gettempdir())

    def __save_and_load__(self, filename, mesh, save_options):
        print(filename)
        lagrange.io.save_mesh(filename, mesh, save_options)
        load_options = lagrange.io.LoadOptions()
        load_options.load_vertex_colors = True
        mesh2 = lagrange.io.load_mesh(filename, load_options)

        assert_same_vertices_and_facets(mesh, mesh2)

        required = (
            save_options.attribute_conversion_policy
            == lagrange.io.AttributeConversionPolicy.ConvertAsNeeded
        )
        # Check special attributes.
        assert_same_attribute(mesh, mesh2, lagrange.AttributeUsage.UV, required)
        assert_same_attribute(
            mesh, mesh2, lagrange.AttributeUsage.Normal, required
        )
        if filename.suffix != ".obj":
            assert_same_attribute(
                mesh, mesh2, lagrange.AttributeUsage.Color, required
            )

        if (
            save_options.output_attributes
            == lagrange.io.OutputAttributes.SelectedOnly
        ):
            for attr_id in save_options.selected_attributes:
                if mesh.is_attribute_indexed(attr_id):
                    pass
                else:
                    attr_name = mesh.get_attribute_name(attr_id)
                    attr = mesh.attribute(attr_id)
                    assert mesh2.has_attribute(attr_name)

    def save_and_load(self, mesh, attribute_ids=[]):
        save_options = lagrange.io.SaveOptions()
        if len(attribute_ids) > 0:
            save_options.output_attributes = (
                lagrange.io.OutputAttributes.SelectedOnly
            )
            save_options.selected_attributes = attribute_ids
        else:
            save_options.output_attributes = lagrange.io.OutputAttributes.All

        for policy in [
            lagrange.io.AttributeConversionPolicy.ExactMatchOnly,
            lagrange.io.AttributeConversionPolicy.ConvertAsNeeded,
        ]:
            save_options.attribute_conversion_policy = policy
            # For ascii formats.
            for ext in [".obj", ".ply", ".msh"]:
                save_options.encoding = lagrange.io.FileEncoding.Ascii
                filename = self.tmp_dir / f"test{ext}"
                self.__save_and_load__(filename, mesh, save_options)

            # For binary formats
            for ext in [".ply", ".msh"]:
                save_options.encoding = lagrange.io.FileEncoding.Binary
                filename = self.tmp_dir / f"test{ext}"
                self.__save_and_load__(filename, mesh, save_options)

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
