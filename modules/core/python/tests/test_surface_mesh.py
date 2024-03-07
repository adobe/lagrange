#
# Copyright 2022 Adobe. All rights reserved.
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
import sys

from .assets import single_triangle, single_triangle_with_index, cube
from .utils import address, assert_sharing_raw_data


class TestSurfaceMesh:
    def test_dimension(self):
        mesh = lagrange.SurfaceMesh()
        assert mesh.dimension == 3
        mesh2D = lagrange.SurfaceMesh(2)
        assert mesh2D.dimension == 2

    def test_add_vertex(self):
        vertices = np.eye(3)
        mesh = lagrange.SurfaceMesh()
        mesh.add_vertex(vertices[0])
        assert mesh.num_vertices == 1
        mesh.add_vertex(vertices[1])
        assert mesh.num_vertices == 2
        mesh.add_vertex(vertices[2])
        assert mesh.num_vertices == 3

        mesh.add_vertices(vertices)
        assert mesh.num_vertices == 6
        assert np.all(mesh.get_position(0) == mesh.get_position(3))
        assert np.all(mesh.get_position(1) == mesh.get_position(4))
        assert np.all(mesh.get_position(2) == mesh.get_position(5))

    def test_add_facets(self):
        vertices = np.eye(3)
        mesh = lagrange.SurfaceMesh()
        mesh.add_vertices(vertices)
        assert mesh.num_facets == 0

        mesh.add_triangle(0, 1, 2)
        assert mesh.num_facets == 1
        assert mesh.is_triangle_mesh
        assert not mesh.is_quad_mesh
        assert mesh.is_regular
        assert not mesh.is_hybrid

    def test_span_properties(self):
        mesh = lagrange.SurfaceMesh()
        mesh.add_vertices(np.eye(3))

        v = mesh.get_position(0)  # Read only span.
        assert isinstance(v, np.ndarray)
        assert not v.flags["OWNDATA"]

        v2 = mesh.ref_position(0)  # Writeable span.
        assert isinstance(v2, np.ndarray)
        assert not v2.flags["OWNDATA"]

        # TODO: dlpack data is read-only when exposed via numpy...
        # This is a known limitation.
        # assert v2.flags["WRITEABLE"]

        # v and v2 share to the same raw data address.
        assert_sharing_raw_data(v, v2)

    def test_attribute(self, single_triangle):
        mesh = single_triangle

        id = mesh.create_attribute(
            name="index",
            element=lagrange.AttributeElement.Vertex,
            usage=lagrange.AttributeUsage.Scalar,
            initial_values=np.array([1, 2, 3], dtype=float),
            initial_indices=np.array([], dtype=np.uint32),
        )
        assert mesh.get_attribute_name(id) == "index"

        attr = mesh.attribute(id)
        assert attr.usage == lagrange.AttributeUsage.Scalar
        assert attr.element_type == lagrange.AttributeElement.Vertex
        assert attr.num_channels == 1
        assert not attr.external

        attr2 = mesh.attribute("index")
        assert address(attr.data) == address(attr2.data)

        mesh.delete_attribute("index")
        del attr2

        with pytest.raises(RuntimeError) as e:
            data = attr.data

    def test_create_attribute_without_init_values(self, single_triangle):
        mesh = single_triangle

        with pytest.raises(Exception):
            # Without initial values, num_channels and dtype arguments are required.
            id = mesh.create_attribute(
                name="index",
                element=lagrange.AttributeElement.Vertex,
                usage=lagrange.AttributeUsage.Scalar,
            )

        for t in [
            float,
            np.float32,
            np.float64,
            np.int8,
            np.int16,
            np.int32,
            np.int64,
            np.uint8,
            np.uint16,
            np.uint32,
            np.uint64,
        ]:
            id = mesh.create_attribute(
                name=f"{t} type",
                element=lagrange.AttributeElement.Vertex,
                usage=lagrange.AttributeUsage.Scalar,
                dtype=t,
                num_channels=1,
            )

            attr = mesh.attribute(id)
            assert attr.usage == lagrange.AttributeUsage.Scalar
            assert attr.element_type == lagrange.AttributeElement.Vertex
            assert attr.num_channels == 1
            assert attr.data.dtype == t
            attr.data = np.ones(mesh.num_vertices, dtype=t)
            assert np.all(attr.data == 1)

    def test_edges(self, single_triangle, cube):
        mesh = single_triangle
        mesh.initialize_edges(
            np.array([[0, 1], [1, 2], [2, 0]], dtype=np.uint32)
        )
        assert mesh.has_edges
        assert mesh.num_edges == 3
        assert mesh.is_boundary_edge(0)
        assert mesh.is_boundary_edge(1)
        assert mesh.is_boundary_edge(2)
        assert mesh.count_num_corners_around_edge(0) == 1
        assert mesh.count_num_corners_around_edge(1) == 1
        assert mesh.count_num_corners_around_edge(2) == 1
        assert mesh.count_num_corners_around_vertex(0) == 1
        assert mesh.count_num_corners_around_vertex(1) == 1
        assert mesh.count_num_corners_around_vertex(2) == 1

        mesh = cube
        mesh.initialize_edges()
        assert mesh.has_edges
        assert mesh.num_edges == 12
        for ei in range(12):
            assert not mesh.is_boundary_edge(ei)
            assert mesh.count_num_corners_around_edge(ei) == 2
            v = mesh.get_edge_vertices(ei)
            assert len(v) == 2
            assert v[0] < mesh.num_vertices
            assert v[1] < mesh.num_vertices
        for vi in range(8):
            assert mesh.count_num_corners_around_vertex(vi) == 3

    def test_wrap_vertices(self):
        mesh = lagrange.SurfaceMesh()

        # allocate large buffer so there is room for growth.
        vertex_buffer = np.ndarray((10, 3), dtype=float)
        vertex_buffer[:3] = np.eye(3)
        mesh.wrap_as_vertices(vertex_buffer, 3)
        assert mesh.num_vertices == 3
        assert address(vertex_buffer) == address(mesh.vertices)

        # update vertex growth policy
        attr = mesh.attribute(mesh.attr_id_vertex_to_position)
        attr.growth_policy = lagrange.AttributeGrowthPolicy.AllowWithinCapacity

        assert np.all(mesh.vertices == np.eye(3))
        mesh.add_vertex(np.array([1, 2, 3], dtype=float))
        assert np.all(vertex_buffer[3] == [1, 2, 3])

    def test_wrap_facets_regular(self):
        mesh = lagrange.SurfaceMesh()

        # allocate large buffer.
        facets_buffer = np.ndarray((10, 3), dtype=np.uint32)
        facets_buffer[0] = [1, 2, 3]
        facets_buffer[1] = [4, 5, 6]

        mesh.wrap_as_facets(facets_buffer, 2, 3)
        assert mesh.num_facets == 2
        assert address(facets_buffer) == address(mesh.facets)

        # update facet growth policy
        attr = mesh.attribute(mesh.attr_id_corner_to_vertex)
        attr.growth_policy = lagrange.AttributeGrowthPolicy.AllowWithinCapacity

        mesh.add_triangle(7, 8, 9)
        assert mesh.num_facets == 3
        assert np.all(facets_buffer[2] == [7, 8, 9])

    def test_wrap_facets_hybrid(self):
        mesh = lagrange.SurfaceMesh()
        facets_buffer = np.arange(10, dtype=np.uint32)
        offsets_buffer = np.array([0, 3, 7], dtype=np.uint32)

        mesh.wrap_as_facets(offsets_buffer, 2, facets_buffer, 7)
        assert mesh.is_hybrid
        assert mesh.num_facets == 2
        assert mesh.get_facet_size(0) == 3
        assert mesh.get_facet_size(1) == 4
        assert np.all(mesh.get_facet_vertices(0) == [0, 1, 2])
        assert np.all(mesh.get_facet_vertices(1) == [3, 4, 5, 6])

        assert address(mesh.facets) == address(facets_buffer)
        offsets_attr = mesh.attribute(mesh.attr_id_facet_to_first_corner)
        assert address(offsets_attr.data) == address(offsets_buffer)

    def test_clone(self):
        mesh = lagrange.SurfaceMesh()
        mesh.add_vertex([0, 0, 0])
        mesh.add_vertex([1, 0, 0])
        mesh.add_vertex([0, 1, 0])
        mesh.add_vertex([1, 1, 0])
        mesh.add_triangle(0, 1, 2)
        mesh.add_triangle(2, 1, 3)

        mesh.create_attribute(
            "vertex_index",
            element=lagrange.AttributeElement.Vertex,
            usage=lagrange.AttributeUsage.Scalar,
            initial_values=np.arange(mesh.num_vertices, dtype=np.uint32),
        )
        mesh.create_attribute(
            "facet_index",
            element=lagrange.AttributeElement.Facet,
            usage=lagrange.AttributeUsage.Scalar,
            initial_values=np.arange(mesh.num_facets, dtype=np.uint32),
        )
        mesh.create_attribute(
            "uv",
            element=lagrange.AttributeElement.Indexed,
            usage=lagrange.AttributeUsage.UV,
            initial_values=mesh.vertices[:, :2].copy(),
            initial_indices=mesh.facets,
        )

        mesh2 = mesh.clone()
        assert mesh is not mesh2

        mesh.vertices.fill(0)
        assert np.amax(mesh2.vertices) != 0
        mesh.facets[[0, 1]] = mesh.facets[[1, 0]]
        assert np.all(mesh2.facets[0] == mesh.facets[1])
        assert np.all(mesh2.facets[1] == mesh.facets[0])

        assert mesh2.has_attribute("vertex_index")
        assert mesh2.has_attribute("facet_index")
        assert mesh2.has_attribute("uv")

        vertex_index = mesh2.attribute("vertex_index")
        facet_index = mesh2.attribute("facet_index")
        uv = mesh2.indexed_attribute("uv")

        assert not vertex_index.external
        assert not facet_index.external
        assert not uv.values.external
        assert not uv.indices.external

    def test_copy_and_deepcopy(self, single_triangle_with_index):
        from copy import copy, deepcopy

        mesh = single_triangle_with_index
        shallow_copy = copy(mesh)

        assert shallow_copy is not mesh
        assert_sharing_raw_data(mesh.vertices, shallow_copy.vertices)
        assert_sharing_raw_data(mesh.facets, shallow_copy.facets)
        assert_sharing_raw_data(
            mesh.attribute("vertex_index").data,
            shallow_copy.attribute("vertex_index").data,
        )

        deep_copy = deepcopy(mesh)
        assert deep_copy is not mesh
        assert address(mesh.vertices) != address(deep_copy.vertices)
        assert address(mesh.facets) != address(deep_copy.facets)
        assert address(mesh.attribute("vertex_index").data) != address(
            deep_copy.attribute("vertex_index").data
        )

    def test_position_usage(self, single_triangle):
        mesh = single_triangle
        position_attr = mesh.attribute(mesh.attr_id_vertex_to_position)
        assert position_attr.usage == lagrange.AttributeUsage.Position
