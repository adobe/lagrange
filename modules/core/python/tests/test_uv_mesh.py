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
import copy
import gc
import sys
import weakref

import lagrange
import numpy as np
import pytest


def make_uv_mesh(indexed, hybrid):
    mesh = lagrange.SurfaceMesh()
    mesh.add_vertices(np.zeros((4, 3)))
    mesh.add_triangle(0, 1, 2)
    if hybrid:
        mesh.add_quad(0, 1, 2, 3)
    values = np.array([[0, 0], [1, 0], [0, 1], [1, 1]], dtype=np.float64)
    indices = None
    if indexed:
        indices = np.array([0, 1, 2, 2, 1, 3, 0] if hybrid else [0, 1, 2], dtype=np.uint32)
    mesh.create_attribute(
        "uv",
        element=lagrange.AttributeElement.Indexed if indexed else lagrange.AttributeElement.Vertex,
        usage=lagrange.AttributeUsage.UV,
        initial_values=values,
        initial_indices=indices,
    )
    return mesh


def source_uv_buffers(mesh, indexed):
    if indexed:
        attr = mesh.indexed_attribute("uv")
        return attr.values.data, attr.indices.data
    return mesh.attribute("uv").data, mesh.facets.reshape(-1)


@pytest.mark.parametrize("indexed", [False, True], ids=["vertex", "indexed"])
@pytest.mark.parametrize("hybrid", [False, True], ids=["regular", "hybrid"])
@pytest.mark.parametrize("readonly", [False, True], ids=["ref", "view"])
def test_uv_buffers_share_allocations_and_permissions(indexed, hybrid, readonly):
    mesh = make_uv_mesh(indexed, hybrid)
    uv_id = mesh.get_attribute_id("uv")
    tracked_ids = [uv_id]
    if not indexed:
        tracked_ids.append(mesh.attr_id_corner_to_vertex)
    if hybrid:
        tracked_ids.append(mesh.attr_id_facet_to_first_corner)
    counts = [mesh.__attribute_ref_count__(attr_id) for attr_id in tracked_ids]
    mesh_refcount = sys.getrefcount(mesh)

    extract = lagrange.uv_mesh_view if readonly else lagrange.uv_mesh_ref
    uv_mesh = extract(mesh, "uv")
    values, indices = source_uv_buffers(mesh, indexed)
    uv_values = uv_mesh.vertices
    uv_indices = uv_mesh.facets.reshape(-1)

    assert np.shares_memory(uv_values, values)
    assert np.shares_memory(uv_indices, indices)
    assert uv_values.flags.writeable is not readonly
    assert uv_indices.flags.writeable is not readonly
    assert sys.getrefcount(mesh) == mesh_refcount
    assert [mesh.__attribute_ref_count__(attr_id) for attr_id in tracked_ids] == counts
    if hybrid:
        offsets = mesh.attribute(mesh.attr_id_facet_to_first_corner).data
        uv_offsets = uv_mesh.attribute(uv_mesh.attr_id_facet_to_first_corner).data
        assert np.shares_memory(uv_offsets, offsets)
        assert uv_offsets.flags.writeable is not readonly
        assert [uv_mesh.get_facet_size(i) for i in range(2)] == [3, 4]
        if readonly:
            with pytest.raises(ValueError):
                uv_offsets[1] = 4
            inverse = uv_mesh.attribute(uv_mesh.attr_id_corner_to_facet).data
            np.testing.assert_array_equal(inverse, [0, 0, 0, 1, 1, 1, 1])
            with pytest.raises(ValueError):
                inverse[0] = 1

    if readonly:
        with pytest.raises(ValueError):
            uv_values[0, 0] = 9
        with pytest.raises(ValueError):
            uv_indices[0] = 3
        values[0, 0] = 5
        indices[0] = 3
    else:
        uv_values[0, 0] = 5
        uv_indices[0] = 3
    assert values[0, 0] == uv_values[0, 0] == 5
    assert indices[0] == uv_indices[0] == 3


@pytest.mark.parametrize("indexed", [False, True], ids=["vertex", "indexed"])
@pytest.mark.parametrize("hybrid", [False, True], ids=["regular", "hybrid"])
@pytest.mark.parametrize("readonly", [False, True], ids=["ref", "view"])
@pytest.mark.parametrize("rebind", [False, True], ids=["delete", "rebind-then-delete"])
def test_uv_mesh_outlives_source_allocations(indexed, hybrid, readonly, rebind):
    mesh = make_uv_mesh(indexed, hybrid)
    extract = lagrange.uv_mesh_view if readonly else lagrange.uv_mesh_ref
    uv_mesh = extract(mesh, "uv")
    values, indices = source_uv_buffers(mesh, indexed)
    expected_values = values.copy()
    expected_indices = indices.copy()
    del values, indices

    if rebind:
        if indexed:
            attr = mesh.indexed_attribute("uv")
            attr.values.data = np.full_like(expected_values, 8)
            attr.indices.data = np.zeros_like(expected_indices)
            del attr
        else:
            mesh.attribute("uv").data = np.full_like(expected_values, 8)
            mesh.attribute(mesh.attr_id_corner_to_vertex).data = np.zeros_like(expected_indices)
        if hybrid:
            mesh.attribute(mesh.attr_id_facet_to_first_corner).data = np.array(
                [0, 4], dtype=np.uint32
            )
            assert [mesh.get_facet_size(i) for i in range(2)] == [4, 3]
    del mesh
    gc.collect()

    # Export only after deleting the source, so the UV builder itself must own every buffer.
    np.testing.assert_array_equal(uv_mesh.vertices, expected_values)
    np.testing.assert_array_equal(uv_mesh.facets.reshape(-1), expected_indices)
    if hybrid:
        assert [uv_mesh.get_facet_size(i) for i in range(2)] == [3, 4]
        np.testing.assert_array_equal(
            uv_mesh.attribute(uv_mesh.attr_id_facet_to_first_corner).data, [0, 3]
        )
    if not readonly:
        uv_mesh.vertices[0, 0] = 7
        uv_mesh.facets.reshape(-1)[0] = 3
        assert uv_mesh.vertices[0, 0] == 7
        assert uv_mesh.facets.reshape(-1)[0] == 3


@pytest.mark.parametrize("indexed", [False, True], ids=["vertex", "indexed"])
@pytest.mark.parametrize("readonly", [False, True], ids=["ref", "view"])
def test_uv_extraction_preserves_genuine_mesh_cow(indexed, readonly):
    mesh = make_uv_mesh(indexed, hybrid=True)
    sibling = copy.copy(mesh)
    uv_id = mesh.get_attribute_id("uv")
    assert mesh.__attribute_ref_count__(uv_id) == 2
    extract = lagrange.uv_mesh_view if readonly else lagrange.uv_mesh_ref
    uv_mesh = extract(mesh, "uv")
    values, indices = source_uv_buffers(mesh, indexed)
    sibling_values, sibling_indices = source_uv_buffers(sibling, indexed)
    offsets = mesh.attribute(mesh.attr_id_facet_to_first_corner).data
    sibling_offsets = sibling.attribute(sibling.attr_id_facet_to_first_corner).data

    assert not np.shares_memory(values, sibling_values)
    assert not np.shares_memory(indices, sibling_indices)
    assert not np.shares_memory(offsets, sibling_offsets)
    if readonly:
        values[0, 0] = 9
        indices[0] = 3
        np.testing.assert_array_equal(uv_mesh.vertices[0], [9, 0])
        assert uv_mesh.facets.reshape(-1)[0] == 3
        assert sibling_values[0, 0] == 0
        assert sibling_indices[0] == 0
    else:
        uv_mesh.vertices[0, 0] = 9
        uv_mesh.facets.reshape(-1)[0] = 3
        assert values[0, 0] == 9
        assert indices[0] == 3
        assert sibling_values[0, 0] == 0
        assert sibling_indices[0] == 0
        np.testing.assert_array_equal(
            uv_mesh.attribute(uv_mesh.attr_id_facet_to_first_corner).data, [0, 3]
        )


@pytest.mark.parametrize("readonly", [False, True], ids=["ref", "view"])
def test_uv_mesh_keeps_foreign_buffer_owners_not_source_mesh(readonly):
    mesh = lagrange.SurfaceMesh()
    mesh.add_vertices(np.zeros((4, 3)))
    values = np.array([[0, 0], [1, 0], [0, 1], [1, 1]], dtype=np.float64)
    indices = np.array([0, 1, 2, 0, 1, 2, 3], dtype=np.uint32)
    offsets = np.array([0, 3], dtype=np.uint32)
    mesh.wrap_as_facets(offsets, 2, indices, 7)
    mesh.wrap_as_indexed_attribute("uv", lagrange.AttributeUsage.UV, values, indices)
    owner_refs = [weakref.ref(buffer) for buffer in (values, indices, offsets)]
    mesh_refcount = sys.getrefcount(mesh)
    extract = lagrange.uv_mesh_view if readonly else lagrange.uv_mesh_ref
    uv_mesh = extract(mesh, "uv")
    assert sys.getrefcount(mesh) == mesh_refcount
    del mesh, values, indices, offsets
    gc.collect()
    assert all(ref() is not None for ref in owner_refs)
    np.testing.assert_array_equal(uv_mesh.vertices, [[0, 0], [1, 0], [0, 1], [1, 1]])
    np.testing.assert_array_equal(uv_mesh.facets.reshape(-1), [0, 1, 2, 0, 1, 2, 3])
    assert [uv_mesh.get_facet_size(i) for i in range(2)] == [3, 4]
    del uv_mesh
    gc.collect()
    assert all(ref() is None for ref in owner_refs)


@pytest.mark.parametrize("indexed", [False, True], ids=["vertex", "indexed"])
def test_uv_ref_growth_and_shrink_detach_without_changing_source_policies(indexed):
    mesh = make_uv_mesh(indexed, hybrid=True)
    if indexed:
        uv_attr = mesh.indexed_attribute("uv")
        source_attrs = [uv_attr.values, uv_attr.indices]
    else:
        source_attrs = [mesh.attribute("uv")]
    source_attrs.append(mesh.attribute(mesh.attr_id_facet_to_first_corner))
    if not indexed:
        source_attrs.append(mesh.attribute(mesh.attr_id_corner_to_vertex))
    for attr in source_attrs:
        attr.growth_policy = lagrange.AttributeGrowthPolicy.AllowWithinCapacity
        attr.shrink_policy = lagrange.AttributeShrinkPolicy.IgnoreIfExternal

    uv_mesh = lagrange.uv_mesh_ref(mesh, "uv")
    uv_mesh.attribute(uv_mesh.attr_id_vertex_to_position).reserve_entries(32)
    uv_mesh.attribute(uv_mesh.attr_id_corner_to_vertex).reserve_entries(32)
    uv_mesh.attribute(uv_mesh.attr_id_facet_to_first_corner).reserve_entries(8)
    old_values = uv_mesh.vertices
    old_indices = uv_mesh.facets
    old_offsets = uv_mesh.attribute(uv_mesh.attr_id_facet_to_first_corner).data
    expected_indices = old_indices.copy()
    uv_mesh.shrink_to_fit()
    assert not np.shares_memory(uv_mesh.vertices, old_values)
    assert not np.shares_memory(uv_mesh.facets, old_indices)
    assert not np.shares_memory(
        uv_mesh.attribute(uv_mesh.attr_id_facet_to_first_corner).data, old_offsets
    )
    uv_mesh.add_vertex([2, 2])
    uv_mesh.add_triangle(0, 1, 4)
    uv_mesh.shrink_to_fit()

    assert uv_mesh.num_vertices == 5
    assert [uv_mesh.get_facet_size(i) for i in range(3)] == [3, 4, 3]
    assert old_values.shape == (4, 2)
    np.testing.assert_array_equal(old_values, [[0, 0], [1, 0], [0, 1], [1, 1]])
    np.testing.assert_array_equal(old_indices, expected_indices)
    np.testing.assert_array_equal(old_offsets, [0, 3])
    values, indices = source_uv_buffers(mesh, indexed)
    np.testing.assert_array_equal(values, old_values)
    np.testing.assert_array_equal(indices, expected_indices.reshape(-1))
    assert [mesh.get_facet_size(i) for i in range(2)] == [3, 4]
    for attr in source_attrs:
        assert attr.growth_policy == lagrange.AttributeGrowthPolicy.AllowWithinCapacity
        assert attr.shrink_policy == lagrange.AttributeShrinkPolicy.IgnoreIfExternal


@pytest.mark.parametrize("indexed", [False, True], ids=["vertex", "indexed"])
@pytest.mark.parametrize("hybrid", [False, True], ids=["regular", "hybrid"])
@pytest.mark.parametrize("readonly", [False, True], ids=["ref", "view"])
def test_native_source_mesh_grows_after_uv_extraction(indexed, hybrid, readonly):
    # UV extraction promotes the *source* buffers, so the source needs the same Python
    # storage defaults a plain NumPy export applies, including for natively created meshes.
    mesh = lagrange.combine_meshes([make_uv_mesh(indexed, hybrid)])
    extract = lagrange.uv_mesh_view if readonly else lagrange.uv_mesh_ref
    uv_mesh = extract(mesh, "uv")
    old_values = uv_mesh.vertices.copy()

    num_facets = mesh.num_facets
    mesh.add_vertex([1.0, 2.0, 3.0])
    mesh.add_triangle(0, 1, mesh.num_vertices - 1)

    assert mesh.num_vertices == 5
    assert mesh.num_facets == num_facets + 1
    # The extracted mesh keeps the allocation it was given.
    np.testing.assert_array_equal(uv_mesh.vertices, old_values)
