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
import sys
import weakref

import lagrange
import numpy as np
import pytest

from .utils import assert_sharing_raw_data


class TestAttribute:
    def test_basics(self, single_triangle_with_index):
        mesh = single_triangle_with_index
        attr = mesh.attribute("vertex_index")
        assert attr.num_channels == 1
        assert attr.usage == lagrange.AttributeUsage.Scalar
        assert attr.element_type == lagrange.AttributeElement.Vertex
        assert attr.external

        data = attr.data
        assert not data.flags["OWNDATA"]
        # assert not data.flags["WRITEABLE"]
        assert len(data.shape) == 1
        assert data.shape[0] == 3

    def test_attribute_wrap(self, single_triangle_with_index):
        mesh = single_triangle_with_index
        attr = mesh.attribute("vertex_index")
        assert attr.num_channels == 1

        # Wrap should cause attribute to keep `data` alive.
        data = np.array([1, 2, 3], dtype=np.intc)
        data_ref_count = sys.getrefcount(data)
        attr.data = data
        assert attr.external
        assert sys.getrefcount(data) == data_ref_count + 1
        assert_sharing_raw_data(attr.data, data)

        # Rewrap should release attribute's hold on `data`.
        data2 = np.array([4, 5, 6], dtype=np.intc)
        data2_ref_count = sys.getrefcount(data2)
        attr.data = data2
        assert attr.external
        assert sys.getrefcount(data) == data_ref_count
        assert sys.getrefcount(data2) == data2_ref_count + 1
        assert_sharing_raw_data(attr.data, data2)

    def test_in_place_update_does_not_rewrap(self, single_triangle):
        mesh = single_triangle
        attr_id = mesh.create_attribute(
            "index",
            element=lagrange.AttributeElement.Vertex,
            usage=lagrange.AttributeUsage.Scalar,
            initial_values=[1, 2, 3],
        )
        attr = mesh.attribute(attr_id)
        assert not attr.external

        attr.data *= 2

        assert np.array_equal(attr.data, [2, 4, 6])

    def test_external_subview_remains_shared(self, single_triangle):
        attr_id = single_triangle.create_attribute(
            "values",
            element=lagrange.AttributeElement.Value,
            usage=lagrange.AttributeUsage.Scalar,
            num_channels=1,
            dtype=np.int32,
        )
        attr = single_triangle.attribute(attr_id)
        values = np.arange(4, dtype=np.int32)
        attr.data = values

        attr.data = values[1:]
        values[1:] *= 2

        assert np.array_equal(attr.data, [2, 4, 6])
        assert attr.external
        assert_sharing_raw_data(attr.data, values[1:])

    def test_sibling_external_subview_remains_shared(self, single_triangle):
        values = np.arange(4, dtype=np.int32)
        attrs = []
        for name in ["a", "b"]:
            attr_id = single_triangle.create_attribute(
                name,
                element=lagrange.AttributeElement.Value,
                usage=lagrange.AttributeUsage.Scalar,
                num_channels=1,
                dtype=np.int32,
            )
            attr = single_triangle.attribute(attr_id)
            attr.data = values
            attrs.append(attr)

        attrs[0].data = attrs[1].data[1:]
        values[1:] *= 2

        assert np.array_equal(attrs[0].data, [2, 4, 6])
        assert_sharing_raw_data(attrs[0].data, values[1:])

    def test_getter_subview_does_not_self_own(self, single_triangle):
        attr_id = single_triangle.create_attribute(
            "values",
            element=lagrange.AttributeElement.Value,
            usage=lagrange.AttributeUsage.Scalar,
            initial_values=np.arange(4, dtype=np.int32),
        )
        attr = single_triangle.attribute(attr_id)
        data = attr.data

        attr.data = data[1:]
        assert_sharing_raw_data(attr.data, data[1:])
        del data

        assert np.array_equal(attr.data, [1, 2, 3])
        assert single_triangle.__attribute_ref_count__(attr_id) == 1

    def test_reciprocal_data_assignment_does_not_create_ownership_cycle(self, single_triangle):
        attr_ids = [
            single_triangle.create_attribute(
                name,
                element=lagrange.AttributeElement.Value,
                usage=lagrange.AttributeUsage.Scalar,
                initial_values=np.array(values, dtype=np.int32),
            )
            for name, values in [("a", [1, 2, 3]), ("b", [4, 5, 6])]
        ]
        attr1, attr2 = [single_triangle.attribute(attr_id) for attr_id in attr_ids]

        attr1.data = attr2.data
        attr2.data = attr1.data

        attr1.data[0] = 7
        np.testing.assert_array_equal(attr2.data, [7, 5, 6])
        attr2.data[1] = 8
        np.testing.assert_array_equal(attr1.data, [7, 8, 6])

        view = attr1.data
        for attr_id in attr_ids:
            single_triangle.delete_attribute(attr_id)
        for attr in (attr1, attr2):
            with pytest.raises(RuntimeError, match="Attribute is no longer valid"):
                _ = attr.data
        np.testing.assert_array_equal(view, [7, 8, 6])

    @pytest.mark.parametrize(
        "growth,shrink",
        [
            (None, None),
            (
                lagrange.AttributeGrowthPolicy.ErrorIfExtenal,
                lagrange.AttributeShrinkPolicy.ErrorIfExternal,
            ),
            (
                lagrange.AttributeGrowthPolicy.AllowWithinCapacity,
                lagrange.AttributeShrinkPolicy.IgnoreIfExternal,
            ),
            (
                lagrange.AttributeGrowthPolicy.WarnAndCopy,
                lagrange.AttributeShrinkPolicy.WarnAndCopy,
            ),
        ],
        ids=["default", "error", "capacity", "warn"],
    )
    def test_internal_export_normalizes_error_policies_and_preserves_views(
        self, single_triangle, growth, shrink
    ):
        # Only the internal-to-managed transition normalizes error policies. Custom policies
        # still govern resizing, and all exported arrays retain their original allocation.
        mesh = single_triangle
        attr_id = mesh.create_attribute(
            "values",
            element=lagrange.AttributeElement.Value,
            usage=lagrange.AttributeUsage.Scalar,
            initial_values=[1.0, 2.0, 3.0, 4.0],
        )
        attr = mesh.attribute(attr_id)
        attr.reserve_entries(8)
        if growth is not None:
            attr.growth_policy = growth
            attr.shrink_policy = shrink
        expected_growth = attr.growth_policy
        expected_shrink = attr.shrink_policy
        if expected_growth == lagrange.AttributeGrowthPolicy.ErrorIfExtenal:
            expected_growth = lagrange.AttributeGrowthPolicy.SilentCopy
        if expected_shrink == lagrange.AttributeShrinkPolicy.ErrorIfExternal:
            expected_shrink = lagrange.AttributeShrinkPolicy.SilentCopy

        view = attr.data
        assert attr.external
        assert attr.growth_policy == expected_growth
        assert attr.shrink_policy == expected_shrink

        attr.insert_elements(1)
        assert attr.num_elements == 5
        assert attr.growth_policy == expected_growth
        np.testing.assert_array_equal(attr.data[:4], [1, 2, 3, 4])
        # Whether the resize detaches or stays in place, the exported array keeps its extent.
        np.testing.assert_array_equal(view, [1, 2, 3, 4])

        attr.shrink_to_fit()
        assert attr.shrink_policy == expected_shrink
        np.testing.assert_array_equal(view, [1, 2, 3, 4])

    def test_post_promotion_error_policies_survive_export_and_reject_resize(self, single_triangle):
        attr_id = single_triangle.create_attribute(
            "values",
            element=lagrange.AttributeElement.Value,
            usage=lagrange.AttributeUsage.Scalar,
            initial_values=[1.0, 2.0, 3.0, 4.0],
        )
        attr = single_triangle.attribute(attr_id)
        attr.reserve_entries(8)
        attr.ensure_shared_storage()
        attr.growth_policy = lagrange.AttributeGrowthPolicy.ErrorIfExtenal
        attr.shrink_policy = lagrange.AttributeShrinkPolicy.ErrorIfExternal

        attr.ensure_shared_storage()
        view = attr.data
        with pytest.raises(RuntimeError, match="prevents growing"):
            attr.insert_elements(1)
        with pytest.raises(RuntimeError, match="prevents shrinking"):
            attr.shrink_to_fit()
        assert attr.num_elements == 4
        assert_sharing_raw_data(attr.data, view)
        np.testing.assert_array_equal(view, [1, 2, 3, 4])

    def test_wrapped_foreign_buffer_still_obeys_its_policy(self, single_triangle):
        # A NumPy array really is a foreign buffer, so policies apply to it as usual.
        mesh = single_triangle
        attr_id = mesh.create_attribute(
            "values",
            element=lagrange.AttributeElement.Value,
            usage=lagrange.AttributeUsage.Scalar,
            num_channels=1,
            dtype=np.float64,
        )
        attr = mesh.attribute(attr_id)
        attr.data = np.arange(4.0)
        # Wrapping selects the Python default.
        assert attr.growth_policy == lagrange.AttributeGrowthPolicy.SilentCopy

        attr.growth_policy = lagrange.AttributeGrowthPolicy.ErrorIfExtenal
        _ = attr.data
        assert attr.growth_policy == lagrange.AttributeGrowthPolicy.ErrorIfExtenal
        with pytest.raises(RuntimeError, match="prevents growing"):
            attr.insert_elements(1)

    @pytest.mark.parametrize("mode", ["shallow", "deep", "duplicate"])
    def test_copy_preserves_explicit_policies_after_export(self, single_triangle, mode):
        mesh = single_triangle
        attr_id = mesh.create_attribute(
            "values",
            element=lagrange.AttributeElement.Value,
            usage=lagrange.AttributeUsage.Scalar,
            initial_values=[1.0, 2.0, 3.0, 4.0],
        )
        attr = mesh.attribute(attr_id)
        attr.growth_policy = lagrange.AttributeGrowthPolicy.AllowWithinCapacity
        attr.shrink_policy = lagrange.AttributeShrinkPolicy.IgnoreIfExternal
        if mode == "duplicate":
            copied_id = mesh.duplicate_attribute("values", "copy")
            copied_attr = mesh.attribute(copied_id)
        else:
            copied_mesh = copy.copy(mesh) if mode == "shallow" else copy.deepcopy(mesh)
            copied_attr = copied_mesh.attribute(attr_id)
        view = copied_attr.data
        assert copied_attr.growth_policy == lagrange.AttributeGrowthPolicy.AllowWithinCapacity
        assert copied_attr.shrink_policy == lagrange.AttributeShrinkPolicy.IgnoreIfExternal
        np.testing.assert_array_equal(view, [1, 2, 3, 4])

    def test_native_created_attribute_growth_after_identifier_reuse(self, single_triangle):
        mesh = single_triangle
        mesh.metadata["old"] = "first"
        attr = mesh.attribute("old")
        attr.growth_policy = lagrange.AttributeGrowthPolicy.ErrorIfExtenal
        del attr
        del mesh.metadata["old"]
        mesh.metadata["new"] = "second"
        attr = mesh.attribute("new")
        view = attr.data
        expected = view.copy()
        attr.reserve_entries(attr.num_elements + 16)
        np.testing.assert_array_equal(view, expected)
        np.testing.assert_array_equal(attr.data, expected)

    def test_rename_after_cow_preserves_explicit_policies(self, single_triangle):
        mesh = single_triangle
        attr_id = mesh.create_attribute(
            "values",
            element=lagrange.AttributeElement.Value,
            usage=lagrange.AttributeUsage.Scalar,
            initial_values=[1.0, 2.0, 3.0, 4.0],
        )
        mesh.attribute(attr_id).growth_policy = lagrange.AttributeGrowthPolicy.AllowWithinCapacity
        sibling = copy.copy(mesh)
        mesh.attribute(attr_id, sharing=False)
        del sibling
        mesh.rename_attribute("values", "renamed")
        attr = mesh.attribute("renamed")
        view = attr.data
        assert attr.growth_policy == lagrange.AttributeGrowthPolicy.AllowWithinCapacity
        # Custom policies continue to govern promoted storage.
        with pytest.raises(RuntimeError, match="beyond capacity"):
            attr.insert_elements(1)
        np.testing.assert_array_equal(view, [1, 2, 3, 4])

    def test_growth_and_shrink_keep_old_allocations_alive(self, single_triangle):
        attr_id = single_triangle.create_attribute(
            "values",
            element=lagrange.AttributeElement.Value,
            usage=lagrange.AttributeUsage.Scalar,
            initial_values=[1.0, 2.0, 3.0, 4.0],
        )
        attr = single_triangle.attribute(attr_id)
        first = attr.data
        attr.reserve_entries(32)
        padded = attr.data
        attr.shrink_to_fit()
        current = attr.data
        assert not np.shares_memory(first, padded)
        assert not np.shares_memory(padded, current)
        current[0] = 9
        attr.insert_elements([5.0, 6.0])
        np.testing.assert_array_equal(first, [1, 2, 3, 4])
        np.testing.assert_array_equal(padded, [1, 2, 3, 4])
        np.testing.assert_array_equal(current, [9, 2, 3, 4])
        np.testing.assert_array_equal(attr.data, [9, 2, 3, 4, 5, 6])

    def test_readonly_numpy_input_is_copied_not_aliased(self, single_triangle):
        # Wrapping requires a writable buffer. Assignment falls back to a value copy, so a
        # read-only source is never aliased by writable attribute storage.
        original = np.arange(single_triangle.num_vertices, dtype=np.float64)
        original.flags.writeable = False
        attr_id = single_triangle.create_attribute(
            "values",
            element=lagrange.AttributeElement.Vertex,
            usage=lagrange.AttributeUsage.Scalar,
            num_channels=1,
            dtype=np.float64,
        )
        attr = single_triangle.attribute(attr_id)
        attr.data = original
        assert not attr.readonly
        assert not np.shares_memory(np.asarray(attr.data), original)

        attr.data[0] = 9
        np.testing.assert_array_equal(attr.data, [9, 1, 2])
        np.testing.assert_array_equal(original, [0, 1, 2])

        with pytest.raises(TypeError):
            single_triangle.wrap_as_attribute(
                "wrapped",
                lagrange.AttributeElement.Vertex,
                lagrange.AttributeUsage.Scalar,
                original,
            )

    def test_foreign_buffer_preserves_owner(self, single_triangle):
        values = np.arange(single_triangle.num_vertices, dtype=np.float64)
        buffer = memoryview(values)
        attr_id = single_triangle.create_attribute(
            "values",
            element=lagrange.AttributeElement.Vertex,
            usage=lagrange.AttributeUsage.Scalar,
            num_channels=1,
            dtype=np.float64,
        )
        attr = single_triangle.attribute(attr_id)
        attr.data = buffer
        # Read through another handle: buffer assignment does not determine the getter's type.
        view = single_triangle.attribute(attr_id).data
        assert view.flags.writeable
        assert np.shares_memory(view, values)
        values[1] = 9
        del buffer, values
        np.testing.assert_array_equal(view, [0, 9, 2])
        np.testing.assert_array_equal(attr.data, [0, 9, 2])

    def test_saved_full_view_restoration_preserves_owner(self, single_triangle):
        mesh = single_triangle
        attr_id = mesh.create_attribute(
            "values",
            element=lagrange.AttributeElement.Value,
            usage=lagrange.AttributeUsage.Scalar,
            num_channels=1,
            dtype=np.float64,
        )
        attr = mesh.attribute(attr_id)
        original = np.arange(4, dtype=np.float64)
        original_ref = weakref.ref(original)
        replacement = np.zeros_like(original)

        try:
            attr.data = original
            full = attr.data
            attr.data = full[1:]
            attr.data = full
            del full, original

            # Check lifetime before reading a buffer that may have been freed.
            assert original_ref() is not None
            assert mesh.__attribute_ref_count__(attr_id) == 1
            np.testing.assert_array_equal(attr.data, np.arange(4, dtype=np.float64))
        finally:
            # Rewrap independent live storage without copying potentially freed values.
            attr.data = replacement

    def test_cross_attribute_view_swap_does_not_create_cycle(self, single_triangle):
        mesh = single_triangle
        originals = [np.arange(4, dtype=np.float64), np.arange(4, 8, dtype=np.float64)]
        replacement = np.zeros(4, dtype=np.float64)
        attr_ids = []
        attrs = []
        for name, values in zip(["a", "b"], originals):
            attr_id = mesh.create_attribute(
                name,
                element=lagrange.AttributeElement.Value,
                usage=lagrange.AttributeUsage.Scalar,
                num_channels=1,
                dtype=np.float64,
            )
            attr = mesh.attribute(attr_id)
            attr.data = values
            attr_ids.append(attr_id)
            attrs.append(attr)

        try:
            a_view, b_view = attrs[0].data, attrs[1].data
            attrs[0].data = b_view
            attrs[1].data = a_view
            del a_view, b_view

            np.testing.assert_array_equal(attrs[0].data, originals[1])
            np.testing.assert_array_equal(attrs[1].data, originals[0])
            assert tuple(mesh.__attribute_ref_count__(attr_id) for attr_id in attr_ids) == (1, 1)
        finally:
            # Break both ownership edges even when the assertion fails.
            for attr in attrs:
                attr.data = replacement

    def test_explicit_end_empty_view_does_not_self_own(self, single_triangle):
        mesh = single_triangle
        attr_id = mesh.create_attribute(
            "values",
            element=lagrange.AttributeElement.Value,
            usage=lagrange.AttributeUsage.Scalar,
            initial_values=[1.0, 2.0, 3.0, 4.0],
        )
        attr = mesh.attribute(attr_id)
        replacement = np.zeros(4, dtype=np.float64)

        try:
            view = attr.data
            empty = np.ndarray(shape=(0,), dtype=view.dtype, buffer=view, offset=view.nbytes)
            attr.data = empty
            del empty, view

            assert attr.num_elements == 0
            assert mesh.__attribute_ref_count__(attr_id) == 1
        finally:
            attr.data = replacement

    def test_source_rebinding_preserves_wrapped_view_owner(self, single_triangle):
        mesh = single_triangle
        attrs = []
        for name in ["src", "dst"]:
            attr_id = mesh.create_attribute(
                name,
                element=lagrange.AttributeElement.Value,
                usage=lagrange.AttributeUsage.Scalar,
                num_channels=1,
                dtype=np.float64,
            )
            attrs.append(mesh.attribute(attr_id))
        src, dst = attrs
        original = np.arange(4, dtype=np.float64)
        original_ref = weakref.ref(original)
        replacement = np.zeros_like(original)

        try:
            src.data = original
            dst.data = src.data
            src.data = replacement
            del original

            assert original_ref() is not None
            np.testing.assert_array_equal(dst.data, np.arange(4, dtype=np.float64))
        finally:
            dst.data = replacement

    def test_internal_copy_and_rebinding_preserve_old_views(self, single_triangle):
        mesh = single_triangle
        original = np.array([0, 0, 0], dtype=np.intc)
        attr_id = mesh.wrap_as_attribute(
            "index",
            lagrange.AttributeElement.Vertex,
            lagrange.AttributeUsage.Scalar,
            original,
        )
        attr = mesh.attribute(attr_id)
        external_view = attr.data
        attr.create_internal_copy()
        internal_view = attr.data

        external_view[0] = 42
        np.testing.assert_array_equal(original, [42, 0, 0])
        np.testing.assert_array_equal(internal_view, [0, 0, 0])

        attr.data = np.array([4, 5, 6], dtype=np.intc)
        del original
        internal_view[1] = 99
        np.testing.assert_array_equal(external_view, [42, 0, 0])
        np.testing.assert_array_equal(internal_view, [0, 99, 0])
        np.testing.assert_array_equal(attr.data, [4, 5, 6])

    def test_implicit_conversion(self, single_triangle_with_index):
        mesh = single_triangle_with_index
        attr = mesh.attribute("vertex_index")

        attr.data = [1, 2, 3]
        assert np.array_equal(attr.data, [1, 2, 3])

        attr.data = np.array([1, 2, 3], dtype=np.int32)
        assert np.array_equal(attr.data, [1, 2, 3])
        assert attr.external

    def test_delete_attribute(self, single_triangle_with_index):
        mesh = single_triangle_with_index
        attr = mesh.attribute("vertex_index")
        assert attr.num_channels == 1

        mesh.delete_attribute("vertex_index")

        # `attr` is no longer valid.
        with pytest.raises(RuntimeError):
            attr.num_channels == 1

    def test_delete_attribute_with_wrap(self, single_triangle_with_index):
        data = np.array([-1, 1, 0], dtype=np.intc)
        init_refcount = sys.getrefcount(data)
        mesh = single_triangle_with_index
        attr = mesh.attribute("vertex_index")

        attr.data = data
        assert sys.getrefcount(data) == init_refcount + 1

        mesh.delete_attribute("vertex_index")
        assert sys.getrefcount(data) == init_refcount

    def test_read(self, single_triangle_with_index):
        mesh = single_triangle_with_index
        attr = mesh.attribute("vertex_index")

        assert np.all(attr.data == np.array([1, 2, 3], dtype=np.intc))

    def test_write(self, single_triangle_with_index):
        mesh = single_triangle_with_index
        attr = mesh.attribute("vertex_index")
        data = attr.data

        assert data.flags["WRITEABLE"]
        data[0] = 10
        assert attr.data[0] == 10

        attr.data = np.array([3, 2, 1], dtype=np.int32)
        assert np.array_equal(attr.data, [3, 2, 1])

    def test_default_value(self, single_triangle_with_index):
        mesh = single_triangle_with_index
        attr = mesh.attribute("vertex_index")
        assert attr.default_value == 0
        attr.default_value = 1
        assert attr.default_value == 1

        attr.create_internal_copy()
        attr.insert_elements(1)
        assert attr.data[-1] == 1

    def test_insert_elements(self, single_triangle_with_index):
        mesh = single_triangle_with_index
        attr = mesh.attribute("vertex_index")

        attr.create_internal_copy()

        # Insert numpy array
        attr.clear()
        assert attr.empty
        attr.insert_elements(np.array([1, 2, 3], dtype=np.int32))
        assert np.array_equal(attr.data, [1, 2, 3])

        # Insert list
        attr.clear()
        assert attr.empty
        attr.insert_elements([1, 2, 3])
        assert np.array_equal(attr.data, [1, 2, 3])
