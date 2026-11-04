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
import copy
import gc
import sys

import lagrange
import numpy as np
import pytest

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

        assert not v.flags["WRITEABLE"]
        assert v2.flags["WRITEABLE"]

        # v and v2 share to the same raw data address.
        assert_sharing_raw_data(v, v2)

    @pytest.mark.parametrize("facets", [False, True], ids=["vertices", "facets"])
    def test_writable_exports_detach_shallow_mesh_copies(self, facets):
        mesh = lagrange.SurfaceMesh()
        mesh.add_vertices(np.eye(3))
        mesh.add_triangle(0, 1, 2)
        sibling = copy.copy(mesh)

        view = mesh.facets if facets else mesh.vertices
        view[0, 0] = 2 if facets else 9

        if facets:
            np.testing.assert_array_equal(mesh.facets, [[2, 1, 2]])
            np.testing.assert_array_equal(sibling.facets, [[0, 1, 2]])
        else:
            expected = np.eye(3)
            expected[0, 0] = 9
            np.testing.assert_array_equal(mesh.vertices, expected)
            np.testing.assert_array_equal(sibling.vertices, np.eye(3))

    def test_retained_vertex_view_stays_live_after_ref_position_write(self):
        mesh = lagrange.SurfaceMesh()
        mesh.add_vertices(np.eye(3))
        attr = mesh.attribute(mesh.attr_id_vertex_to_position)
        view = mesh.vertices

        mesh.ref_position(0)[:] = 7

        expected = np.eye(3)
        expected[0] = 7
        np.testing.assert_array_equal(mesh.vertices, expected)
        np.testing.assert_array_equal(view, expected)
        np.testing.assert_array_equal(attr.data, expected)

    @pytest.mark.parametrize("facets", [False, True], ids=["positions", "facets"])
    @pytest.mark.parametrize("readonly", [False, True], ids=["ref", "get"])
    def test_row_views_survive_rebinding_and_mesh_deletion(self, facets, readonly):
        mesh = lagrange.SurfaceMesh()
        mesh.add_vertices(np.eye(3))
        mesh.add_triangle(0, 1, 2)
        initial_refcount = sys.getrefcount(mesh)
        if facets:
            row = mesh.get_facet_vertices(0) if readonly else mesh.ref_facet_vertices(0)
        else:
            row = mesh.get_position(0) if readonly else mesh.ref_position(0)
        expected = row.copy()
        assert row.flags.writeable != readonly
        assert sys.getrefcount(mesh) == initial_refcount
        if facets:
            mesh.facets = np.array([[2, 1, 0]], dtype=np.uint32)
        else:
            mesh.vertices = np.full((3, 3), 7.0)
        del mesh
        np.testing.assert_array_equal(row, expected)

    @pytest.mark.parametrize(
        "derive",
        [
            lambda mesh: mesh,
            lambda mesh: copy.copy(mesh),
            lambda mesh: copy.deepcopy(mesh),
            lambda mesh: mesh.clone(),
        ],
        ids=["direct", "shallow", "deep", "clone"],
    )
    def test_natively_created_mesh_grows_after_export(self, derive):
        # Meshes produced by native algorithms carry the native `ErrorIfExternal` defaults.
        # Reading `vertices`/`facets` promotes their storage to external, so without applying
        # the Python defaults at that boundary an ordinary `add_vertex` would raise.
        source = lagrange.SurfaceMesh()
        source.add_vertices(np.eye(3))
        source.add_triangle(0, 1, 2)
        mesh = derive(lagrange.combine_meshes([source]))
        del source
        gc.collect()

        view = mesh.vertices
        assert mesh.attribute(mesh.attr_id_vertex_to_position).external
        _ = mesh.facets

        mesh.add_vertex([1.0, 0.0, 0.0])
        mesh.add_triangle(0, 1, 3)
        assert mesh.num_vertices == 4
        assert mesh.vertices.shape == (4, 3)
        assert mesh.facets.shape == (2, 3)
        # Growth detaches, leaving the previously exported allocation intact.
        np.testing.assert_array_equal(view, np.eye(3))

    def test_in_place_property_updates_do_not_leak(self):
        mesh = lagrange.SurfaceMesh()
        mesh.add_vertices(np.eye(3))
        mesh.add_triangle(0, 1, 2)
        initial_refcount = sys.getrefcount(mesh)

        mesh.vertices *= 2
        mesh.facets += 0

        assert np.array_equal(mesh.vertices, 2 * np.eye(3))
        assert np.array_equal(mesh.facets, [[0, 1, 2]])
        assert sys.getrefcount(mesh) == initial_refcount

    def test_property_assignment_wraps_numpy_storage(self):
        vertices = np.eye(3)
        facets = np.array([[0, 1, 2]], dtype=np.uint32)
        mesh = lagrange.SurfaceMesh()

        mesh.vertices = vertices
        mesh.facets = facets

        assert_sharing_raw_data(mesh.vertices, vertices)
        assert_sharing_raw_data(mesh.facets, facets)
        assert mesh.attribute(mesh.attr_id_vertex_to_position).external
        assert mesh.attribute(mesh.attr_id_corner_to_vertex).external

    def test_vertices_assignment_from_internal_subview(self):
        vertices = np.arange(12, dtype=float).reshape(4, 3)
        mesh = lagrange.SurfaceMesh()
        mesh.add_vertices(vertices)
        view = mesh.vertices

        mesh.vertices = view[1:]
        del view

        assert mesh.num_vertices == 3
        assert np.array_equal(mesh.vertices, vertices[1:])
        assert mesh.__attribute_ref_count__(mesh.attr_id_vertex_to_position) == 1

    def test_vertices_assignment_from_external_subview_remains_shared(self):
        vertices = np.arange(12, dtype=float).reshape(4, 3)
        mesh = lagrange.SurfaceMesh()
        mesh.vertices = vertices

        mesh.vertices = mesh.vertices[1:]
        vertices[1:] *= 2

        assert mesh.num_vertices == 3
        assert np.array_equal(mesh.vertices, vertices[1:])
        assert_sharing_raw_data(mesh.vertices, vertices[1:])
        assert mesh.attribute(mesh.attr_id_vertex_to_position).external

    def test_facets_assignment_from_internal_subview(self):
        facets = np.arange(9, dtype=np.uint32).reshape(3, 3)
        mesh = lagrange.SurfaceMesh()
        mesh.add_vertices(np.zeros((9, 3)))
        mesh.add_triangles(facets)
        view = mesh.facets

        mesh.facets = view[1:]
        del view

        assert mesh.num_facets == 2
        assert np.array_equal(mesh.facets, facets[1:])
        assert mesh.__attribute_ref_count__(mesh.attr_id_corner_to_vertex) == 1

    def test_facets_assignment_from_external_subview_remains_shared(self):
        facets = np.arange(9, dtype=np.uint32).reshape(3, 3)
        mesh = lagrange.SurfaceMesh()
        mesh.add_vertices(np.zeros((9, 3)))
        mesh.facets = facets

        mesh.facets = mesh.facets[1:]
        facets[1:, 0] = 0

        assert mesh.num_facets == 2
        assert np.array_equal(mesh.facets, facets[1:])
        assert_sharing_raw_data(mesh.facets, facets[1:])
        assert mesh.attribute(mesh.attr_id_corner_to_vertex).external

    def test_empty_tail_property_assignments_do_not_self_own(self):
        mesh = lagrange.SurfaceMesh()
        mesh.add_vertices(np.eye(3))
        vertices = mesh.vertices

        mesh.vertices = vertices[mesh.num_vertices :]
        del vertices

        assert mesh.num_vertices == 0
        assert mesh.__attribute_ref_count__(mesh.attr_id_vertex_to_position) == 1

        mesh = lagrange.SurfaceMesh()
        mesh.add_vertices(np.eye(3))
        mesh.add_triangle(0, 1, 2)
        facets = mesh.facets

        mesh.facets = facets[mesh.num_facets :]
        del facets

        assert mesh.num_facets == 0
        assert mesh.__attribute_ref_count__(mesh.attr_id_corner_to_vertex) == 1

    def test_empty_tail_explicit_wraps_do_not_self_own(self):
        mesh = lagrange.SurfaceMesh()
        mesh.add_vertices(np.eye(3))

        mesh.wrap_as_vertices(mesh.vertices[mesh.num_vertices :], 0)

        assert mesh.num_vertices == 0
        assert mesh.__attribute_ref_count__(mesh.attr_id_vertex_to_position) == 1

        mesh = lagrange.SurfaceMesh()
        mesh.add_vertices(np.eye(3))
        mesh.add_triangle(0, 1, 2)

        mesh.wrap_as_facets(mesh.facets[mesh.num_facets :], 0, 3)

        assert mesh.num_facets == 0
        assert mesh.__attribute_ref_count__(mesh.attr_id_corner_to_vertex) == 1

    def test_hybrid_self_wrap_does_not_self_own(self):
        mesh = lagrange.SurfaceMesh()
        mesh.add_vertices(np.zeros((4, 3)))
        mesh.add_triangle(0, 1, 2)
        mesh.add_quad(0, 1, 2, 3)
        offsets_attr = mesh.attribute(mesh.attr_id_facet_to_first_corner)
        offsets = offsets_attr.data
        facets = mesh.facets

        mesh.wrap_as_facets(offsets, mesh.num_facets, facets, mesh.num_corners)
        del offsets, facets

        assert mesh.is_hybrid
        assert mesh.get_facet_size(0) == 3
        assert mesh.get_facet_size(1) == 4
        assert mesh.__attribute_ref_count__(mesh.attr_id_facet_to_first_corner) == 1
        assert mesh.__attribute_ref_count__(mesh.attr_id_corner_to_vertex) == 1

    def test_in_place_hybrid_facets_update(self):
        mesh = lagrange.SurfaceMesh()
        mesh.add_vertices(np.zeros((4, 3)))
        mesh.add_triangle(0, 1, 2)
        mesh.add_quad(0, 1, 2, 3)

        mesh.facets += 0

        assert mesh.is_hybrid
        assert mesh.get_facet_size(0) == 3
        assert mesh.get_facet_size(1) == 4

    @pytest.mark.parametrize(
        "hybrid, setter",
        [(False, "property"), (False, "wrap"), (True, "property"), (True, "wrap")],
        ids=["regular-assignment", "regular-wrap", "hybrid-assignment", "hybrid-wrap"],
    )
    @pytest.mark.parametrize("same_buffer", [False, True], ids=["new-buffer", "same-buffer"])
    @pytest.mark.parametrize("with_edges", [False, True], ids=["no-edges", "with-edges"])
    def test_facet_updates_keep_edge_connectivity_consistent(
        self, hybrid, setter, same_buffer, with_edges, caplog
    ):
        mesh = lagrange.SurfaceMesh()
        mesh.add_vertices(np.zeros((5, 3)))
        mesh.add_triangle(0, 1, 2)
        if hybrid:
            mesh.add_quad(2, 1, 3, 4)
        else:
            mesh.add_triangle(2, 1, 3)
        if with_edges:
            mesh.initialize_edges()
            mesh.create_attribute(
                "edge_values",
                element=lagrange.AttributeElement.Edge,
                usage=lagrange.AttributeUsage.Scalar,
                initial_values=np.arange(mesh.num_edges, dtype=float),
            )
            mesh.attribute("edge_values").default_value = -1

        facets = mesh.facets
        if not same_buffer:
            facets = facets.copy()
        # Rotate the first triangle: its shared edge moves from corner 1 to 0,
        # while the shape and facet boundaries remain unchanged.
        facets.reshape(-1)[:3] = [1, 2, 0]
        if setter == "property":
            mesh.facets = facets
        elif hybrid:
            offsets = np.array([0, 3], dtype=np.uint32)
            mesh.wrap_as_facets(offsets, mesh.num_facets, facets, mesh.num_corners)
        else:
            mesh.wrap_as_facets(facets, mesh.num_facets, mesh.vertex_per_facet)

        expected_facets = [[1, 2, 0], [2, 1, 3, 4] if hybrid else [2, 1, 3]]
        if hybrid and setter == "property" and not same_buffer:
            expected_facets = [[vertex for facet in expected_facets for vertex in facet]]
        assert mesh.num_facets == len(expected_facets)
        for facet_id, vertices in enumerate(expected_facets):
            np.testing.assert_array_equal(mesh.get_facet_vertices(facet_id), vertices)

        warning = "Rebuilding edge connectivity after facet assignment"
        assert mesh.has_edges == with_edges
        assert (warning in caplog.text) == with_edges
        if with_edges:
            edge_attr = mesh.attribute("edge_values")
            assert edge_attr.num_elements == mesh.num_edges
            np.testing.assert_array_equal(edge_attr.data, np.full(mesh.num_edges, -1.0))
        else:
            mesh.initialize_edges()
        corner_id = 0
        for facet_id, vertices in enumerate(expected_facets):
            for local_corner, vertex in enumerate(vertices):
                expected_edge = sorted([vertex, vertices[(local_corner + 1) % len(vertices)]])
                edge = mesh.get_corner_edge(corner_id)
                assert sorted(mesh.get_edge_vertices(edge)) == expected_edge, (
                    f"Stale edge connectivity at facet {facet_id}, corner {local_corner}"
                )
                corner_id += 1

    @pytest.mark.parametrize("setter", ["property", "regular-wrap", "hybrid-wrap"])
    @pytest.mark.parametrize("empty", [False, True], ids=["triangle", "empty"])
    def test_facet_replacement_rebuilds_edges_and_resizes_edge_attributes(
        self, setter, empty, caplog
    ):
        mesh = lagrange.SurfaceMesh()
        mesh.add_vertices(np.zeros((6, 3)))
        mesh.add_triangles(np.arange(6, dtype=np.uint32).reshape(2, 3))
        mesh.initialize_edges()
        edge_attr_id = mesh.create_attribute(
            "edge_values",
            element=lagrange.AttributeElement.Edge,
            usage=lagrange.AttributeUsage.Scalar,
            initial_values=np.arange(mesh.num_edges, dtype=float),
        )
        mesh.attribute(edge_attr_id).default_value = -1
        facets = np.array([[0, 1, 2]], dtype=np.uint32)[: 0 if empty else 1]
        if setter == "property":
            mesh.facets = facets
        elif setter == "regular-wrap":
            mesh.wrap_as_facets(facets, len(facets), 3)
        else:
            offsets = np.array([0], dtype=np.uint32)[: len(facets)]
            mesh.wrap_as_facets(offsets, len(facets), facets.reshape(-1), facets.size)

        assert mesh.has_edges
        assert mesh.num_edges == facets.size
        edge_attr = mesh.attribute(edge_attr_id)
        assert edge_attr.num_elements == mesh.num_edges
        np.testing.assert_array_equal(edge_attr.data, np.full(mesh.num_edges, -1.0))
        assert "Rebuilding edge connectivity after facet assignment" in caplog.text

    def test_facet_reshape_changes_topology(self):
        mesh = lagrange.SurfaceMesh()
        mesh.add_vertices(np.zeros((6, 3)))
        mesh.add_triangle(0, 1, 2)
        mesh.add_triangle(3, 4, 5)

        mesh.facets = mesh.facets.reshape(1, 6)

        assert mesh.num_facets == 1
        assert mesh.vertex_per_facet == 6
        assert np.array_equal(mesh.facets, [[0, 1, 2, 3, 4, 5]])
        assert mesh.__attribute_ref_count__(mesh.attr_id_corner_to_vertex) == 1

    def test_empty_mesh_views_do_not_retain_mesh(self):
        mesh = lagrange.SurfaceMesh()
        initial_refcount = sys.getrefcount(mesh)

        vertices = mesh.vertices
        mesh.wrap_as_attribute(
            "empty",
            lagrange.AttributeElement.Vertex,
            lagrange.AttributeUsage.Position,
            vertices,
        )

        assert sys.getrefcount(mesh) == initial_refcount

    def test_wrapping_mesh_views_preserves_sharing_without_leaking(self):
        vertex_buffer = np.eye(3)
        mesh = lagrange.SurfaceMesh()
        mesh.wrap_as_vertices(vertex_buffer, 3)
        mesh.add_triangle(0, 1, 2)
        initial_refcount = sys.getrefcount(mesh)

        mesh.wrap_as_attribute(
            "position_copy",
            lagrange.AttributeElement.Vertex,
            lagrange.AttributeUsage.Position,
            mesh.vertices,
        )
        mesh.wrap_as_indexed_attribute(
            "indexed_copy",
            lagrange.AttributeUsage.Vector,
            mesh.vertices,
            mesh.facets.reshape(-1),
        )
        copied_attr_id = mesh.create_attribute(
            "assigned_copy",
            element=lagrange.AttributeElement.Vertex,
            usage=lagrange.AttributeUsage.Position,
            initial_values=np.zeros((3, 3)),
        )
        copied_attr = mesh.attribute(copied_attr_id)
        copied_attr.data = mesh.vertices

        vertex_buffer *= 2

        assert np.array_equal(mesh.vertices, 2 * np.eye(3))
        assert np.array_equal(mesh.attribute("position_copy").data, mesh.vertices)
        indexed_copy = mesh.indexed_attribute("indexed_copy")
        assert np.array_equal(indexed_copy.values.data, mesh.vertices)
        assert np.array_equal(indexed_copy.indices.data, [0, 1, 2])
        assert np.array_equal(copied_attr.data, mesh.vertices)
        assert_sharing_raw_data(copied_attr.data, mesh.vertices)
        assert sys.getrefcount(mesh) == initial_refcount

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

        with pytest.raises(RuntimeError):
            attr.data

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

    def test_create_attribute_with_init_values(self, single_triangle):
        mesh = single_triangle

        with pytest.raises(Exception):
            # Ambiguous element type: could be vertex or corner.
            mesh.create_attribute("vertex_index", initial_values=[1, 2, 3])

        # Vertex attribute
        mesh.add_vertex([1, 1, 1])  # Add an isolated vertex.
        id = mesh.create_attribute("vertex_index", initial_values=[1, 2, 3, 4])
        attr = mesh.attribute(id)
        assert attr.element_type == lagrange.AttributeElement.Vertex
        assert attr.usage == lagrange.AttributeUsage.Scalar
        assert np.all(attr.data == [1, 2, 3, 4])
        assert attr.dtype == np.float64
        assert attr.dtype == attr.data.dtype

        # Corner attribute
        id = mesh.create_attribute("corner_index", initial_values=np.array([1, 2, 3]))
        attr = mesh.attribute(id)
        assert attr.element_type == lagrange.AttributeElement.Corner
        assert attr.usage == lagrange.AttributeUsage.Scalar
        assert np.all(attr.data == [1, 2, 3])
        assert attr.dtype == attr.data.dtype

        # Facet attribute
        id = mesh.create_attribute("facet_index", initial_values=[0], num_channels=1)
        attr = mesh.attribute(id)
        assert attr.element_type == lagrange.AttributeElement.Facet
        assert attr.usage == lagrange.AttributeUsage.Scalar
        assert attr.dtype == np.float64
        assert attr.dtype == attr.data.dtype

        # Indexed attribute
        id = mesh.create_attribute(
            "custom", initial_values=np.array([0, 1]), initial_indices=[0, 0, 1]
        )
        assert mesh.is_attribute_indexed(id)
        attr = mesh.indexed_attribute(id)
        assert attr.usage == lagrange.AttributeUsage.Scalar
        assert np.all(attr.indices.data == [0, 0, 1])
        assert np.issubdtype(attr.values.dtype, np.integer)
        assert attr.indices.dtype == np.uint32
        assert attr.values.dtype == attr.values.data.dtype
        assert attr.indices.dtype == attr.indices.data.dtype

        # Indexed attribute with incorrect index dtype
        id = mesh.create_attribute(
            "custom2",
            initial_values=np.array([0, 1]),
            initial_indices=np.array([0, 0, 1]),
        )
        assert mesh.is_attribute_indexed(id)
        attr = mesh.indexed_attribute(id)
        assert attr.usage == lagrange.AttributeUsage.Scalar
        assert np.all(attr.indices.data == [0, 0, 1])
        assert np.issubdtype(attr.values.dtype, np.integer)
        assert attr.indices.dtype == np.uint32

    def test_edges(self, single_triangle, cube):
        mesh = single_triangle
        mesh.initialize_edges(np.array([[0, 1], [1, 2], [2, 0]], dtype=np.uint32))
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

    def test_flip_facets(self, cube):
        mesh = cube
        mesh.flip_facets([0, 2])
        mesh.flip_facets(np.array([1, 3], dtype=np.uint32))

    def test_copy_and_deepcopy(self, single_triangle_with_index):
        from copy import copy, deepcopy

        mesh = single_triangle_with_index
        shallow_copy = copy(mesh)

        assert shallow_copy is not mesh
        # Exporting writable vertex/facet views triggers CoW on the shared attribute data.
        assert not np.shares_memory(mesh.vertices, shallow_copy.vertices)
        assert not np.shares_memory(mesh.facets, shallow_copy.facets)
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

    def test_metadata(self, single_triangle):
        mesh = single_triangle
        mesh.metadata["author"] = "John"
        assert mesh.metadata["author"] == "John"
        mesh.metadata["author"] = "Jane"
        assert mesh.metadata["author"] == "Jane"
        mesh.metadata["author"] = "🐶"
        assert mesh.metadata["author"] == "🐶"

    def test_1_ring_callback(self, cube):
        mesh = cube
        mesh.initialize_edges()

        facet_one_ring = []
        mesh.foreach_facet_around_facet(0, lambda fid: facet_one_ring.append(fid))
        assert set(facet_one_ring) == {2, 3, 4, 5}

        vertex_one_ring = []
        mesh.foreach_facet_around_vertex(0, lambda fid: vertex_one_ring.append(fid))
        assert set(vertex_one_ring) == {0, 3, 5}

        edge_one_ring = []
        mesh.foreach_facet_around_edge(0, lambda fid: edge_one_ring.append(fid))
        assert len(edge_one_ring) == 2

        edge_one_ring_corner = []
        mesh.foreach_corner_around_edge(0, lambda cid: edge_one_ring_corner.append(cid))
        assert len(edge_one_ring_corner) == 2

        for cid in edge_one_ring_corner:
            fid = mesh.get_corner_facet(cid)
            assert fid in edge_one_ring

        vertex_one_ring_edges_list = []
        mesh.foreach_edge_around_vertex(0, lambda eid: vertex_one_ring_edges_list.append(eid))
        vertex_one_ring_edges = set(vertex_one_ring_edges_list)
        assert len(vertex_one_ring_edges) == len(vertex_one_ring)

        for eid in vertex_one_ring_edges:
            v0, v1 = mesh.get_edge_vertices(eid)
            assert 0 in (v0, v1)

    def test_1_ring_pythonic(self, cube):
        mesh = cube
        mesh.initialize_edges()

        # Test facets_around_facet
        facet_one_ring = mesh.facets_around_facet(0)
        assert set(facet_one_ring) == {2, 3, 4, 5}

        # Test facets_around_vertex
        vertex_one_ring = mesh.facets_around_vertex(0)
        assert set(vertex_one_ring) == {0, 3, 5}

        # Test facets_around_edge
        edge_one_ring = mesh.facets_around_edge(0)
        assert len(edge_one_ring) == 2

        # Test corners_around_edge
        edge_one_ring_corner = mesh.corners_around_edge(0)
        assert len(edge_one_ring_corner) == 2

        for cid in edge_one_ring_corner:
            fid = mesh.get_corner_facet(cid)
            assert fid in edge_one_ring

        # Test edges_around_vertex
        vertex_one_ring_edges = mesh.edges_around_vertex(0)
        vertex_one_ring_edges = set(vertex_one_ring_edges)
        assert len(vertex_one_ring_edges) == len(vertex_one_ring)

        for eid in vertex_one_ring_edges:
            v0, v1 = mesh.get_edge_vertices(eid)
            assert 0 in (v0, v1)

        # Test list comprehension style
        facets = [fid for fid in mesh.facets_around_facet(0)]
        assert set(facets) == {2, 3, 4, 5}
